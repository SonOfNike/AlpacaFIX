#include <quickfix/Application.h>
#include <quickfix/SocketInitiator.h>
#include <quickfix/SessionSettings.h>
#include <quickfix/FileStore.h>
#include <quickfix/FileLog.h>

#include <iostream>
#include "ReqRespEngine.h"

ReqRespEngine       gReqRespEngine;

int main() {
    try {
        FIX::SessionSettings settings("alpaca.cfg");
        gReqRespEngine.startUp();
        FIX::FileStoreFactory storeFactory(settings);
        FIX::ScreenLogFactory logFactory(settings);
        
        // Initiator manages the connection
        FIX::SocketInitiator initiator(gReqRespEngine, storeFactory, settings, logFactory);
        
        initiator.start();
        gReqRespEngine.run();
        gReqRespEngine.shutDown();
        // Keep the main thread alive or do other work
        initiator.stop();
    } catch (FIX::ConfigError& e) {
        std::cerr << e.what() << std::endl;
    }
}