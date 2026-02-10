#pragma once

#include "../Utils/RespShmem.h"
#include "../Utils/Response.h"
#include "../Utils/ReqShmem.h"
#include "../Utils/ErrorShmem.h"
#include "../Utils/Request.h"
#include "../Utils/Response.h"

class ShmemManager {
private:
    RespShmem* resp_shmem = nullptr;
    ReqShmem* req_shmem = nullptr;
    int32_t next_req_read_index = 0;
    int32_t next_req_read_page = 0;

    ErrorShmem* error_shmem = nullptr;

    static ShmemManager* uniqueInstance;
    ShmemManager(){;}

public:
    static ShmemManager* getInstance();
    void startUp();
    void shutDown();
    void write_resp(const Response& _response);
    bool gotReq();
    void getReq(Request& newReq);
    void pushError(const Response& newError);
};