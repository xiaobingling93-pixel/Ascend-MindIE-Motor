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
#include <algorithm>
#include "Configure.h"
#include "Logger.h"
#include "RequestMgr.h"
#include "PerfMonitor.h"

using namespace MINDIE::MS;

PerfMonitor::PerfMonitor() : running(true) {}

PerfMonitor::~PerfMonitor()
{
    Stop();
}

void PerfMonitor::Start()
{
    if (!Configure::Singleton()->metricsConfig.metrics) {
        return;
    }
    t = std::thread([this] {
        while (running) {
            OnWork();
        }
    });
}

void PerfMonitor::Stop()
{
    if (!Configure::Singleton()->metricsConfig.metrics) {
        return;
    }
    running = false;
    cv.notify_all();
    if (t.joinable()) {
        t.join();
    }
}

void PerfMonitor::PushReq(std::shared_ptr<ReqAgent> reqInfo)
{
    if (!Configure::Singleton()->metricsConfig.metrics) {
        return;
    }
    if (reqInfo == nullptr) {
        return; // 请求已经不存在了
    }
    reqs.PushBack(reqInfo);
    cv.notify_one();
}

static bool FirstTokenCompare(const ReqPerf &a, const ReqPerf &b)
{
    return a.firstTokenTotal < b.firstTokenTotal;
}

static bool InferCompare(const ReqPerf &a, const ReqPerf &b)
{
    return a.totalInfer < b.totalInfer;
}

void PerfMonitor::OnWork()
{
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [this] { return !running || !reqs.Empty(); });
    if (!running) {
        return;
    }
    auto reqInfo = reqs.Front();
    reqs.PopFront();
    if (!(reqInfo->HasState(ReqState::ARRIVE) && reqInfo->HasState(ReqState::SCHEDULED) &&
        reqInfo->HasState(ReqState::RECV_TOKENS_FROM_INS))) {
        return; // 合法性校验不通过，跳过该请求的性能统计
    }
    ReqPerf reqPerf;
    auto &arriveTime = reqInfo->GetStateTime(ReqState::ARRIVE);
    auto &scheduleTime = reqInfo->GetStateTime(ReqState::SCHEDULED);
    auto &reqeatedTime = reqInfo->GetStateTime(ReqState::REPEATED);
    schedule.emplace_back(scheduleTime[0] - arriveTime[0]);
    repeated.emplace_back(reqeatedTime[0] - scheduleTime[0]);
    auto &recvTokens = reqInfo->GetStateTime(ReqState::RECV_TOKENS_FROM_INS);
    recvFirstToken.emplace_back(recvTokens[0] - reqeatedTime[0]);
    if (reqInfo->HasState(ReqState::SEND_TOKENS_TO_USER)) {
        auto &sendTokens = reqInfo->GetStateTime(ReqState::SEND_TOKENS_TO_USER);
        sendFirstToken.emplace_back(sendTokens[0] - recvTokens[0]);
        reqPerf.sendFirstToken = sendTokens[0] - recvTokens[0];
        reqPerf.firstTokenTotal = sendTokens[0] - arriveTime[0];
    }
    reqPerf.scheduler = scheduleTime[0] - arriveTime[0];
    reqPerf.repeated = reqeatedTime[0] - scheduleTime[0];
    reqPerf.recvFirstToken = recvTokens[0] - reqeatedTime[0];
    if (recvTokens.size() > 1) {
        reqPerf.recvFirstDToken = recvTokens[1] - reqeatedTime[0];
    }
    reqPerf.recvLastDToken = recvTokens.back() - reqeatedTime[0];
    reqPerf.totalInfer = recvTokens.back() - arriveTime[0];
    reqPerfs.emplace_back(reqPerf);
    if (schedule.size() >= Configure::Singleton()->metricsConfig.triggerSize) {
        ShowMetrics();
    }
}

void PerfMonitor::ShowMetrics()
{
    auto scheduleMax = std::max_element(schedule.begin(), schedule.end());
    auto scheduleMin = std::min_element(schedule.begin(), schedule.end());
    uint64_t sum = std::accumulate(schedule.begin(), schedule.end(), static_cast<uint64_t>(0));
    uint64_t scheduleMean = schedule.size() > 0 ? sum / schedule.size() : 0;
    auto repeatedMax = std::max_element(repeated.begin(), repeated.end());
    auto repeatedMin = std::min_element(repeated.begin(), repeated.end());
    sum = std::accumulate(repeated.begin(), repeated.end(), static_cast<uint64_t>(0));
    uint64_t repeatedMean = repeated.size() > 0 ? sum / repeated.size() : 0;
    auto firstTokenMax = std::max_element(recvFirstToken.begin(), recvFirstToken.end());
    auto firstTokenMin = std::min_element(recvFirstToken.begin(), recvFirstToken.end());
    sum = std::accumulate(recvFirstToken.begin(), recvFirstToken.end(), static_cast<uint64_t>(0));
    auto firstTokenMean = recvFirstToken.size() > 0 ? sum / recvFirstToken.size() : 0;
    LOG_P("[metrics] Performance Report:");
    LOG_P("[metrics] Summary of %u requests.", schedule.size());
    LOG_P("[metrics] Schedule max: %luns (from request arrival to schedule finish)", *scheduleMax);
    LOG_P("[metrics] Schedule min: %luns (from request arrival to schedule finish)", *scheduleMin);
    LOG_P("[metrics] Schedule mean: %luns (from request arrival to schedule finish)", scheduleMean);
    LOG_P("[metrics] Repeated max: %luns (from schedule finish to mindie-server)", *repeatedMax);
    LOG_P("[metrics] Repeated min: %luns (from schedule finish to mindie-server)", *repeatedMin);
    LOG_P("[metrics] Repeated mean: %luns (from schedule finish to mindie-server)", repeatedMean);
    LOG_P("[metrics] Receive first token max: %luns (from request arrival to first token arrival)", *firstTokenMax);
    LOG_P("[metrics] Receive first token min: %luns (from request arrival to first token arrival)", *firstTokenMin);
    LOG_P("[metrics] Receive first token mean: %luns (from request arrival to first token arrival)", firstTokenMean);
    if (!sendFirstToken.empty()) {
        auto sendFirstTokenMax = std::max_element(sendFirstToken.begin(), sendFirstToken.end());
        auto sendFirstTokenMin = std::min_element(sendFirstToken.begin(), sendFirstToken.end());
        sum = std::accumulate(sendFirstToken.begin(), sendFirstToken.end(), static_cast<uint64_t>(0));
        auto sendFirstTokenMean = sum / sendFirstToken.size();
        LOG_P("[metrics] Send first token max: %luns (from first token arrival to sending to user)",
            *sendFirstTokenMax);
        LOG_P("[metrics] Send first token min: %luns (from first token arrival to sending to user)",
            *sendFirstTokenMin);
        LOG_P("[metrics] Send first token mean: %luns (from first token arrival to sending to user)",
            sendFirstTokenMean);
    }
    ShowAllReq();
    schedule.clear();
    repeated.clear();
    recvFirstToken.clear();
    sendFirstToken.clear();
}

void PerfMonitor::ShowAllReq()
{
    LOG_P("[metrics] first token perf:");
    std::sort(reqPerfs.begin(), reqPerfs.end(), FirstTokenCompare);
    for (auto &perf : reqPerfs) {
        LOG_P("Scheduler: %luns, Repeated: %luns, First Token Received: %luns, First Token Sent: %luns,"
            " Total First Token: %luns",
            perf.scheduler, perf.repeated, perf.recvFirstToken, perf.sendFirstToken, perf.firstTokenTotal);
    }
    LOG_P("[metrics] infer perf:");
    std::sort(reqPerfs.begin(), reqPerfs.end(), InferCompare);
    for (auto &perf : reqPerfs) {
        LOG_P("Scheduler: %luns, Repeated: %luns, First Token Received: %luns, First Token Sent: %luns, "
            "First D Token Received: %luns, Last D Token Received: %luns, Total Inference Time: %luns",
            perf.scheduler, perf.repeated, perf.recvFirstToken, perf.sendFirstToken,
            perf.recvFirstDToken, perf.recvLastDToken, perf.totalInfer);
    }
    reqPerfs.clear();
}