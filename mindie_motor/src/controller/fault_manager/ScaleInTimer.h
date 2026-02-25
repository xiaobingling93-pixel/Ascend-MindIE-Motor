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
#ifndef MINDIE_MS_SCALE_IN_TIMER_H
#define MINDIE_MS_SCALE_IN_TIMER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include "Logger.h"

namespace MINDIE::MS {

/*
 * This timer is used to juldge whether the cluster have
 * backup node when Decode instance occurde error.
 * If 30s later, the cluster still have no backup node,
 * then we need to release a prefill instance to recover
 * the Decode instance.
 */
class ScaleInTimer {
public:
    ScaleInTimer() : active_(false) {}

    ~ScaleInTimer() { Stop(); }
    ScaleInTimer(const ScaleInTimer &) = delete;
    ScaleInTimer &operator=(const ScaleInTimer &) = delete;

    template <typename Function, typename... Args>
    int32_t Start(int32_t timeout, Function &&f, Args &&...args)
    {
        if (active_.load()) {
            LOG_W("[FaultManager] Timer is already running!");
            return -1;
        }
        if (worker_.joinable()) {
            worker_.join();
        }

        active_ = true;
        // 绑定回调函数及其参数
        auto boundFunc = std::bind(std::forward<Function>(f), std::forward<Args>(args)...);
        auto interval = std::chrono::seconds(timeout);

        worker_ = std::thread([this, interval, boundFunc]() {
            while (active_.load()) {
                auto startTime = std::chrono::steady_clock::now();
                std::unique_lock<std::mutex> lock(mtx_);
                // 等待 interval 时间或者直到接收到停止信号
                if (cv_.wait_until(lock, startTime + interval, [this]() { return !active_.load(); })) {
                    break; // 在等待期间检测到定时器已停止
                }
                lock.unlock();
                LOG_I("[FaultManager] Timer expired after %ld seconds. will do recover strategy. ", interval.count());
                boundFunc(); // 超时后调用回调函数
            }
        });
        LOG_I("[FaultManager] Timer started with timeout: %d seconds.", timeout);
        return 0;
    }

    void Stop()
    {
        active_ = false;
        cv_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    bool IsActive() const { return active_.load(); }

private:
    std::atomic<bool> active_;
    std::thread worker_;
    std::mutex mtx_;
    std::condition_variable cv_;
};
} // namespace MINDIE::MS

#endif