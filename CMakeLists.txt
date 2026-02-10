cmake_minimum_required(VERSION 3.10.0)
project(AlpacaFIX VERSION 0.1.0 LANGUAGES C CXX)

find_package(OpenSSL REQUIRED)
find_package(glog REQUIRED PATHS ../glog/build/)
find_library(QUICKFIX_LIB quickfix)
include_directories(/usr/local/include)
set(QUICKFIX_SHARED_LIBS OFF)

add_executable(AlpacaFIX 
    main.cpp
    ShmemManager.cpp
    ReqRespEngine.cpp
    ../Utils/SymbolIDManager.cpp
    ../Utils/simdjson/simdjson.cpp)

target_link_libraries(AlpacaFIX PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(AlpacaFIX PRIVATE glog::glog)
target_link_libraries(AlpacaFIX PRIVATE ${QUICKFIX_LIB})
target_include_directories(AlpacaFIX PUBLIC
        ../rapidjson/include
    )

include(CTest)
enable_testing()

set_property(TARGET AlpacaFIX PROPERTY CXX_STANDARD 17)