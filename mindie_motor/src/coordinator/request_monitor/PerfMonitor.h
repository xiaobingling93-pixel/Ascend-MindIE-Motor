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
#ifndef MINDIE_MS_COORDINATOR_PERF_MONITOR_H
#define MINDIE_MS_COORDINATOR_PERF_MONITOR_H

#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include "WriteDeque.h"

namespace MINDIE::MS {

class ReqAgent;

struct ReqPerf {
    uint64_t scheduler = 0;
    uint64_t repeated = 0;
    uint64_t recvFirstToken = 0;
    uint64_t sendFirstToken = 0;
    uint64_t firstTokenTotal = 0;
    uint64_t recvFirstDToken = 0;
    uint64_t recvLastDToken = 0;
    uint64_t totalInfer = 0;
};

class PerfMonitor {
public:
    PerfMonitor();
    ~PerfMonitor();
    PerfMonitor(const PerfMonitor &obj) = delete;
    PerfMonitor &operator=(const PerfMonitor &obj) = delete;
    PerfMonitor(PerfMonitor &&obj) = delete;
    PerfMonitor &operator=(PerfMonitor &&obj) = delete;
    void Start();
    void Stop();
    void PushReq(std::shared_ptr<ReqAgent> reqInfo);

private:
    std::mutex mtx;
    std::condition_variable cv;
    std::thread t;
    std::atomic<bool> running;
    WriteDeque<std::shared_ptr<ReqAgent>> reqs;
    std::vector<uint64_t> schedule; // 一个周期所有请求从到达到调度完成的时间
    std::vector<uint64_t> repeated; // 一个周期所有请求从调度完成到转发成功的时间
    std::vector<uint64_t> recvFirstToken; // 一个周期所有请求从到达到收到首token的时间
    std::vector<uint64_t> sendFirstToken; // 一个周期所有请求从收到首token到发回给用户的时间
    std::vector<ReqPerf> reqPerfs;
    void OnWork();
    void ShowMetrics();
    void ShowAllReq();
};

}
#endif