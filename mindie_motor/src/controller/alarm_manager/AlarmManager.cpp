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
#include "AlarmManager.h"
#include "Logger.h"
#include "ConfigParams.h"
#include "ControllerConfig.h"
#include "SharedMemoryUtils.h"
#include "IPCConfig.h"

namespace MINDIE {
namespace MS {
constexpr size_t MAX_DEQUE_SIZE = 1000;

int32_t AlarmManager::Init(std::shared_ptr<NodeStatus> nodeStatus)
{
    try {
        mAlarmListener = std::make_unique<AlarmListener>();
        StartAlarmManagerThread();
        if (mAlarmListener->Init(nodeStatus) != 0) {
            LOG_W("[%s] [AlarmManager] Initializing alarm listener failed.",
                GetErrorCode(ErrorType::CALL_ERROR, ControllerFeature::CONTROLLER).c_str());
            return -1;
        }
    } catch (const std::exception& e) {
        LOG_E("[AlarmManager] Failed to create thread for alarm manager. Error: %s.", e.what());
        return -1;
    }
    return 0;
}

AlarmManager::~AlarmManager()
{
    Stop();
    LOG_I("[AlarmManager] Alarm manager destroy successfully.");
}

void AlarmManager::Stop()
{
    mRun.store(false);
    mAlarmDequeCV.notify_one(); // 唤醒线程退出等待

    if (mAlarmListener != nullptr) {
        mAlarmListener->Stop();
        mAlarmListener.reset();
    }
    if (mAlarmManagerThread != nullptr && mAlarmManagerThread->joinable()) {
        mAlarmManagerThread->join();
    }
    LOG_I("[AlarmManager] Stop successfully.");
};

void AlarmManager::AlarmAdded(const std::string& alarmMsg)
{
    {
        std::lock_guard<std::mutex> lock(mAlarmDequeMutex);
        if (mAlarmDeque.size() >= MAX_DEQUE_SIZE) {
            LOG_W("[AlarmManager] Queue full.The oldest alarm: %s has been discarded",
                mAlarmDeque.front().c_str());
            mAlarmDeque.pop_front(); // 覆盖最先的告警
        }
        mAlarmDeque.emplace_back(alarmMsg);
    }
    mAlarmDequeCV.notify_one();
}

void AlarmManager::StartAlarmManagerThread()
{
    try {
        mAlarmManagerThread = std::make_unique<std::thread>([this]() {
            LOG_I("[AlarmManager] Start alarm manager thread.");
            while (mRun.load()) {
                std::unique_lock<std::mutex> lock(mAlarmDequeMutex);
                // 条件变量等待：队列非空或线程停止
                mAlarmDequeCV.wait(lock, [this]() {
                    return !mAlarmDeque.empty() || !mRun.load();
                });

                // 处理队列中的所有消息
                while (!mAlarmDeque.empty()) {
                    auto alarmMsg = std::move(mAlarmDeque.front());
                    mAlarmDeque.pop_front();
                    HandleAlarmMsg(alarmMsg);
                }
            }
            LOG_I("[AlarmManager] End alarm manager thread.");
        });
    } catch (const std::exception& e) {
        LOG_E("[AlarmManager] Thread creation failed: %s", e.what());
    }
}

void AlarmManager::HandleAlarmMsg(const std::string& alarmMsg)
{
    if (!WriteAlarmToSHM(alarmMsg)) {
        LOG_E("[%s] [AlarmManager] Failed to write alarm: %s.",
            GetErrorCode(ErrorType::NOT_FOUND, ControllerFeature::SERVER_REQUEST_HANDLER).c_str(),
            alarmMsg.c_str());
    } else {
        LOG_D("[AlarmDeque] Process alarm: %s", alarmMsg.c_str()); // 打印日志
    }
}

bool AlarmManager::WriteAlarmToSHM(const std::string& alarmMsg)
{
    static RetainSharedMemoryUtils shm_writer(ALM_CTRL_SHM_NAME, ALM_CTRL_SEM_NAME, DEFAULT_ALM_BUFFER_SIZE);
    return shm_writer.Write(alarmMsg);
}

}
}