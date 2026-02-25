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
#include "resource_view_manager.h"

namespace MINDIE::MS {

ResourceViewManager::ResourceViewManager(size_t maxResNum) : maxResNum_(maxResNum)
{
    view_.reserve(maxResNum);
    prefillPool_.reserve(maxResNum);
    globalPool_.reserve(maxResNum);
}

void ResourceViewManager::ClearView()
{
    for (auto& resInfo : view_) {
        resInfo.first->ScheduleInfo()->CheckOverload();
    }
    view_.clear();
    prefillPool_.clear();
    decodePool_.clear();
    globalPool_.clear();
    scheduleInfos_.clear();
}

bool ResourceViewManager::Empty() const
{
    return view_.empty();
}

int32_t ResourceViewManager::AddResInfo(const MINDIE::MS::ResourceViewManager::ResInfo& resInfo)
{
    view_.push_back(resInfo);
    resInfo.first->ReviseMaxResource();
    switch (resInfo.first->StaticInfo().label) {
        case DIGSInstanceLabel::PREFILL_STATIC:
            resInfo.first->ScheduleInfo()->UpdateDuty(InstanceDuty::PREFILLING);
            prefillPool_.push_back(resInfo);
            break;
        case DIGSInstanceLabel::DECODE_STATIC:
            resInfo.first->ScheduleInfo()->UpdateDuty(InstanceDuty::DECODING);
            Add2GroupedPool(decodePool_, resInfo.first->StaticInfo().groupId, maxResNum_).push_back(resInfo);
            break;
        case DIGSInstanceLabel::PREFILL_PREFER:
        case DIGSInstanceLabel::DECODE_PREFER:
            LOG_I("[DIGS] Add instance to global pool, instance ID is %lu", resInfo.first->StaticInfo().id);
            globalPool_.push_back(resInfo);
            break;
        default:
            LOG_W("[%s] [DIGS] Unsupported instance label, ID is %lu label is %d",
                  GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
                  resInfo.first->StaticInfo().id,
                  static_cast<int32_t>(resInfo.first->StaticInfo().label));
            break;
    }
    scheduleInfos_.insert(std::make_pair(resInfo.first->ScheduleInfo()->ResId(), resInfo.first->ScheduleInfo()));

    return static_cast<int32_t>(common::Status::OK);
}

int32_t ResourceViewManager::UpdateScheduleInfo(const std::shared_ptr<DIGSRequest>& req, MINDIE::MS::DIGSReqStage stage)
{
    uint64_t instId = common::ILLEGAL_INSTANCE_ID;
    switch (stage) {
        case DIGSReqStage::PREFILL:
            instId = req->ScheduleInfo()->PrefillInst();
            break;
        case DIGSReqStage::DECODE:
            instId = req->ScheduleInfo()->DecodeInst();
            break;
        default:
            LOG_W("[%s] [DIGS] Unsupported request stage.",
                  GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str());
            break;
    }
    if (instId == common::ILLEGAL_INSTANCE_ID) {
        // 未分配实例，跳过资源释放
        return static_cast<int32_t>(common::Status::OK);
    }
    LOG_I("[DIGS] Update schedule information for instance, instance ID is %lu request ID is %s stage: %d",
          instId,
          req->ReqId().c_str(),
          static_cast<int32_t>(stage));
    auto iter = scheduleInfos_.find(instId);
    if (iter != scheduleInfos_.end()) {
        iter->second->RemoveDemand(req->ScheduleInfo()->Demand(), stage);
        if (stage == DIGSReqStage::PREFILL) {
            iter->second->reqQueueTime -= req->PrefillCostTime();
        }
        return static_cast<int32_t>(common::Status::OK);
    }
    LOG_W("[%s] [DIGS] Update schedule information failed, instance ID: %lu stage: %d demand: %s",
          GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
          instId,
          static_cast<int32_t>(stage),
          MINDIE::MS::common::ToStr(*req->ScheduleInfo()->Demand()).c_str());
    return static_cast<int32_t>(common::Status::RESOURCE_NOT_FOUND);
}

ResourceViewManager::ResView& ResourceViewManager::Add2GroupedPool(std::map<uint64_t, ResView>& pool,
                                                                   uint64_t groupId, size_t maxResNum)
{
    auto& res = pool[groupId];
    if (res.empty()) {
        res.reserve(maxResNum);
    }
    return res;
}

bool ResourceViewManager::CheckConnection(const ResourceViewManager::ResInfo& resInfo, uint64_t otherResId)
{
    if (!CheckConnection(resInfo)) {
        return false;
    }
    if (resInfo.first->StaticInfo().id == otherResId) {
        return true;
    }
    // 检测与prefill是否连通
    return std::any_of(resInfo.second->DynamicInfo().peers.begin(),
                       resInfo.second->DynamicInfo().peers.end(),
                       [&otherResId](uint64_t peer) { return peer == otherResId; });
}

bool ResourceViewManager::CheckConnection(const ResourceViewManager::ResInfo& resInfo)
{
    // 检查最大链接数
    return resInfo.first->StaticInfo().maxConnectionNum > resInfo.first->ScheduleInfo()->TotalConnection();
}

int32_t ResourceViewManager::Add2PrefillPool(MINDIE::MS::ResourceViewManager::ResInfo resInfo)
{
    prefillPool_.push_back(std::move(resInfo));
    return static_cast<int32_t>(common::Status::OK);
}

int32_t ResourceViewManager::Add2DecodePool(MINDIE::MS::ResourceViewManager::ResInfo resInfo, uint64_t groupId)
{
    decodePool_[groupId].push_back(std::move(resInfo));
    return static_cast<int32_t>(common::Status::OK);
}

ptrdiff_t ResourceViewManager::GlobalPoolRemoveAndNext(ptrdiff_t offset, ResInfo& resInfo)
{
    if (offset < 0 || offset >= static_cast<ptrdiff_t>(globalPool_.size())) {
        LOG_W("[%s] [DIGS] Offset is invalid when remove global pool.",
              GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str());
        return globalPool_.end() - globalPool_.begin();
    }
    auto iter = globalPool_.begin() + offset;
    resInfo = std::move(*iter);
    auto next = globalPool_.erase(iter);
    return next - globalPool_.begin();
}

}
