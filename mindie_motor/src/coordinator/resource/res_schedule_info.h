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
#ifndef MINDIE_DIGS_RES_SCHEDULE_INFO_H
#define MINDIE_DIGS_RES_SCHEDULE_INFO_H

#include "digs_scheduler/meta_resource.h"
#include "Logger.h"


namespace MINDIE::MS {

enum class InstanceRole : char {
    UN_DEF_INSTANCE = 'U',
    PREFILL_INSTANCE = 'P',
    DECODE_INSTANCE = 'D',
    FLEX_INSTANCE = 'F',
};
enum class InstanceDuty : char {
    UNKNOWN = 'u',
    MIXING = 'm',
    PREFILLING = 'p',
    DECODING = 'd',
};

class ResScheduleInfo {
public:
    explicit ResScheduleInfo(uint64_t resId, DIGSInstanceLabel label);

    ~ResScheduleInfo() = default;

    uint64_t ResId() const { return resId_; }

    const std::unique_ptr<MetaResource>& PrefillDemands() { return prefillDemands_; }

    const std::unique_ptr<MetaResource>& DecodeDemands() { return decodeDemands_; }

    const std::unique_ptr<MetaResource>& MaxPrefillRes() { return maxPrefillRes_; }

    const std::unique_ptr<MetaResource>& MaxDecodeRes() { return maxDecodeRes_; }

    void SetMaxRes(std::unique_ptr<MetaResource>&& maxPrefillRes, std::unique_ptr<MetaResource>&& maxDecodeRes)
    {
        maxPrefillRes_ = std::move(maxPrefillRes);
        maxDecodeRes_ = std::move(maxDecodeRes);
    }

    DIGSInstanceRole Role() const { return role_; }

    InstanceDuty Duty() const { return duty_; }

    void AddDemand(const std::unique_ptr<MetaResource>& demand, MINDIE::MS::DIGSReqStage stage);

    void RemoveDemand(const std::unique_ptr<MetaResource>& demand, MINDIE::MS::DIGSReqStage stage);

    bool UpdateRole(DIGSInstanceRole newRole);

    bool UpdateDuty(InstanceDuty newDuty);

    int32_t CloseInstance();

    int32_t ActivateInstance();

    bool IsInstanceClosed() const { return isInstanceClosed_.load(std::memory_order_relaxed); };

    bool IsOverload() const { return isOverload_.load(std::memory_order_relaxed); }

    void CheckOverload();

    void CountAllocateUnmatch(bool isResAvailable);

    bool GenerateDynamicResRate(double& dynamicResRate);

    uint64_t TotalConnection();

    static void SetDynamicResRateConfig(size_t maxDynamicResRateCount, double dynamicResRateUnit)
    {
        maxDynamicResRateCount_ = maxDynamicResRateCount;
        dynamicResRateUnit_ = dynamicResRateUnit;
    }

public:
    uint64_t reqQueueTime = 0;
    uint64_t prefillCostReq = common::INVALID_SCORE;
    uint64_t score = common::INVALID_SCORE;

private:
    uint64_t resId_;
    DIGSInstanceLabel label_;
    std::unique_ptr<MetaResource> prefillDemands_;
    std::unique_ptr<MetaResource> decodeDemands_;

    std::unique_ptr<MetaResource> maxPrefillRes_;
    std::unique_ptr<MetaResource> maxDecodeRes_;

    DIGSInstanceRole role_ = DIGSInstanceRole::UN_DEF_INSTANCE;
    InstanceDuty duty_ = InstanceDuty::UNKNOWN;

    // 是否关闭实例服务，默认为开启服务
    std::atomic_bool isInstanceClosed_ = false;

    // 记录实例是否过载（调度层面）
    std::atomic_bool isOverload_ = false;

    std::atomic_int64_t dynamicResRateCount_ = 0;

    // 动态资源上限相关配置
    static size_t maxDynamicResRateCount_;
    static double dynamicResRateUnit_;
};

}
#endif
