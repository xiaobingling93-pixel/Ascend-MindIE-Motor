/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MindIE is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#pragma once

#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <atomic>
#include <string>
#include <cstddef>
#include "Logger.h"

namespace MINDIE::MS {

struct CircularBufferHeader {
    std::atomic<uint32_t> readIdx;
    std::atomic<uint32_t> writeIdx;

    explicit CircularBufferHeader()
        : readIdx(0), writeIdx(0) {}
};

class SharedMemoryUtils {
public:
    explicit SharedMemoryUtils(const std::string& shmName, const std::string& semName,
    size_t bufferSize = 10 * 1024 * 1024);

    virtual ~SharedMemoryUtils();

    virtual bool Write(const std::string& msg) = 0;
    std::string Read();

    static void ClearResources(const std::string& shmName, const std::string& semName);

protected:
    size_t bufferSize_;
    int8_t* data_; // Pointer to the data region in shared memory
    CircularBufferHeader* cb_;
    sem_t* sem_;

private:
    void InitSharedMemory();
    void Cleanup();
    void CheckPermission();

    std::string shmName_;
    std::string semName_;
    int shmFd_;
    bool isOwner_;
    void* mmapAddr_;
    size_t mmapLength_;
};

class RetainSharedMemoryUtils : public SharedMemoryUtils {
public:
    using SharedMemoryUtils::SharedMemoryUtils;
    bool Write(const std::string& msg) override;
};

class OverwriteSharedMemoryUtils : public SharedMemoryUtils {
public:
    using SharedMemoryUtils::SharedMemoryUtils;
    bool Write(const std::string& msg) override;
};

} // namespace MINDIE::MS