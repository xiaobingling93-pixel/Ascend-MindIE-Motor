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
#include "request_manager.h"

namespace MINDIE::MS {

RequestManager::RequestManager(RequestConfig& config)
    : pullRequestTimeout_(config.pullRequestTimeout)
{
    requestProfiler_ = std::make_unique<RequestProfiler>(config);
}

int32_t RequestManager::Create(std::shared_ptr<RequestManager>& reqMgr, RequestConfig& config)
{
    auto requestManager = std::make_shared<RequestManager>(config);
    reqMgr = std::move(requestManager);
    return static_cast<int32_t>(common::Status::OK);
}

int32_t RequestManager::ProcessEndedReq(std::vector<std::shared_ptr<DIGSRequest>>& endedReqs)
{
    auto iter = processingQueue_.begin();
    while (iter != processingQueue_.end()) {
        if ((*iter)->State() == DIGSReqState::DECODE_END) {
            requestProfiler_->ExportEndedReq(*iter);
            endedReqs.push_back(std::move(*iter));
            iter = processingQueue_.erase(iter);
        } else {
            iter++;
        }
    }
    return static_cast<int32_t>(common::Status::OK);
}

int32_t RequestManager::ProcessRelease()
{
    if (releaseProcessor_ == nullptr) {
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }
    for (auto& req : processingQueue_) {
        if (req->ScheduleInfo() == nullptr) {
            continue;
        }

        switch (req->State()) {
            case DIGSReqState::DECODE_END:
                if (req->ScheduleInfo()->DecodeRelease()) {
                    releaseProcessor_(req, DIGSReqStage::DECODE);
                }
                [[fallthrough]];
            case DIGSReqState::PREFILL_END:
                if (req->ScheduleInfo()->PrefillRelease()) {
                    releaseProcessor_(req, DIGSReqStage::PREFILL);
                }
                [[fallthrough]];
            default:
                break;
        }
    }
    return static_cast<int32_t>(common::Status::OK);
}

int32_t RequestManager::PullRequest(size_t maxReqNum, std::deque<std::shared_ptr<DIGSRequest>>& waitingReqs)
{
    size_t count = 0;
    auto size = waitingQueue_.size();
    if (size > maxReqNum) {
        LOG_W("[%s] [DIGS] Number of waiting request more than max limit, waiting queue size is %d, limit is %d",
              GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
              size,
              maxReqNum);
    }
    do {
        std::shared_ptr<DIGSRequest> req;
        {
            std::unique_lock<std::timed_mutex> lock(waitingQueueMutex_, pullRequestTimeout_);
            if (!lock.owns_lock() || waitingQueue_.empty() || count >= maxReqNum) {
                break;
            }
            req = std::move(waitingQueue_.front());
            waitingQueue_.pop_front();
        }

        req->UpdateState2Scheduling();
        waitingReqs.push_back(req);
        processingQueue_.push_back(std::move(req));
        count++;
    } while (true);
    return static_cast<int32_t>(common::Status::OK);
}

int32_t RequestManager::AddReq(std::unique_ptr<DIGSRequest> req)
{
    if (req == nullptr) {
        LOG_E("[%s] [DIGS] Add request failed, input request pointer is null.",
              GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str());
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }
    LOG_I("[DIGS] Add request " << req->ReqId());
    req->SetReleaseCallback(notifyScheduler_);
    DIGSRequest::ControlCallback ctrlCallback;
    req->GetRequestControlCallback(ctrlCallback);
    bool insertFailed;
    {
        std::unique_lock<std::mutex> lock(reqMutex_);
        auto res = reqId2Callback_.insert(std::make_pair(req->ReqId(), ctrlCallback));
        insertFailed = !res.second;
    }

    // 校验请求ID是否重复
    if (insertFailed) {
        LOG_W("[%s] [DIGS] Add request failed, duplicate ID %s",
              GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
              req->ReqId().c_str());
        req->UpdateState2Invalid();
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }

    {
        std::unique_lock<std::timed_mutex> lock(waitingQueueMutex_);
        waitingQueue_.push_back(std::move(req));
    }
    if (notifyScheduler_ != nullptr) {
        notifyScheduler_(false);
    }
    return static_cast<int32_t>(common::Status::OK);
}

int32_t RequestManager::UpdateReq(std::string& reqId, uint64_t prefillEndTime)
{
    std::unique_lock<std::mutex> lock(reqMutex_);
    auto iter = reqId2Callback_.find(reqId);
    if (iter != reqId2Callback_.end()) {
        return iter->second(DIGSReqOperation::REQUEST_UPDATE, prefillEndTime, 0, 0);
    }
    LOG_W("[%s] [DIGS] Update request failed, request not found, request ID: %s",
          GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
          reqId.c_str());
    return static_cast<int32_t>(common::Status::RESOURCE_NOT_FOUND);
}

int32_t RequestManager::RemoveReq(std::string& reqId, uint64_t prefillEndTime,
                                  uint64_t decodeEndTime, size_t outputLength)
{
    std::unique_lock<std::mutex> lock(reqMutex_);
    auto iter = reqId2Callback_.find(reqId);
    if (iter != reqId2Callback_.end()) {
        auto callback = std::move(iter->second);
        reqId2Callback_.erase(iter);
        return callback(DIGSReqOperation::REQUEST_REMOVE, prefillEndTime, decodeEndTime, outputLength);
    }
    LOG_W("[%s] [DIGS] Remove request failed, request not found, request ID: %s",
          GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
          reqId.c_str());
    return static_cast<int32_t>(common::Status::RESOURCE_NOT_FOUND);
}

void RequestManager::GetAvgLength(double& avgInputLength, double& avgOutputLength) const
{
    avgInputLength = requestProfiler_->GetAvgInputLength();
    avgOutputLength = requestProfiler_->GetAvgOutputLength();
}

void RequestManager::CalculateSummary()
{
    requestProfiler_->CalculateSummary();
}
}