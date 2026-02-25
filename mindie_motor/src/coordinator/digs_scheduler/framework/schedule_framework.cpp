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
#include "schedule_framework.h"

namespace MINDIE::MS {

ScheduleFramework::ScheduleFramework(MINDIE::MS::SchedulerConfig& config, size_t maxResNum)
    : maxResNum_(maxResNum)
{
    InitReorderingPolicy(config);
    InitPoolPolicy(config);
    InitSelectPolicy(config);
}

void ScheduleFramework::Scheduling(const std::unique_ptr<ResourceViewManager>& resView,
                                   std::deque<std::shared_ptr<DIGSRequest>>& schedulingReqs,
                                   std::vector<std::shared_ptr<DIGSRequest>>& allocatedReqs)
{
    // 2.1 reordering requests
    reorderingPolicy_->Reordering(schedulingReqs);

    // 2.2 instance schedule
    poolPolicy_->ScheduleInst(resView);

    // 2.3 schedule prefill instance for all request
    selectPolicy_->LoadResourceView(resView, DIGSReqStage::PREFILL);
    this->SchedulePrefill(resView, schedulingReqs, allocatedReqs);
    selectPolicy_->OffloadResourceView();

    // 2.4 instance schedule
    poolPolicy_->ScheduleInst(resView);

    // 2.5 schedule decode instance for all request
    selectPolicy_->LoadResourceView(resView, DIGSReqStage::DECODE);
    this->ScheduleDecode(resView, schedulingReqs, allocatedReqs);
    selectPolicy_->OffloadResourceView();
}

void ScheduleFramework::InitReorderingPolicy(MINDIE::MS::SchedulerConfig& config)
{
    LOG_I("[DIGS] Reordering policy initialization, type is %d", config.reorderingType);
    switch (config.reorderingType) {
        case static_cast<int32_t>(ReorderingPolicyType::FCFS):
            reorderingPolicy_ = std::make_unique<FcfsPolicy>();
            break;
        case static_cast<int32_t>(ReorderingPolicyType::SJF):
            reorderingPolicy_ = std::make_unique<SljfPolicy>(false);
            break;
        case static_cast<int32_t>(ReorderingPolicyType::LJF):
            reorderingPolicy_ = std::make_unique<SljfPolicy>(true);
            break;
        case static_cast<int32_t>(ReorderingPolicyType::MPRF):
            reorderingPolicy_ = std::make_unique<MprfPolicy>();
            break;
        default:
            LOG_W("[%s] [DIGS] Unsupported reordering type %d, user default type 1",
                  GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
                  config.reorderingType);
            reorderingPolicy_ = std::make_unique<FcfsPolicy>();
    }
}

void ScheduleFramework::InitPoolPolicy(MINDIE::MS::SchedulerConfig& config)
{
    LOG_I("[DIGS] Pool policy initialize, type is %d", config.poolType);
    switch (config.poolType) {
        case static_cast<int32_t>(InstPoolPolicyType::STATIC):
            poolPolicy_ = std::make_unique<StaticPoolPolicy>();
            break;
        default:
            LOG_W("[%s] [DIGS] Unsupported pool type %d, user default type 1",
                  GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
                  config.poolType);
            poolPolicy_ = std::make_unique<StaticPoolPolicy>();
    }
}

void ScheduleFramework::InitSelectPolicy(MINDIE::MS::SchedulerConfig& config)
{
    LOG_I("[DIGS] Select policy initialize, type is %d", config.selectType);
    switch (config.selectType) {
        case static_cast<int32_t>(InstSelectPolicyType::STATIC_ALLOC):
            selectPolicy_ = std::make_unique<StaticAllocPolicy>(maxResNum_);
            break;
        case static_cast<int32_t>(InstSelectPolicyType::LOAD_BALANCE):
            selectPolicy_ = std::make_unique<LoadBalancePolicy>(maxResNum_);
            break;
        default:
            LOG_W("[%s] [DIGS] Unsupported select type %d, user default type 1",
                  GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
                  config.selectType);
            selectPolicy_ = std::make_unique<StaticAllocPolicy>(maxResNum_);
    }
}

void ScheduleFramework::SchedulePrefill(const std::unique_ptr<ResourceViewManager>& resView,
                                        std::deque<std::shared_ptr<DIGSRequest>>& schedulingReqs,
                                        std::vector<std::shared_ptr<DIGSRequest>>& allocatedReqs)
{
    while (!schedulingReqs.empty()) {
        std::unique_ptr<MetaResource> demand;
        uint64_t prefill = 0;
        uint64_t groupId = 0;  // 加入初始化， 防止静态分配时候报错
        auto& req = schedulingReqs.front();
        if (req->State() != DIGSReqState::SCHEDULING) {
            schedulingReqs.pop_front();
            continue;
        }

        if (req->ScheduleInfo() != nullptr) {
            // 清理之前的调度资源
            if (req->ScheduleInfo()->PrefillRelease()) {
                resView->UpdateScheduleInfo(req, DIGSReqStage::PREFILL);
            }

            if (req->ScheduleInfo()->DecodeRelease()) {
                resView->UpdateScheduleInfo(req, DIGSReqStage::DECODE);
            }
        }

        this->GenerateDemand(req, demand);

        req->InitScheduleInfo(std::move(demand));
        if (selectPolicy_->SelectPrefillInst(req, prefill, groupId) == static_cast<int32_t>(common::Status::OK)) {
            req->ScheduleInfo()->SetPrefillInst(prefill);
            req->ScheduleInfo()->SetGroupId(groupId);
            allocatedReqs.push_back(std::move(schedulingReqs.front()));
            schedulingReqs.pop_front();
        } else {
            LOG_D("[DIGS] Select prefill instance end, no instance match, request ID is " << req->ReqId());
            break;
        }
    }
}

void ScheduleFramework::ScheduleDecode(const std::unique_ptr<ResourceViewManager>& resView,
                                       std::deque<std::shared_ptr<DIGSRequest>>& schedulingReqs,
                                       std::vector<std::shared_ptr<DIGSRequest>>& allocatedReqs)
{
    for (auto it = allocatedReqs.begin(); it != allocatedReqs.end();) {
        auto& req = *it;
        uint64_t decode = common::ILLEGAL_INSTANCE_ID;

        if (req->ScheduleInfo()->PrefillInst() == common::ILLEGAL_INSTANCE_ID ||
                selectPolicy_->SelectDecodeInst(req, req->ScheduleInfo()->PrefillInst(),
                                                req->ScheduleInfo()->GroupId(), decode) ==
                                                static_cast<int32_t>(common::Status::OK)) {
            req->ScheduleInfo()->SetDecodeInst(decode);
            req->UpdateState2Allocated();
            it++;
        } else {
            // D实例选择时调度所有请求，尽可能将请求分发下去
            LOG_D("[DIGS] Select decode instance end, no instance match, request ID is " << req->ReqId());
            if (req->ScheduleInfo()->PrefillRelease()) {
                resView->UpdateScheduleInfo(req, DIGSReqStage::PREFILL);
            }
            schedulingReqs.push_back(std::move(req));
            it = allocatedReqs.erase(it);
        }
    }
}

void ScheduleFramework::GenerateDemand(const std::shared_ptr<DIGSRequest>& req,
                                       std::unique_ptr<MetaResource> &demand) const
{
    auto defaultDemand = std::make_unique<MetaResource>();
    if (defaultDemand->Blocks() == 0) {
        defaultDemand->UpdateBlocks(common::BlockNum(req->InputLen(), common::GetBlockSize()));
    }
    defaultDemand->UpdateTokens(req->InputLen());
    LOG_I("[DIGS] Generate demand request ID %s, demand: %s",
          req->ReqId().c_str(),
          MINDIE::MS::common::ToStr(*defaultDemand).c_str());
    demand = std::move(defaultDemand);
}

}