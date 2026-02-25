/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MindIE is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef MINDIE_MS_RESOURCE_MANAGER_H
#define MINDIE_MS_RESOURCE_MANAGER_H

#include <string>
#include <vector>
#include <queue>
#include <unordered_map>
#include "digs_instance.h"
#include "RankTableLoader.h"
namespace MINDIE::MS {

enum class InstanceStatus : int32_t {
    UNKNOWN = 0,
    ACTIVE,
    FAULT
};

struct InstanceData {
    std::vector<uint64_t> instanceLogicIds {};
    std::unordered_map<uint64_t, std::vector<std::string>> instanceTable {};
    std::unordered_map<uint64_t, InstanceStatus> instanceStatus {};
    std::unordered_map<size_t, uint64_t> instanceHash {};

    InstanceData() = default;
};

class ResourceManager {
public:
    static ResourceManager *GetInstance()
    {
        static ResourceManager instance;
        return &instance;
    }
    void Init(std::vector<std::unique_ptr<InstanceInfo>>& instanceInfoList);
    void InitInstanceStatus(std::unordered_map<uint64_t, InstanceStatus>& instanceStatus) const;
    std::vector<uint64_t> GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole role) const;
    std::vector<std::string> GetInstanceAllServerIP(MINDIE::MS::DIGSInstanceRole role, uint64_t instanceLogicId) const;
    InstanceStatus GetInstanceStatus(MINDIE::MS::DIGSInstanceRole role, uint64_t instanceLogicId) const;
    void UpdateInstanceInfo(MINDIE::MS::DIGSInstanceRole role, InstanceData newData);
    void HandleUnmatchedInstanceInfo(std::queue<uint64_t>& targetUnknownLogicIds,
        InstanceData& targetInstanceData, const std::unique_ptr<InstanceInfo>& instanceInfo);
    void UpdateInstanceTable(std::vector<std::unique_ptr<InstanceInfo>>& instanceInfoList);
private:
    ResourceManager() = default;
    ~ResourceManager() = default;
    uint64_t mPInstanceNewID = 0;
    uint64_t mDInstanceNewID = 0;
    InstanceData mPInstanceData {};
    InstanceData mDInstanceData {};
};
}
#endif