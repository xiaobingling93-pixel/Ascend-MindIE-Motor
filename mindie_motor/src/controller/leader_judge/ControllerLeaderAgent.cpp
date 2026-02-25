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
#include "AlarmManager.h"
#include "AlarmRequestHandler.h"
#include "ControllerConfig.h"
#include "LeaderAgent.h"
#include "ControllerLeaderAgent.h"

namespace MINDIE::MS {

ControllerLeaderAgent::~ControllerLeaderAgent() {}

void ControllerLeaderAgent::Master2Slave()
{
    ControllerConfig::GetInstance()->SetLeader(false);
}

void ControllerLeaderAgent::Slave2Master()
{
    ControllerConfig::GetInstance()->SetLeader(true);
}

void ControllerLeaderAgent::Slave2MasterEvent()
{
    std::string eventMsg = AlarmRequestHandler::GetInstance()->FillControllerToSlaveEventInfo(
        ControllerToSlaveReason::MASTER_CONTROLLER_EXCEPTION);
    AlarmManager::GetInstance()->AlarmAdded(eventMsg); // 原始告警直接入队
    LOG_D("[LeaderAgent] Add event : %s successfully.", eventMsg.c_str());
}

// 单例实现
ControllerLeaderAgent* ControllerLeaderAgent::GetInstance()
{
    static ControllerLeaderAgent instance;
    return &instance;
}

} // namespace MINDIE::MS
