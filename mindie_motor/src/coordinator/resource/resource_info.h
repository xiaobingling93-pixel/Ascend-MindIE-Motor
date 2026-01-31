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
#ifndef MINDIE_DIGS_RESOURCE_INFO_H
#define MINDIE_DIGS_RESOURCE_INFO_H

#include "res_schedule_info.h"
#include "Logger.h"
#include "common.h"


namespace MINDIE::MS {

class ResourceInfo {
public:
    explicit ResourceInfo(DIGSInstanceStaticInfo insInfo, double maxResRate);

    ~ResourceInfo() = default;

    const std::shared_ptr<ResScheduleInfo>& ScheduleInfo() { return scheduleInfo_; }

    const DIGSInstanceStaticInfo& StaticInfo() { return instanceStaticInfo_; }

    void AddDemand(const std::unique_ptr<MetaResource>& demand, DIGSReqStage stage);

    bool UpdateScheduleLoad(bool isResAvailable);

    bool ReviseMaxResource();

    static void SetDynamicMaxResEnable(bool enable)
    {
        isDynamicMaxResEnabled_ = enable;
    }

    void UpdateStaticInfo(const DIGSInstanceDynamicInfo& dynaInfo);

private:
    std::shared_ptr<ResScheduleInfo> scheduleInfo_;
    DIGSInstanceStaticInfo instanceStaticInfo_;

    std::unique_ptr<MetaResource> totalResource_;

    double maxResRate_;

    double dynamicResRate_;

    static bool isDynamicMaxResEnabled_;
};

}
#endif
