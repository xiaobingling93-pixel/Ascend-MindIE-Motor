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
#ifndef MINDIE_DIGS_REQUEST_MANAGER_H
#define MINDIE_DIGS_REQUEST_MANAGER_H

#include "common.h"
#include "request_profiler.h"
#include "Logger.h"


namespace MINDIE::MS {

class RequestManager {
public:
    using ReleaseProcessor = std::function<
        int32_t(const std::shared_ptr<DIGSRequest>& req, MINDIE::MS::DIGSReqStage stage)
    >;

    using NotifyScheduler = std::function<void(bool)>;

    explicit RequestManager(RequestConfig& config);

    ~RequestManager() = default;

    static int32_t Create(std::shared_ptr<RequestManager>& reqMgr, RequestConfig& config);

    int32_t AddReq(std::unique_ptr<DIGSRequest> req);

    int32_t UpdateReq(std::string& reqId, uint64_t prefillEndTime);

    int32_t RemoveReq(std::string& reqId, uint64_t prefillEndTime, uint64_t decodeEndTime, size_t outputLength);

    int32_t PullRequest(size_t maxReqNum, std::deque<std::shared_ptr<DIGSRequest>>& waitingReqs);

    int32_t ProcessEndedReq(std::vector<std::shared_ptr<DIGSRequest>>& endedReqs);

    int32_t ProcessRelease();

    void SetReleaseProcessor(ReleaseProcessor processor) { releaseProcessor_ = std::move(processor); }

    void SetNotifyScheduler(NotifyScheduler notifyScheduler) { notifyScheduler_ = std::move(notifyScheduler); }

    void GetAvgLength(double& avgInputLength, double& avgOutputLength) const;

    void CalculateSummary();

private:
    std::deque<std::unique_ptr<DIGSRequest>> waitingQueue_;

    std::timed_mutex waitingQueueMutex_;

    std::chrono::milliseconds pullRequestTimeout_;

    std::vector<std::shared_ptr<DIGSRequest>> processingQueue_;

    std::map<std::string, DIGSRequest::ControlCallback> reqId2Callback_;

    std::mutex reqMutex_;

    std::unique_ptr<RequestProfiler> requestProfiler_;

    ReleaseProcessor releaseProcessor_ = nullptr;

    NotifyScheduler notifyScheduler_ = nullptr;
};
}
#endif
