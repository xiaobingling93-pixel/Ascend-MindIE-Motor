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
#include "CoordinatorLeaderAgent.h"
#include "LeaderAgent.h"
#include "Configure.h"

namespace MINDIE::MS {

CoordinatorLeaderAgent::~CoordinatorLeaderAgent() {}

void CoordinatorLeaderAgent::Master2Slave()
{
    if (!Configure::Singleton()->CheckBackup()) { // 若不开主备功能不用走这里的断链逻辑
        return;
    }
    Configure::Singleton()->SetMaster(false);
    LOG_D("[CoordinatorLeaderAgent] I have become a slave.");
    controllerListener->Master2Worker();
}

void CoordinatorLeaderAgent::Slave2Master()
{
    if (!Configure::Singleton()->CheckBackup()) { // 若不开主备功能不用走这里的建链逻辑
        return;
    }
    Configure::Singleton()->SetMaster(true);
    LOG_D("[CoordinatorLeaderAgent] I have become a master.");
    requestListener->CreateLinkWithDNode();
}

void CoordinatorLeaderAgent::Slave2MasterEvent()
{
    // 按需为降备逻辑添加事件
}

void CoordinatorLeaderAgent::SetListener(ControllerListener* cl, RequestListener* rl)
{
    controllerListener = cl;
    requestListener = rl;
}

// 单例实现
CoordinatorLeaderAgent* CoordinatorLeaderAgent::GetInstance()
{
    static CoordinatorLeaderAgent instance;
    return &instance;
}

} // namespace MINDIE::MS
