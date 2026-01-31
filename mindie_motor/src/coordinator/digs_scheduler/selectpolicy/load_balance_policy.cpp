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
#include "load_balance_policy.h"

namespace MINDIE::MS {

LoadBalancePolicy::LoadBalancePolicy(size_t maxResNum)
{
    maxResNum_ = maxResNum;
    prefillPool_.reserve(maxResNum_);
}

void LoadBalancePolicy::LoadResourceView(const std::unique_ptr<ResourceViewManager>& view, DIGSReqStage stage)
{
    switch (stage) {
        case DIGSReqStage::PREFILL:
            prefillPool_.insert(prefillPool_.end(), view->PrefillPool().begin(), view->PrefillPool().end());
            prefillPool_.insert(prefillPool_.end(), view->GlobalPool().begin(), view->GlobalPool().end());
            break;
        case DIGSReqStage::DECODE:
            decodePool_.insert(view->DecodePool().begin(), view->DecodePool().end());
            for (auto& res : view->GlobalPool()) {
                ResourceViewManager::Add2GroupedPool(decodePool_, res.first->StaticInfo().groupId,
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

int32_t LoadBalancePolicy::SelectPrefillInst(const std::shared_ptr<DIGSRequest>& req, uint64_t &prefill,
                                             uint64_t &groupId)
{
    if (prefillPool_.empty()) {
        return static_cast<int32_t>(common::Status::NO_SATISFIED_RESOURCE);
    }

    SortPrefill(prefillPool_);
    auto inst = prefillPool_.begin();
    // 可服务与Limit 其一不满足就查看下一个
    while (inst != prefillPool_.end() &&
           (inst->first->ScheduleInfo()->IsInstanceClosed() || !ResourceViewManager::CheckConnection(*inst) ||
            !CheckDemandLimit(*inst, DIGSReqStage::PREFILL))
        ) {
        inst++;
    }
    if (inst == prefillPool_.end()) {
        return static_cast<int32_t>(common::Status::NO_SATISFIED_RESOURCE);
    }
    (*inst).first->AddDemand(req->ScheduleInfo()->Demand(), DIGSReqStage::PREFILL);
    prefill = (*inst).first->StaticInfo().id;
    groupId = (*inst).first->StaticInfo().groupId;
    return static_cast<int32_t>(common::Status::OK);
}

int32_t LoadBalancePolicy::SelectDecodeInst(const std::shared_ptr<DIGSRequest>& req, uint64_t prefill, uint64_t groupId,
                                            uint64_t &decode)
{
    auto group = decodePool_.find(groupId);
    if (group == decodePool_.end() || group->second.empty()) {
        return static_cast<int32_t>(common::Status::NO_SATISFIED_RESOURCE);
    }

    SortDecode(group->second, req->ScheduleInfo()->Demand());
    auto inst = group->second.begin();
    // 连接、可服务与Limit 其一不满足就查看下一个
    while (inst != group->second.end() &&
           (inst->first->ScheduleInfo()->IsInstanceClosed() || !ResourceViewManager::CheckConnection(*inst, prefill) ||
            !CheckDemandLimit(*inst, DIGSReqStage::DECODE))
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

void LoadBalancePolicy::OffloadResourceView()
{
    prefillPool_.clear();
    decodePool_.clear();
}

void LoadBalancePolicy::SortPrefill(ResourceViewManager::ResView& pool)
{
    auto comp = [](const ResourceViewManager::ResInfo& a, const ResourceViewManager::ResInfo& b) {
        return MetaResource::TotalLoad(a.first->ScheduleInfo()->PrefillDemands()) <
               MetaResource::TotalLoad(b.first->ScheduleInfo()->PrefillDemands());
    };
    std::sort(pool.begin(), pool.end(), comp);
}

void LoadBalancePolicy::SortDecode(ResourceViewManager::ResView& group,
                                   const std::unique_ptr<MetaResource>& demand)
{
    // 获取group中最大的slots num
    auto iter = std::max_element(group.begin(), group.end(),
        [](const ResourceViewManager::ResInfo& a, const ResourceViewManager::ResInfo& b) {
            return a.first->ScheduleInfo()->DecodeDemands()->Slots() <
            b.first->ScheduleInfo()->DecodeDemands()->Slots();
        });
    size_t maxSlots = iter->first->ScheduleInfo()->DecodeDemands()->Slots();

    auto comp = [&maxSlots, &demand](const ResourceViewManager::ResInfo& a, const ResourceViewManager::ResInfo& b) {
        size_t aBlocks = a.first->StaticInfo().totalBlockNum - a.second->DynamicInfo().availBlockNum;
        size_t bBlocks = b.first->StaticInfo().totalBlockNum - b.second->DynamicInfo().availBlockNum;
        return MetaResource::ComputeAwareLoad(a.first->ScheduleInfo()->DecodeDemands(), maxSlots, aBlocks, demand) <
                MetaResource::ComputeAwareLoad(b.first->ScheduleInfo()->DecodeDemands(), maxSlots, bBlocks, demand);
    };
    std::sort(group.begin(), group.end(), comp);
}

bool LoadBalancePolicy::CheckDemandLimit(const ResourceViewManager::ResInfo& resInfo, DIGSReqStage stage)
{
    // 分配负载
    auto& prefillDemands = resInfo.first->ScheduleInfo()->PrefillDemands();
    auto& decodeDemands = resInfo.first->ScheduleInfo()->DecodeDemands();
    for (size_t i = 0; i < prefillDemands->Size(); i++) {
        switch (stage) {
            case DIGSReqStage::PREFILL:
                if (prefillDemands->At(i) > resInfo.first->ScheduleInfo()->MaxPrefillRes()->At(i)) {
                    return false;
                }
                break;
            case DIGSReqStage::DECODE:
                if (decodeDemands->At(i) > resInfo.first->ScheduleInfo()->MaxDecodeRes()->At(i)) {
                    return false;
                }
                break;
            default:
                break;
        }
    }
    return true;
}

}