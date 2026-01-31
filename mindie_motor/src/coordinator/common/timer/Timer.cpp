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
#include "Timer.h"

using namespace MINDIE::MS;

Timer::~Timer()
{
    Stop();
}

void Timer::Start()
{
    running = true;
    t = std::thread([this] {
        while (running) {
            OnWork();
            std::this_thread::sleep_for(std::chrono::milliseconds(5)); // 睡眠5毫秒
        }
    });
}

void Timer::Stop()
{
    running = false;
    if (t.joinable()) {
        t.join();
    }
}

void Timer::AsyncWait(size_t ms, TimerFun fun)
{
    TimerInner inner;
    inner.startTime = GetTimeNow();
    inner.waitTime = ms;
    inner.fun = fun;
    timerQueue.PushBack(inner);
}

void Timer::OnWork()
{
    if (timerQueue.Empty()) {
        return;
    }
    auto &inner = timerQueue.Front();
    auto now = GetTimeNow();
    if ((now - inner.startTime) >= inner.waitTime) {
        inner.fun();
        timerQueue.PopFront();
    }
}

uint64_t Timer::GetTimeNow() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}