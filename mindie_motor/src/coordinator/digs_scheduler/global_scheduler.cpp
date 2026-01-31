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
#include "global_scheduler.h"

namespace MINDIE::MS {

int32_t GlobalScheduler::Create(
    SchedulerConfig& config, std::shared_ptr<ResourceManager>& resourceManager,
    std::shared_ptr<RequestManager>& requestManager, std::unique_ptr<GlobalScheduler> &globalSched)
{
    auto sched = std::make_unique<GlobalScheduler>(config, resourceManager, requestManager);

    globalSched = std::move(sched);
    return static_cast<int32_t>(common::Status::OK);
}

GlobalScheduler::GlobalScheduler(
    SchedulerConfig& config, std::shared_ptr<ResourceManager>& resourceManager,
    std::shared_ptr<RequestManager>& requestManager)
    : schedulerTerminate_(true),  resourceManager_(resourceManager), requestManager_(requestManager),
      maxScheduleCount_(config.maxScheduleCount), requestUpdated_(false)
{
    LOG_I("[DIGS] Global scheduler initizing.");
    scheduleFramework_ = std::make_unique<ScheduleFramework>(config, resourceManager_->MaxResNum());

    allocatedReqs_.reserve(maxScheduleCount_);
    requestManager->SetReleaseProcessor(
        [this](const std::shared_ptr<DIGSRequest>& req, MINDIE::MS::DIGSReqStage stage) {
            return resourceManager_->ResourceView()->UpdateScheduleInfo(req, stage);
        });
    requestManager->SetNotifyScheduler([this](bool release) {
        if (release) {
            requestUpdated_.store(release, std::memory_order_relaxed);
        }
        schedulerCv_.notify_one();
    });
}

void GlobalScheduler::Start()
{
    bool start = true;
    if (schedulerTerminate_.compare_exchange_strong(start, false)) {
        schedulerThread_ = std::thread([this]() {
            this->SchedulerThread();
        });
        notifyThread_ = std::thread([this]() {
            this->NotifyOutsideThread();
        });
    }
    LOG_I("[DIGS] Global scheduler start(%d)...", start);
}

void GlobalScheduler::Stop()
{
    LOG_I("[DIGS] Global scheduler stopping...");
    // set thread signal
    schedulerTerminate_.store(true);

    // notify threads
    schedulerCv_.notify_one();
    auto emptyReq = std::make_shared<DIGSRequest>();
    notifyQueue_.Push(std::move(emptyReq));

    // wait for all threads exit
    if (schedulerThread_.joinable()) {
        schedulerThread_.join();
    }
    if (notifyThread_.joinable()) {
        notifyThread_.join();
    }
    LOG_I("[DIGS] Global scheduler stopped!");
}

void GlobalScheduler::ProcessAllocatedRequests()
{
    for (auto it = allocatedReqs_.begin(); it != allocatedReqs_.end();) {
        if (*it) {
            notifyQueue_.Push(std::move(*it)); // 移动并移除
            it = allocatedReqs_.erase(it);
        } else {
            LOG_W("[DIGS] Found null request in allocatedReqs_");
            ++it;
        }
    }
}

void GlobalScheduler::SchedulerThread()
{
    LOG_I("[DIGS] Global scheduler thread start!");
    const uint64_t waitUs = 100 * 1000;
    while (!schedulerTerminate_.load()) {
        std::unique_lock<std::mutex> lock(schedulerMutex_);
        do {
            // 1.1 Get requests from waiting queue
            requestManager_->PullRequest(maxScheduleCount_, schedulingReqs_);
            if (schedulingReqs_.empty() && !requestUpdated_.load(std::memory_order_relaxed)) {
                break;
            }
            requestUpdated_.store(false, std::memory_order_relaxed);

            // 1.2 prepare resource view
            resourceManager_->UpdateResourceView();
            auto& resourceView = resourceManager_->ResourceView();
            if (!resourceView || resourceView->Empty()) {
                LOG_E("[DIGS] ResourceView is null!");
                break;
            }
            requestManager_->ProcessRelease();

            // 2 scheduling
            scheduleFramework_->Scheduling(resourceManager_->ResourceView(), schedulingReqs_, allocatedReqs_);

            // 3 prepare calling outside
            ProcessAllocatedRequests();
            
            // 4 process ended requests
            std::vector<std::shared_ptr<DIGSRequest>> endedReqs;
            requestManager_->ProcessEndedReq(endedReqs);
            this->ReleaseAllocation(endedReqs);
            resourceManager_->ResourceView()->ClearView();
        } while (false);

        std::chrono::microseconds timeout(waitUs);
        schedulerCv_.wait_for(lock, timeout);
    }
    LOG_I("[DIGS] Global scheduler thread end!");
}

void GlobalScheduler::NotifyOutsideThread()
{
    LOG_I("[DIGS] Notify outside thread start!");
    while (!schedulerTerminate_.load()) {
        auto req = notifyQueue_.Take();
        switch (req->State()) {
            case DIGSReqState::WAITING:
            case DIGSReqState::SCHEDULING:
                LOG_W("[%s] [DIGS] Request notify skip, request ID is %s request state is %d",
                      GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
                      req->ReqId().c_str(),
                      static_cast<int32_t>(req->State()));
                // 将未完成调度的请求放回调度队列中
                schedulingReqs_.push_back(std::move(req));
                [[fallthrough]];
            case DIGSReqState::INVALID:
                continue;
            case DIGSReqState::ALLOCATED:
                break;
            default:
                LOG_W("[%s] [DIGS] Request notify repetitively, request ID is %s request state is %d",
                      GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
                      req->ReqId().c_str(),
                      static_cast<int32_t>(req->State()));
                break;
        }

        int32_t status = static_cast<int32_t>(common::Status::OK);

        if (req->ScheduleInfo() != nullptr) {
            LOG_I("[DIGS] req %s allocated, input len:%d, p: %d, d: %d",
                  req->ReqId().c_str(),
                  req->InputLen(),
                  req->ScheduleInfo()->PrefillInst(),
                  req->ScheduleInfo()->DecodeInst());
        }
        if (callback_ != nullptr) {
            status = callback_(req->ScheduleInfo());
        } else {
            LOG_E("[%s] [DIGS] Request notify error, callback is null!",
                  GetErrorCode(ErrorType::NOT_FOUND, CommonFeature::DIGS).c_str());
        }
        if (status != static_cast<int32_t>(common::Status::OK)) {
            LOG_W("[%s] [DIGS] Request notify failed, request %s reset to scheduling",
                  GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
                  req->ReqId().c_str());
            // 进入重调度
            if (req->UpdateState2Scheduling() == static_cast<int32_t>(common::Status::OK)) {
                schedulingReqs_.push_front(std::move(req));
            }
        }
    }
    LOG_I("DIGS: notify outside thread end!");
}

void GlobalScheduler::RegisterNotifyAllocation(MINDIE::MS::GlobalScheduler::NotifyAllocation callback)
{
    std::unique_lock<std::mutex> lock(schedulerMutex_);
    callback_ = std::move(callback);
    // try start
    this->Start();
}

void GlobalScheduler::ReleaseAllocation(std::vector<std::shared_ptr<DIGSRequest>>& endedReqs)
{
    for (auto& req : endedReqs) {
        if (req->ScheduleInfo() == nullptr) {
            continue;
        }

        if (req->ScheduleInfo()->PrefillRelease()) {
            resourceManager_->ResourceView()->UpdateScheduleInfo(req, DIGSReqStage::PREFILL);
        }

        if (req->ScheduleInfo()->DecodeRelease()) {
            resourceManager_->ResourceView()->UpdateScheduleInfo(req, DIGSReqStage::DECODE);
        }
    }
}

}