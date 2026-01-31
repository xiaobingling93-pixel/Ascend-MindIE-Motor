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
#include "static_alloc_policy.h"
#include "load_balance_policy.h"

namespace MINDIE::MS {

StaticAllocPolicy::StaticAllocPolicy(size_t maxResNum)
{
    maxResNum_ = maxResNum;
    staticPrefillPool_.reserve(maxResNum);
}

void StaticAllocPolicy::LoadResourceView(const std::unique_ptr<ResourceViewManager>& view, DIGSReqStage stage)
{
    switch (stage) {
        case DIGSReqStage::PREFILL:
            staticPrefillPool_.insert(staticPrefillPool_.end(), view->PrefillPool().begin(), view->PrefillPool().end());
            staticPrefillPool_.insert(staticPrefillPool_.end(), view->GlobalPool().begin(), view->GlobalPool().end());
            break;
        case DIGSReqStage::DECODE:
            staticDecodePool_.insert(view->DecodePool().begin(), view->DecodePool().end());
            for (auto& res : view->GlobalPool()) {
                ResourceViewManager::Add2GroupedPool(staticDecodePool_, res.first->StaticInfo().groupId,
                                                     maxResNum_).push_back(res);
            }
            break;
        default:
            LOG_W("[%s] [DIGS] Unsupported stage %d when load resource view",
                  GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
                  static_cast<int32_t>(stage));
            break;
    }
}

int32_t StaticAllocPolicy::SelectPrefillInst(const std::shared_ptr<DIGSRequest>& req,
                                             uint64_t &prefill, uint64_t &groupId)
{
    if (staticPrefillPool_.empty()) {
        return static_cast<int32_t>(common::Status::NO_SATISFIED_RESOURCE);
    }

    LoadBalancePolicy::SortPrefill(staticPrefillPool_);
    auto inst = staticPrefillPool_.begin();
    while (inst != staticPrefillPool_.end() &&
        (inst->first->ScheduleInfo()->IsInstanceClosed() || !ResourceViewManager::CheckConnection(*inst))) {
        inst++;
    }
    if (inst == staticPrefillPool_.end()) {
        return static_cast<int32_t>(common::Status::NO_SATISFIED_RESOURCE);
    }
    inst->first->AddDemand(req->ScheduleInfo()->Demand(), DIGSReqStage::PREFILL);
    prefill = inst->first->StaticInfo().id;
    groupId = inst->first->StaticInfo().groupId;
    return static_cast<int32_t>(common::Status::OK);
}

int32_t StaticAllocPolicy::SelectDecodeInst(const std::shared_ptr<DIGSRequest>& req,
                                            uint64_t prefill, uint64_t groupId, uint64_t &decode)
{
    auto group = staticDecodePool_.find(groupId);
    if (group == staticDecodePool_.end() || group->second.empty()) {
        return static_cast<int32_t>(common::Status::NO_SATISFIED_RESOURCE);
    }
    LoadBalancePolicy::SortDecode(group->second, req->ScheduleInfo()->Demand());

    auto inst = group->second.begin();
    while (inst != group->second.end() &&
           (inst->first->ScheduleInfo()->IsInstanceClosed() ||
            !ResourceViewManager::CheckConnection(*inst, prefill))
        ) {
        inst++;
    }
    if (inst == group->second.end()) {
        return static_cast<int32_t>(common::Status::NO_SATISFIED_RESOURCE);
    }
    (*inst).first->AddDemand(req->ScheduleInfo()->Demand(), DIGSReqStage::DECODE);
    decode = (*inst).first->StaticInfo().id;
    return static_cast<int32_t>(common::Status::OK);
}

void StaticAllocPolicy::OffloadResourceView()
{
    staticPrefillPool_.clear();
    staticDecodePool_.clear();
}
}