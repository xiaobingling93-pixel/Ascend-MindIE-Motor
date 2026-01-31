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
#ifndef MINDIE_DIGS_SCHEDULE_FRAMEWORK_H
#define MINDIE_DIGS_SCHEDULE_FRAMEWORK_H

#include "digs_config.h"
#include "digs_scheduler/poolpolicy/inst_pool_policy.h"
#include "digs_scheduler/poolpolicy/static_pool_policy.h"
#include "digs_scheduler/selectpolicy/inst_select_policy.h"
#include "digs_scheduler/selectpolicy/load_balance_policy.h"
#include "digs_scheduler/selectpolicy/static_alloc_policy.h"
#include "digs_scheduler/reorderingpolicy/fcfs_policy.h"
#include "digs_scheduler/reorderingpolicy/mprf_policy.h"
#include "digs_scheduler/reorderingpolicy/req_reordering_policy.h"
#include "digs_scheduler/reorderingpolicy/sljf_policy.h"
#include "Logger.h"
#include "common.h"


namespace MINDIE::MS {

class ScheduleFramework {
public:
    explicit ScheduleFramework(SchedulerConfig& config, size_t maxResNum);

    virtual void Scheduling(const std::unique_ptr<ResourceViewManager>& resView,
                            std::deque<std::shared_ptr<DIGSRequest>>& schedulingReqs,
                            std::vector<std::shared_ptr<DIGSRequest>>& allocatedReqs);

    virtual ~ScheduleFramework() = default;

private:
    void InitReorderingPolicy(MINDIE::MS::SchedulerConfig& config);

    void InitPoolPolicy(MINDIE::MS::SchedulerConfig& config);

    void InitSelectPolicy(MINDIE::MS::SchedulerConfig& config);

    void SchedulePrefill(const std::unique_ptr<ResourceViewManager>& resView,
                         std::deque<std::shared_ptr<DIGSRequest>>& schedulingReqs,
                         std::vector<std::shared_ptr<DIGSRequest>>& allocatedReqs);

    void ScheduleDecode(const std::unique_ptr<ResourceViewManager>& resView,
                        std::deque<std::shared_ptr<DIGSRequest>>& schedulingReqs,
                        std::vector<std::shared_ptr<DIGSRequest>>& allocatedReqs);

    void GenerateDemand(const std::shared_ptr<DIGSRequest>& req, std::unique_ptr<MetaResource> &demand) const;

private:
    std::unique_ptr<InstPoolPolicy> poolPolicy_;
    std::unique_ptr<InstSelectPolicy> selectPolicy_;
    std::unique_ptr<ReqReorderingPolicy> reorderingPolicy_;

    size_t maxResNum_ = 5000;
};

}
#endif
