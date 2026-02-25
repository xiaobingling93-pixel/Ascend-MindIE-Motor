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

#ifndef MINDIE_MS_CAMPAIGN_H
#define MINDIE_MS_CAMPAIGN_H

#include <memory>
#include <string>
#include <atomic>
#include "ConfigParams.h"
#include "DistributedPolicy.h"
#include "Logger.h"

namespace MINDIE::MS {

class LeaderAgent {
public:
    LeaderAgent() = default;
    ~LeaderAgent();
    
    // 核心接口
    void Start();
    void Stop();
    int RegisterStrategy(std::unique_ptr<DistributedLockPolicy> lock_strategy);
    
    // 数据操作接口
    bool WriteNodes(const std::string& value);
    bool ReadNodes(nlohmann::json& value);

    // 改为虚函数，允许子类重写
    virtual void Master2Slave() = 0;
    virtual void Slave2Master() = 0;
    virtual void Slave2MasterEvent() = 0;

    // 增加Key写入
    bool WriteFaultsValue(const std::string& value);
    bool ReadFaultsValue(nlohmann::json& value);

#ifdef UT_FLAG
    bool IsLeader();
#endif // UT_FLAG

private:
    void Campaign();
    void PromoteToLeader();
    void DemoteToFollower();

    std::unique_ptr<DistributedLockPolicy> lock_strategy_;
    std::atomic<bool> isLeader{false};
    std::atomic<bool> running_{true};
    std::atomic<bool> mHasSetRole{false};
    const std::string keyForNodeData{"/controller/node-data"};
    const std::string keyForSwitchFaults{"/controller/switch-faults"}; // switch-faults节点
};

} // namespace MINDIE::MS

#endif // MINDIE_MS_CAMPAIGN_H