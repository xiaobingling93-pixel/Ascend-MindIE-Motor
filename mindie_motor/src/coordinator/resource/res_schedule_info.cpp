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
#include "res_schedule_info.h"

namespace MINDIE::MS {

size_t ResScheduleInfo::maxDynamicResRateCount_ = 3;
double ResScheduleInfo::dynamicResRateUnit_ = 0.1;

ResScheduleInfo::ResScheduleInfo(uint64_t resId, DIGSInstanceLabel label) : resId_(resId), label_(label)
{
    prefillDemands_ = std::make_unique<MetaResource>(0);
    decodeDemands_ = std::make_unique<MetaResource>(0);
    LOG_I("[DIGS] Initialize resource schedule information, resource ID: %lu, prefill demands: %s, decode demands: %s",
          resId_,
          MINDIE::MS::common::ToStr(*prefillDemands_).c_str(),
          MINDIE::MS::common::ToStr(*decodeDemands_).c_str());
}

void ResScheduleInfo::AddDemand(const std::unique_ptr<MetaResource>& demand, MINDIE::MS::DIGSReqStage stage)
{
    bool success;
    switch (stage) {
        case DIGSReqStage::PREFILL:
            success = prefillDemands_->IncResource(*demand);
            break;
        case DIGSReqStage::DECODE:
            success = decodeDemands_->IncResource(*demand);
            break;
        default:
            success = false;
            break;
    }
    if (success) {
        LOG_D("[DIGS] Add demand to resource, resource: %lu stage: %d demand: %s prefill: %s decode: %s",
              resId_,
              static_cast<int32_t>(stage),
              MINDIE::MS::common::ToStr(*demand).c_str(),
              MINDIE::MS::common::ToStr(*prefillDemands_).c_str(),
              MINDIE::MS::common::ToStr(*decodeDemands_).c_str());
    } else {
        LOG_W("[%s] [DIGS] Add demand to resource failed, resource: %lu stage: %d demand: %s",
              GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
              resId_,
              static_cast<int32_t>(stage),
              MINDIE::MS::common::ToStr(*demand).c_str());
    }
}

void ResScheduleInfo::RemoveDemand(const std::unique_ptr<MetaResource>& demand, MINDIE::MS::DIGSReqStage stage)
{
    bool success;
    switch (stage) {
        case DIGSReqStage::PREFILL:
            success = prefillDemands_->DecResource(*demand);
            break;
        case DIGSReqStage::DECODE:
            success = decodeDemands_->DecResource(*demand);
            break;
        default:
            success = false;
            break;
    }
    if (success) {
        LOG_D("[DIGS] remove demand from resource, resource: %lu stage: %d demand: %s prefill: %s decode: %s",
              resId_,
              static_cast<int32_t>(stage),
              MINDIE::MS::common::ToStr(*demand).c_str(),
              MINDIE::MS::common::ToStr(*prefillDemands_).c_str(),
              MINDIE::MS::common::ToStr(*decodeDemands_).c_str());
    } else {
        LOG_W("[%s] [DIGS] Remove demand from resource failed, resource: %lu stage: %d demand: %s",
              GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
              resId_,
              static_cast<int32_t>(stage),
              MINDIE::MS::common::ToStr(*demand).c_str());
    }
}

bool ResScheduleInfo::UpdateRole(MINDIE::MS::DIGSInstanceRole newRole)
{
    if (role_ == newRole) {
        return false;
    }
    switch (newRole) {
        case DIGSInstanceRole::PREFILL_INSTANCE:
            if (label_ != DIGSInstanceLabel::PREFILL_STATIC && label_ != DIGSInstanceLabel::PREFILL_PREFER) {
                return false;
            }
            break;
        case DIGSInstanceRole::DECODE_INSTANCE:
            if (label_ != DIGSInstanceLabel::DECODE_STATIC && label_ != DIGSInstanceLabel::DECODE_PREFER) {
                return false;
            }
            break;
        default:
            return false;
    }
    LOG_I("[DIGS] Update instance role, ID: %lu label: %d current role: %c update role: %c",
          resId_,
          static_cast<int32_t>(label_),
          static_cast<char>(role_),
          static_cast<char>(newRole));
    role_ = newRole;
    return true;
}

bool ResScheduleInfo::UpdateDuty(MINDIE::MS::InstanceDuty newDuty)
{
    if (duty_ == newDuty) {
        return false;
    }
    switch (newDuty) {
        case InstanceDuty::PREFILLING:
            if (role_ != DIGSInstanceRole::PREFILL_INSTANCE) {
                return false;
            }
            break;
        case InstanceDuty::DECODING:
            if (role_ != DIGSInstanceRole::DECODE_INSTANCE) {
                return false;
            }
            break;
        case InstanceDuty::MIXING:
            if (label_ != DIGSInstanceLabel::PREFILL_PREFER && label_ != DIGSInstanceLabel::DECODE_PREFER) {
                return false;
            }
            break;
        default:
            return false;
    }
    LOG_I("[DIGS] Update instance duty, id: %lu label: %d current duty: %c update duty: %c",
          resId_,
          static_cast<int32_t>(label_),
          static_cast<char>(duty_),
          static_cast<char>(newDuty));
    duty_ = newDuty;
    return true;
}

int32_t ResScheduleInfo::CloseInstance()
{
    isInstanceClosed_.store(true, std::memory_order_relaxed);
    LOG_I("DIGS: close resource %lu", resId_);
    return static_cast<int32_t>(common::Status::OK);
}

int32_t ResScheduleInfo::ActivateInstance()
{
    isInstanceClosed_.store(false, std::memory_order_relaxed);
    LOG_I("[DIGS] Activate resource %lu", resId_);
    return static_cast<int32_t>(common::Status::OK);
}

void ResScheduleInfo::CheckOverload()
{
    switch (duty_) {
        case InstanceDuty::PREFILLING:
            isOverload_.store(!(*prefillDemands_ < *maxPrefillRes_), std::memory_order_relaxed);
            break;
        case InstanceDuty::DECODING:
            isOverload_.store(!(*decodeDemands_ < *maxDecodeRes_), std::memory_order_relaxed);
            break;
        case InstanceDuty::MIXING:
            isOverload_.store(!(*prefillDemands_ < *maxPrefillRes_) && !(*decodeDemands_ < *maxDecodeRes_),
                              std::memory_order_relaxed);
            break;
        default:
            break;
    }
}

void ResScheduleInfo::CountAllocateUnmatch(bool isResAvailable)
{
    if (isResAvailable && IsOverload()) {
        dynamicResRateCount_++;
    } else if (!isResAvailable && !IsOverload()) {
        dynamicResRateCount_--;
    }
}

bool ResScheduleInfo::GenerateDynamicResRate(double& dynamicResRate)
{
    auto count = dynamicResRateCount_.load(std::memory_order_relaxed);
    if (static_cast<size_t>(std::abs(count)) < maxDynamicResRateCount_) {
        return false;
    }

    if (count > 0) {
        dynamicResRate += dynamicResRateUnit_;
    } else if (count < 0) {
        dynamicResRate -= dynamicResRateUnit_;
    }

    // 小于0时，置0
    if (dynamicResRate < 0) {
        dynamicResRate = 0;
    }
    dynamicResRateCount_.store(0, std::memory_order_relaxed);
    LOG_I("[DIGS] Generate dynamic resource rate, resource ID: %lu, rate: %f", resId_, dynamicResRate);
    return true;
}

uint64_t ResScheduleInfo::TotalConnection()
{
    uint64_t prefillSlots = prefillDemands_->Slots();
    uint64_t decodeSlots = decodeDemands_->Slots();
    if (prefillSlots > std::numeric_limits<uint64_t>::max() - decodeSlots) {
        LOG_W("[ResScheduleInfo] Connection count overflow detected");
        return std::numeric_limits<uint64_t>::max();  // 返回最大值
    }
    // 返回实例上请求总数
    return prefillSlots + decodeSlots;
}

}
