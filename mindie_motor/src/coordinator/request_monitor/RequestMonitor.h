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
#ifndef MINDIE_MS_COORDINATOR_TIMEOUT_MONITOR_H
#define MINDIE_MS_COORDINATOR_TIMEOUT_MONITOR_H

#include <thread>
#include <atomic>
#include <condition_variable>
#include "RequestMgr.h"
#include "ExceptionMonitor.h"

namespace MINDIE::MS {

class RequestMonitor {
public:
    RequestMonitor(std::unique_ptr<ReqManage>& reqManageInit, std::unique_ptr<ExceptionMonitor> &exceptionMonitorInit);
    ~RequestMonitor();
    RequestMonitor(const RequestMonitor &obj) = delete;
    RequestMonitor &operator=(const RequestMonitor &obj) = delete;
    RequestMonitor(RequestMonitor &&obj) = delete;
    RequestMonitor &operator=(RequestMonitor &&obj) = delete;
    void Start();
    void Stop();

private:
    std::unique_ptr<ReqManage>& reqManage;
    std::unique_ptr<ExceptionMonitor> &exceptionMonitor;
    std::thread t;
    std::atomic<bool> running;
    void OnWork();
    bool HandleFirstTokenTimeout(const std::pair<const std::string, std::shared_ptr<ReqAgent>>& reqPair);
    bool InferTimeOut(std::shared_ptr<ReqAgent> reqInfo) const;
    bool FirstTokenTimeOut(std::shared_ptr<ReqAgent> reqInfo) const;
    bool ScheduleTimeOut(std::shared_ptr<ReqAgent> reqInfo) const;
    bool TokenizerTimeout(std::shared_ptr<ReqAgent> reqInfo) const;
    uint64_t GetTimeNow() const;
    int32_t countNow = 0;
    int32_t countPre = 0;
};

}
#endif