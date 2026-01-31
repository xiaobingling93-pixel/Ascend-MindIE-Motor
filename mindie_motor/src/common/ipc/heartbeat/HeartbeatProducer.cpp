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
#include <sstream>
#include "SharedMemoryUtils.h"
#include "HeartbeatProducer.h"


using namespace MINDIE::MS;

HeartbeatProducer::HeartbeatProducer(
    std::chrono::milliseconds interval,
    const std::string& shmName,
    const std::string& semName,
    size_t bufferSize)
    : m_interval(interval),
      m_shmName(shmName),
      m_semName(semName) {
    try {
        m_shmUtils = std::make_unique<MINDIE::MS::OverwriteSharedMemoryUtils>(shmName, semName, bufferSize);
        LOG_I("[HeartbeatProducer] Initialized SharedMemoryUtils with shm=%s, sem=%s",
              shmName.c_str(), semName.c_str());
    } catch (const std::runtime_error& e) {
        LOG_E("[HeartbeatProducer] Failed to initialize SharedMemoryUtils: %s", e.what());
        throw;
    }
}

HeartbeatProducer::~HeartbeatProducer()
{
    Stop();
    LOG_I("[HeartbeatProducer] Destroyed.");
}

void HeartbeatProducer::Start()
{
    if (m_isRunning.exchange(true)) {
        LOG_W("[HeartbeatProducer] Already running.");
        return;
    }
    if (!m_shmUtils) {
        LOG_E("[HeartbeatProducer] Cannot start, SharedMemoryUtils not initialized.");
        m_isRunning.store(false);
        return;
    }
    m_producerThread = std::thread(&HeartbeatProducer::Run, this);
    LOG_I("[HeartbeatProducer] Started with shm=%s, sem=%s.",
          m_shmName.c_str(), m_semName.c_str());
}

void HeartbeatProducer::Stop()
{
    if (!m_isRunning.exchange(false)) {
        // Already stopped or not started
        return;
    }
    if (m_producerThread.joinable()) {
        m_producerThread.join();
    }
    LOG_I("[HeartbeatProducer] Stopped.");
}

void HeartbeatProducer::Run()
{
    while (m_isRunning.load()) {
        m_sequenceNumber++;
        long long currentTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        std::string hbMessage;
        try {
            hbMessage = CreateHeartbeatMessage(m_sequenceNumber, currentTimeMs);
        } catch (const std::exception& e) {
            std::string msg = "[HeartbeatProducer] Failed to create heartbeat message:" + std::string(e.what());
            LOG_E(msg);
            throw;
        } catch (...) {
            std::string msg = "[HeartbeatProducer] Failed to create heartbeat message: seq=" +
                     std::to_string(m_sequenceNumber) + ", timestamp=" +
                     std::to_string(currentTimeMs);
            LOG_E(msg);
            throw;
        }
        if (m_shmUtils->Write(hbMessage)) {
            LOG_D("[HeartbeatProducer] Sent heartbeat to %s: %s",
                  m_shmName.c_str(), hbMessage.c_str());
        } else {
            LOG_W("[HeartbeatProducer] Failed to send heartbeat to %s: %s",
                  m_shmName.c_str(), hbMessage.c_str());
        }
        std::this_thread::sleep_for(m_interval);
    }
}

std::string HeartbeatProducer::CreateHeartbeatMessage(uint64_t sequenceNumber, long long timestamp) const
{
    std::ostringstream oss;
    oss << "{\"seq\":" << sequenceNumber << ",\"timestamp\":" << timestamp << "}";
    return oss.str();
}
