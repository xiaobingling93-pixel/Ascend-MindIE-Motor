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
#ifndef MINDIE_DIGS_RESOURCE_MANAGER_H
#define MINDIE_DIGS_RESOURCE_MANAGER_H

#include "resource_view_manager.h"
#include "digs_config.h"
#include "Logger.h"


namespace MINDIE::MS {

class ResourceManager {
public:
    using ResourceMap = std::map<uint64_t, std::pair<std::shared_ptr<ResourceInfo>, std::shared_ptr<ResourceLoad>>>;

    explicit ResourceManager(ResourceConfig& config);

    ~ResourceManager() = default;

    static int32_t Create(
        std::shared_ptr<ResourceManager>& resMgr,
        ResourceConfig& config);

    int32_t RegisterInstance(const std::vector<DIGSInstanceStaticInfo>& instances);

    int32_t UpdateInstance(const std::vector<DIGSInstanceDynamicInfo>& instances);

    int32_t RemoveInstance(const std::vector<uint64_t>& instances);

    int32_t QueryInstanceScheduleInfo(std::vector<DIGSInstanceScheduleInfo>& info);

    int32_t CloseInstance(const std::vector<uint64_t>& instances);

    int32_t ActivateInstance(const std::vector<uint64_t>& instances);

    int32_t UpdateResourceView();

    const std::unique_ptr<ResourceViewManager>& ResourceView() const { return resViewMgr_; }

    size_t MaxResNum() const { return maxResNum_; }

private:
    ResourceMap resourceMap_;

    std::unique_ptr<ResourceViewManager> resViewMgr_;

    std::shared_timed_mutex resMapMutex_;

    std::chrono::milliseconds resViewUpdateTimeout_;

    size_t maxResNum_;

    double resLimitRate_;
};
}
#endif