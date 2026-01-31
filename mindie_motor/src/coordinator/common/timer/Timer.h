/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 * MindIE is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef MINDIE_MS_COORDINATOR_TIMER_H
#define MINDIE_MS_COORDINATOR_TIMER_H

#include <thread>
#include <functional>
#include <atomic>
#include "WriteDeque.h"

namespace MINDIE {
namespace MS {

using TimerFun = std::function<void()>; // 定时器的回调函数

// 简易定时器
class Timer {
public:
    Timer() = default;
    Timer(Timer&& other) = delete;
    Timer(const Timer& other) = delete;
    Timer& operator=(const Timer& other) = delete;
    Timer& operator=(Timer&& other) = delete;
    ~Timer();
    void Start();
    void Stop();
    void AsyncWait(size_t ms, TimerFun fun);

private:
    struct TimerInner {
        uint64_t startTime;
        uint64_t waitTime;
        TimerFun fun;
    };
    std::thread t;
    std::atomic<bool> running;
    WriteDeque<TimerInner> timerQueue;
    void OnWork();
    uint64_t GetTimeNow() const;
};

}
}
#endif