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
#ifndef MINDIE_IPC_CONFIG_H
#define MINDIE_IPC_CONFIG_H

#include <string>

namespace MINDIE::MS {
    inline constexpr char const *ALM_CTRL_SHM_NAME = "/mindie_controller_alarms";
    inline constexpr char const *ALM_CTRL_SEM_NAME  = "/mindie_controller_alarms_sem";
    inline constexpr char const *ALM_COORD_SHM_NAME = "/mindie_coordinator_alarms";
    inline constexpr char const *ALM_COORD_SEM_NAME  = "/mindie_coordinator_alarms_sem";

    inline constexpr char const *HB_CTRL_SHM_NAME = "/smu_ctrl_heartbeat_shm";
    inline constexpr char const *HB_CTRL_SEM_NAME = "/smu_ctrl_heartbeat_sem";
    inline constexpr char const *HB_COORD_SHM_NAME = "/smu_coord_heartbeat_shm";
    inline constexpr char const *HB_COORD_SEM_NAME = "/smu_coord_heartbeat_sem";
    inline constexpr char const *HB_ADAPTER_SHM_NAME = "/smu_adapter_heartbeat_shm";
    inline constexpr char const *HB_ADAPTER_SEM_NAME = "/smu_adapter_heartbeat_sem";
    inline constexpr size_t HEARTBEAT_PRODUCER_INTERVAL_MS = 5000;
    inline constexpr size_t DEFAULT_HB_BUFFER_SIZE = 128;
    inline constexpr size_t DEFAULT_ALM_BUFFER_SIZE = 10 * 1024 * 1024;

    inline std::string GenerateHeartbeatShmName(const std::string& instanceName)
    {
        return "/smu_heartbeat_" + instanceName + "_shm";
    }
    
    inline std::string GenerateHeartbeatSemName(const std::string& instanceName)
    {
        return "/smu_heartbeat_" + instanceName + "_sem";
    }
}
#endif
