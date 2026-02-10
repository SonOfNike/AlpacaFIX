#pragma once

#include "ShmemManager.h"
#include "../Utils/Request.h"
#include "../Utils/Response.h"
#include "../Utils/enums_typedef.h"
#include "../Utils/SymbolIDManager.h"
#include <vector>

#include <quickfix/Application.h>
#include <quickfix/MessageCracker.h>
#include <quickfix/Session.h>
#include <quickfix/fix42/NewOrderSingle.h>
#include <quickfix/fix42/OrderCancelRequest.h>
#include <quickfix/fix42/ExecutionReport.h>
#include <quickfix/fix42/OrderCancelReject.h>
#include <atomic>
#include <thread>

struct OrderInfo{
    SymbolId _sym_id;
    side _side;
};

class ReqRespEngine : public FIX::Application, public FIX::MessageCracker{
    // singletons
    ShmemManager* mShmemManager;
    SymbolIDManager* mSymIDManager;

    Request    currentReq;
    Response   currentResponse;

    std::unordered_map<OrderId, OrderInfo> orderid_to_alpaca_id;

    FIX::SessionID m_sessionID;

public:
    ReqRespEngine(){;}
    ~ReqRespEngine(){;}
    void startUp();
    void shutDown();
    void run();
    void processReq();

    // Atomic flag to ensure we don't send orders before logon
    std::atomic<bool> is_logged_on{false};

    void onCreate(const FIX::SessionID& sessionID) override {}
    void onLogon(const FIX::SessionID& sessionID) override {
        is_logged_on = true;
        m_sessionID = sessionID;
    }
    void onLogout(const FIX::SessionID& sessionID) override {}

    void toAdmin(FIX::Message& msg, const FIX::SessionID&) override {
        // Here you can inject passwords/tags into the Logon message (35=A)
        FIX::MsgType msgType;
        msg.getHeader().getField(msgType);
        if (msgType == FIX::MsgType_Logon) {
            // Tag 554 is the standard for Password in FIX 4.2/4.4
            msg.setField(FIX::Password("YOUR_SECRET_KEY"));
        }
    }

    void toApp(FIX::Message& msg, const FIX::SessionID&) noexcept(false) override {
        // Called before an order is sent to the exchange
    }

    void fromAdmin(const FIX::Message& msg, const FIX::SessionID&) noexcept(false) override {
        // Administrative messages from the exchange (Heartbeats, etc.)
    }
    // Callback Thread (Internal QuickFIX Thread)
    void fromApp(const FIX::Message& msg, const FIX::SessionID& sID) override {
        crack(msg, sID); 
    }

    void onMessage(const FIX42::ExecutionReport& report, const FIX::SessionID&) override {
        // This runs on the Network Thread. Keep it fast!
        FIX::ExecType execType;
        FIX::LastPx lastPx;
        FIX::LastShares lastQty;
        FIX::ClOrdID myID;
        FIX::OrigClOrdID cancelID;

        report.get(execType);
        report.get(myID);

        switch(execType) {
            case FIX::ExecType_NEW:
                currentResponse.clear();
                currentResponse.m_type = resp_type::NEWORDER_CONFIRM;
                currentResponse.m_symbolId = orderid_to_alpaca_id[std::stoi(myID.getString().c_str())]._sym_id;
                currentResponse.m_order_id = std::stoi(myID.getString().c_str());
                mShmemManager->write_resp(currentResponse);
                break;
            case FIX::ExecType_FILL:
            case FIX::ExecType_PARTIAL_FILL:
                report.get(lastPx);
                report.get(lastQty);
                currentResponse.clear();
                currentResponse.m_type = resp_type::TRADE_CONFIRM;
                currentResponse.m_symbolId = orderid_to_alpaca_id[std::stoi(myID.getString().c_str())]._sym_id;
                currentResponse.m_order_id = std::stoi(myID.getString().c_str());
                currentResponse.m_resp_price = Price(lastPx * DOLLAR);
                currentResponse.m_resp_quant = lastQty;
                currentResponse.m_side = orderid_to_alpaca_id[std::stoi(myID.getString().c_str())]._side;
                mShmemManager->write_resp(currentResponse);
                break;
            case FIX::ExecType_CANCELED:
                report.get(cancelID);
                currentResponse.clear();
                currentResponse.m_type = resp_type::CANCEL_CONFIRM;
                currentResponse.m_symbolId = orderid_to_alpaca_id[std::stoi(cancelID.getString().c_str())]._sym_id;
                currentResponse.m_order_id = std::stoi(cancelID.getString().c_str());
                mShmemManager->write_resp(currentResponse);
                break;
            case FIX::ExecType_REJECTED:
                currentResponse.clear();
                currentResponse.m_type = resp_type::ORDER_REJECT;
                currentResponse.m_symbolId = orderid_to_alpaca_id[std::stoi(myID.getString().c_str())]._sym_id;
                currentResponse.m_order_id = std::stoi(myID.getString().c_str());
                mShmemManager->write_resp(currentResponse);
                break;
        }
    }

    void onMessage(const FIX42::OrderCancelReject& reject, const FIX::SessionID&) override {
        // This fires if you sent a cancel (35=F) but the order couldn't be killed.
        // Common reason: The order was already filled before the cancel arrived.
        FIX::CxlRejReason reason;
        FIX::OrigClOrdID cancelID;

        reject.get(cancelID);

        currentResponse.clear();
        currentResponse.m_type = resp_type::CANCEL_REJECT;
        currentResponse.m_symbolId = orderid_to_alpaca_id[std::stoi(cancelID.getString().c_str())]._sym_id;
        currentResponse.m_order_id = std::stoi(cancelID.getString().c_str());

        mShmemManager->write_resp(currentResponse);
    }

    // Strategy Thread Helper (Called from Main)
    void sendLimitOrder(const std::string& symbol, double price, int qty, OrderId _order_id, SymbolId _sym_id, side _side) {
        if (!is_logged_on) return;

        FIX42::NewOrderSingle order(
            FIX::ClOrdID(std::to_string(_order_id)),
            FIX::HandlInst('1'),
            FIX::Symbol(symbol),
            FIX::Side(_side == side::BUY?FIX::Side_BUY:FIX::Side_SELL),
            FIX::TransactTime(),
            FIX::OrdType(FIX::OrdType_LIMIT)
        );

        order.set(FIX::OrderQty(qty));
        order.set(FIX::Price(price));

        // This is a non-blocking call that pushes the order to the network buffer
        FIX::Session::sendToTarget(order, m_sessionID);

        OrderInfo new_info;
        new_info._sym_id = _sym_id;
        new_info._side = _side;

        orderid_to_alpaca_id[_order_id] = new_info;
    }

    void cancelOrder(const std::string& origClOrdID, const std::string& symbol, char side, int qty) {
        if (!is_logged_on) return;

        // 1. Create the Cancel Request object (MsgType 35=F)
        FIX42::OrderCancelRequest cancel(
            FIX::OrigClOrdID(origClOrdID),           // The ID of the order you want to cancel
            FIX::ClOrdID("Cxl_" + std::to_string(std::time(0))), // A NEW unique ID for this cancel msg
            FIX::Symbol(symbol),
            FIX::Side(side),
            FIX::TransactTime()
        );

        // 2. Specify the quantity being cancelled (usually the full amount)
        cancel.set(FIX::OrderQty(qty));

        // 3. Send it out over the wire
        FIX::Session::sendToTarget(cancel, m_sessionID);
    }
};