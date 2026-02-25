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
#ifndef MINDIE_MS_ALARM_MANAGER_H
#define MINDIE_MS_ALARM_MANAGER_H
#include <deque>
#include <memory>
#include <cstdint>
#include <mutex>
#include <thread>
#include <condition_variable>
#include "AlarmListener.h"
#include "NodeStatus.h"
namespace MINDIE {
namespace MS {
/// The server of the controller.
class AlarmManager {
public:
    static AlarmManager *GetInstance()
    {
        static AlarmManager instance;
        return &instance;
    }
    /// Initialize the server.
    /// return The result of the initialization. 0 indicates success. -1 indicates failure.
    int32_t Init(std::shared_ptr<NodeStatus> nodeStatus);

    /// Run the server.
    /// return The result of the running. 0 indicates success. -1 indicates failure.
    int32_t Run();

    /// Stop server and threads.
    void Stop();

    void AlarmAdded(const std::string& alarmMsg);
    void StartAlarmManagerThread();
    void HandleAlarmMsg(const std::string& alarmMsg);
    virtual bool WriteAlarmToSHM(const std::string& alarmMsg);
private:
    AlarmManager(const AlarmManager &obj) = delete;
    AlarmManager &operator=(const AlarmManager &obj) = delete;
    AlarmManager(AlarmManager &&obj) = delete;
    AlarmManager &operator=(AlarmManager &&obj) = delete;
    AlarmManager() = default;

    /// Destructor.
    /// This destructor call the Stop method, which stops the server and the thread.
    ~AlarmManager();

    std::deque<std::string> mAlarmDeque = {};
    std::mutex mAlarmDequeMutex {};
    std::condition_variable mAlarmDequeCV {};
    std::atomic<bool> mRun = { true };                   /// The run status of the HTTP server.
    std::unique_ptr<AlarmListener> mAlarmListener = nullptr;
    std::unique_ptr<std::thread> mAlarmManagerThread = nullptr;  /// The thread that executes the Run method.
};
}
}
#endif // MINDIE_MS_ALARM_MANAGER_H
