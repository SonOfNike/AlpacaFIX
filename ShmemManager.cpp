#include "ShmemManager.h"
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

ShmemManager* ShmemManager::uniqueInstance = nullptr;

ShmemManager* ShmemManager::getInstance(){
    if(uniqueInstance == nullptr){
        uniqueInstance = new ShmemManager();
    }
    return uniqueInstance;
}

void ShmemManager::startUp(){
    int shm_fd;
    size_t shm_size = sizeof(RespShmem);

    // Create or open the shared memory object
    shm_fd = shm_open(RESP_shm_name, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
    }

    // Configure the size of the shared memory object
    if (ftruncate(shm_fd, shm_size) == -1) {
        perror("ftruncate");
    }

    // Map the shared memory object into the process's address space
    resp_shmem = (RespShmem*)mmap(0, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (resp_shmem == MAP_FAILED) {
        perror("mmap");
    }
    close(shm_fd);

    shm_size = sizeof(ReqShmem);

    // Create or open the shared memory object
    shm_fd = shm_open(REQ_shm_name, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
    }

    // Configure the size of the shared memory object
    if (ftruncate(shm_fd, shm_size) == -1) {
        perror("ftruncate");
    }

    // Map the shared memory object into the process's address space
    req_shmem = (ReqShmem*)mmap(0, shm_size, PROT_READ, MAP_SHARED, shm_fd, 0);
    if (req_shmem == MAP_FAILED) {
        perror("mmap");
    }
    close(shm_fd);

    next_req_read_index = req_shmem->next_write_index.load(std::memory_order_acquire);
    next_req_read_page = req_shmem->next_write_page;

    shm_size = sizeof(ErrorShmem);

    // Create or open the shared memory object
    shm_fd = shm_open(ERROR_shm_name, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
    }

    // Configure the size of the shared memory object
    if (ftruncate(shm_fd, shm_size) == -1) {
        perror("ftruncate");
    }

    // Map the shared memory object into the process's address space
    error_shmem = (ErrorShmem*)mmap(0, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (error_shmem == MAP_FAILED) {
        perror("mmap");
    }
    close(shm_fd);
}

void ShmemManager::shutDown(){
    if (munmap(resp_shmem, sizeof(RespShmem)) == -1) {
        perror("munmap");
    }

    if (munmap(req_shmem, sizeof(ReqShmem)) == -1) {
        perror("munmap");
    }

    if (munmap(error_shmem, sizeof(ErrorShmem)) == -1) {
        perror("munmap");
    }
}

void ShmemManager::write_resp(const Response& _response){
    resp_shmem->m_queue[resp_shmem->next_write_index] = _response;
    resp_shmem->next_write_index.fetch_add(1, std::memory_order_release);
    if(resp_shmem->next_write_index >= RESP_QUEUE_SIZE){
        resp_shmem->next_write_index = 0;
        resp_shmem->next_write_page++;
    }
}

bool ShmemManager::gotReq(){
    if(req_shmem->next_write_index.load(std::memory_order_acquire) == next_req_read_index)
        return false;
    return true;
}

void ShmemManager::getReq(Request& newReq){
    newReq = req_shmem->m_queue[next_req_read_index];
    next_req_read_index++;
    if(next_req_read_index >= REQ_QUEUE_SIZE){
        next_req_read_index = 0;
        next_req_read_page++;
    }
}

void ShmemManager::pushError(const Response& newError){
    error_shmem->m_queue[error_shmem->next_write_index] = newError;
    error_shmem->next_write_index.fetch_add(1, std::memory_order_release);
    if(error_shmem->next_write_index >= ERROR_QUEUE_SIZE){
        error_shmem->next_write_index = 0;
        error_shmem->next_write_page++;
    }
}