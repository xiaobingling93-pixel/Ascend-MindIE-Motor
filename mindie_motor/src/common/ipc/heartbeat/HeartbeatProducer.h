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
#ifndef MINDIE_HEARTBEAT_PRODUCER_H
#define MINDIE_HEARTBEAT_PRODUCER_H


#include <string>
#include <atomic>
#include <thread>
#include <chrono>
#include <memory>
#include "Logger.h"
#include "SharedMemoryUtils.h"
#include "IPCConfig.h"

namespace MINDIE::MS {
class HeartbeatProducer {
public:
    explicit HeartbeatProducer(
        std::chrono::milliseconds interval = std::chrono::milliseconds(1000),
        const std::string& shmName = HB_ADAPTER_SHM_NAME,
        const std::string& semName = HB_ADAPTER_SEM_NAME,
        size_t bufferSize = DEFAULT_HB_BUFFER_SIZE);  // 添加默认缓冲区大小参数);
    ~HeartbeatProducer();

    void Start();
    void Stop();

private:
    void Run();
    std::string CreateHeartbeatMessage(uint64_t sequenceNumber, long long timestamp) const;

    std::atomic<bool> m_isRunning{false};
    std::thread m_producerThread;
    std::chrono::milliseconds m_interval;
    uint64_t m_sequenceNumber{0};
    std::unique_ptr<MINDIE::MS::OverwriteSharedMemoryUtils> m_shmUtils;  // 使用OverwriteSharedMemoryUtils
    std::string m_shmName;  // 存储共享内存名称
    std::string m_semName;  // 存储信号量名称
};

}

#endif
