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
#ifndef MINDIE_MS_INSTANCE_RECOVERY_TIMER_H
#define MINDIE_MS_INSTANCE_RECOVERY_TIMER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include "Logger.h"

namespace MINDIE::MS {
class InstanceRecoveryTimer {
public:
    InstanceRecoveryTimer() : active_(false), instanceId_(0), isPeriodic_(false) {}

    ~InstanceRecoveryTimer() { Stop(); }
    
    InstanceRecoveryTimer(const InstanceRecoveryTimer &) = delete;
    InstanceRecoveryTimer &operator=(const InstanceRecoveryTimer &) = delete;

    /**
     * Start recovery timer (one-shot)
     * @param timeout Timeout in seconds
     * @param instanceId Associated instance ID
     * @param f Timeout callback function
     * @param args Callback function arguments
     * @return 0 success, -1 failure
     */
    template <typename Function, typename... Args>
    int32_t Start(int32_t timeout, uint64_t instanceId, Function &&f, Args &&...args)
    {
        return StartInternal(timeout, instanceId, false, std::forward<Function>(f), std::forward<Args>(args)...);
    }

    /**
     * Start periodic timer (compatible with ScaleInTimer usage)
     * @param interval Interval in seconds
     * @param f Periodic callback function
     * @param args Callback function arguments
     * @return 0 success, -1 failure
     */
    template <typename Function, typename... Args>
    int32_t Start(int32_t interval, Function &&f, Args &&...args)
    {
        return StartInternal(interval, 0, true, std::forward<Function>(f), std::forward<Args>(args)...);
    }

    /**
     * Stop the timer
     */
    void Stop()
    {
        if (!active_.load()) {
            return; // Already stopped
        }

        // Check if Stop is called from within the worker thread to avoid deadlock
        if (worker_.joinable() && worker_.get_id() == std::this_thread::get_id()) {
            LOG_D("[InstanceRecoveryTimer] Stop called from worker thread, setting flag only");
            active_ = false;
            cv_.notify_all();
            worker_.detach(); // Detach to avoid joining self
            return;
        }

        active_ = false;
        cv_.notify_all();
        
        if (worker_.joinable()) {
            try {
                worker_.join();
                LOG_D("[InstanceRecoveryTimer] Worker thread joined successfully");
            } catch (const std::exception& e) {
                LOG_D("[InstanceRecoveryTimer] Exception when stopping timer: %s", e.what());
            }
        }
        
        // Reset state
        instanceId_ = 0;
        isPeriodic_ = false;
    }
    
    /**
     * Check if the timer is active
     */
    bool IsActive() const
    {
        return active_.load();
    }
    
    /**
     * Get the associated instance ID
     */
    uint64_t GetInstanceId() const
    {
        return instanceId_;
    }

private:
    std::atomic<bool> active_;
    std::atomic<uint64_t> instanceId_; // Associated instance ID
    std::atomic<bool> isPeriodic_; // Whether this is a periodic timer
    std::thread worker_;
    std::mutex mtx_;
    std::condition_variable cv_;

    /**
     * Internal start implementation
     */
    template <typename Function, typename... Args>
    int32_t StartInternal(int32_t timeout, uint64_t instanceId, bool periodic, Function &&f, Args &&...args)
    {
        if (active_.load()) {
            LOG_D("[InstanceRecoveryTimer] Timer is already running!");
            return -1;
        }
        
        if (worker_.joinable()) {
            worker_.join();
        }

        active_ = true;
        instanceId_ = instanceId;
        isPeriodic_ = periodic;
        
        auto boundFunc = std::bind(std::forward<Function>(f), std::forward<Args>(args)...);
        auto interval = std::chrono::seconds(timeout);

        if (periodic) {
            worker_ = std::thread(&InstanceRecoveryTimer::RunTimer, this, interval, boundFunc, timeout, true);
            LOG_I("[InstanceRecoveryTimer] Periodic timer started, interval: %d seconds", timeout);
        } else {
            worker_ = std::thread(&InstanceRecoveryTimer::RunTimer, this, interval, boundFunc, timeout, false);
            LOG_I("[InstanceRecoveryTimer] One-shot timer started, timeout: %d seconds", timeout);
        }
        
        return 0;
    }

    void RunTimer(std::chrono::seconds interval, std::function<void()> boundFunc, int32_t timeout, bool isPeriodic)
    {
        try {
            do {
                if (!WaitForInterval(interval)) {
                    LOG_D("[InstanceRecoveryTimer] Timer stopped normally");
                    break;
                }
                if (!isPeriodic) {
                    LOG_D("[InstanceRecoveryTimer] One-shot timer expired %d seconds, executing callback", timeout);
                }
                boundFunc();
            } while (isPeriodic && active_.load());
        } catch (const std::exception& e) {
            LOG_E("[InstanceRecoveryTimer] Exception in timer: %s", e.what());
        } catch (...) {
            LOG_E("[InstanceRecoveryTimer] Unknown exception in timer.");
        }
    }

    bool WaitForInterval(std::chrono::seconds interval)
    {
        auto startTime = std::chrono::steady_clock::now();
        std::unique_lock<std::mutex> lock(mtx_);
        
        bool stopped = cv_.wait_until(lock, startTime + interval,
            [this]() { return !active_.load(); });
            
        if (!active_.load()) {
            return false;
        }
        
        return !stopped;
    }
};

} // namespace MINDIE::MS

#endif // MINDIE_MS_INSTANCE_RECOVERY_TIMER_H