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
#ifndef MIDNIE_DIGS_GLOBAL_SCHEDULER_H
#define MIDNIE_DIGS_GLOBAL_SCHEDULER_H

#include "blocking_queue.h"
#include "digs_config.h"
#include "digs_scheduler/poolpolicy/inst_pool_policy.h"
#include "digs_scheduler/selectpolicy/inst_select_policy.h"
#include "digs_scheduler/reorderingpolicy/req_reordering_policy.h"
#include "digs_scheduler/framework/schedule_framework.h"
#include "request/req_schedule_info.h"
#include "request/request_manager.h"
#include "resource/resource_manager.h"
#include "Logger.h"


namespace MINDIE::MS {

class GlobalScheduler {
public:
    using NotifyAllocation = std::function<int32_t(const std::unique_ptr<ReqScheduleInfo>&)>;

    explicit GlobalScheduler(
        MINDIE::MS::SchedulerConfig& config, std::shared_ptr<ResourceManager>& resourceManager,
        std::shared_ptr<RequestManager>& requestManager);

    ~GlobalScheduler() = default;

    static int32_t Create(
        SchedulerConfig& config,
        std::shared_ptr<ResourceManager>& resourceManager, std::shared_ptr<RequestManager>& requestManager,
        std::unique_ptr<GlobalScheduler> &globalSched);

    void RegisterNotifyAllocation(NotifyAllocation callback);

    void Start();

    void Stop();

private:
    void ProcessAllocatedRequests();

    void SchedulerThread();

    void NotifyOutsideThread();

    void ReleaseAllocation(std::vector<std::shared_ptr<DIGSRequest>>& endedReqs);

private:
    std::atomic<bool> schedulerTerminate_;
    std::mutex schedulerMutex_;
    std::condition_variable schedulerCv_;
    std::thread schedulerThread_;

    std::thread notifyThread_;
    BlockingQueue<std::shared_ptr<DIGSRequest>> notifyQueue_;

    std::shared_ptr<ResourceManager> resourceManager_;
    std::shared_ptr<RequestManager> requestManager_;

    std::unique_ptr<ScheduleFramework> scheduleFramework_;

    NotifyAllocation callback_ = nullptr;

    size_t maxScheduleCount_ = 10000;
    std::deque<std::shared_ptr<DIGSRequest>> schedulingReqs_;
    std::vector<std::shared_ptr<DIGSRequest>> allocatedReqs_;

    // 记录是否有请求状态更新（Prefill end与Decode end）
    std::atomic_bool requestUpdated_;
};

}
#endif
