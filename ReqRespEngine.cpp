#include "ReqRespEngine.h"
#include <iostream>
#include "glog/logging.h"

void ReqRespEngine::startUp(){
    mSymIDManager = SymbolIDManager::getInstance();
    mShmemManager = ShmemManager::getInstance();


    mSymIDManager->startUp();
    mShmemManager->startUp();
}

void ReqRespEngine::shutDown(){
    mShmemManager->shutDown();
    mSymIDManager->shutDown();

    delete mShmemManager;
    delete mSymIDManager;
}

void ReqRespEngine::run(){
    while(true){
        if(mShmemManager->gotReq()){
            processReq();
        }
    }
}

void ReqRespEngine::processReq(){
    mShmemManager->getReq(currentReq);

    if(currentReq.m_type == req_type::NEWORDER){
        sendLimitOrder(mSymIDManager->getTicker(currentReq.m_symbolId), 
                        double(currentReq.m_req_price / CENTS) / 100.0, 
                        currentReq.m_order_quant,
                        currentReq.m_order_id,
                        currentReq.m_symbolId,
                        currentReq.m_order_side);

    }
    else if(currentReq.m_type == req_type::CANCEL){
        if(orderid_to_alpaca_id.find(currentReq.m_order_id) != orderid_to_alpaca_id.end()){
            cancelOrder(std::to_string(currentReq.m_order_id), 
                        mSymIDManager->getTicker(currentReq.m_symbolId), 
                        currentReq.m_order_side == side::BUY?FIX::Side_BUY:FIX::Side_SELL,
                        currentReq.m_order_quant);
        }
    }
}