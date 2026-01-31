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
#ifndef MINDIE_MS_MANAGERCONSTANT_H
#define MINDIE_MS_MANAGERCONSTANT_H

#include <string>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include "digs_instance.h"
namespace MINDIE::MS {
enum class FilePath : int32_t {
    CONFIG_PATH = 0,
};

enum class RoleState : int32_t {
    UNKNOWN = 0,
    SWITCHING,
    READY
};

enum class RoleType : int32_t {
    PREFILL = 0,
    DECODE,
    FLEX,
    NONE
};

enum class CoordinatorURI : int32_t {
    POST_REFRESH = 0,
    GET_INFO,
    POST_OFFLINE,
    POST_ONLINE,
    GET_TASK,
    POST_QUERY_TASK,
    GET_METRICS,
    POST_BACKUP_INFO,
    GET_RECVS_INFO,
    GET_HEALTH_READY
};

enum class ServerURI : int32_t {
    GET_CONFIG = 0,
    GET_STATUS_V1,
    POST_DECODE_ROLE_V1,
    POST_PREFILL_ROLE_V1,
    POST_DECODE_ROLE_V2,
    POST_PREFILL_ROLE_V2,
    POST_FLEX_ROLE_V1,
    POST_FLEX_ROLE_V2,
    GET_STATUS_V2,
    STOP_SERVICE,
};

enum class CCAEURI : int32_t {
    REGISTER = 0,
    INVENTORY
};

enum class EnvParam : int32_t {
    CONTROLLER_CONFIG_FILE_PATH = 0,
    GLOBAL_RANK_TABLE,
    P_RATE,
    D_RATE,
    POD_IP,
    CHECK_FILES,
};


enum class ConfigKey : int32_t {
    CONFIG_KEY_GLOBAL_RANK_TABLE_PATH = 0,
    CONFIG_KEY_SERVER_PORT,
    CONFIG_KEY_SERVER_CONTROL_PORT,
    CONFIG_KEY_COORDINATOR_PORT,
    CONFIG_KEY_COORDINATOR_EXTERNAL_PORT,
    CONFIG_KEY_CONTROLLER_ALARM_PORT,
    CONFIG_KEY_NODE_MANAGER_PORT,
    CONFIG_KEY_MODEL_CONFIG_FILE_PATH,
    CONFIG_KEY_MACHINE_CONFIG_FILE_PATH,
    CONFIG_KEY_DIGS_PREFILL_SLO,
    CONFIG_KEY_DIGS_DECODE_SLO,
    CONFIG_KEY_MODEL_TYPE,
    CONFIG_KEY_TRANSFER_TYPE,
    CONFIG_KEY_DIGS_PP,
    CONFIG_KEY_ROLE_DECISION_METHODS,
    CONFIG_KEY_IP,
    CONFIG_KEY_SERVER_METRIC_PORT
};

enum class DeployMode : int32_t {
    SINGLE_NODE = 0,  // 单机
    PD_SEPARATE // PD分离
};

enum class NodeManagerURI : int32_t {
    SEND_FAULT_COMMAND = 0,
    GET_NODE_STATUS
};

class ControllerConstant {
public:
    static ControllerConstant *GetInstance()
    {
        static ControllerConstant instance;
        return &instance;
    }

    std::string GetFilePath(FilePath type);
    std::string GetRoleState(RoleState type);
    std::string GetCoordinatorURI(CoordinatorURI type);
    std::string GetServerURI(ServerURI type);
    std::string GetCCAEURI(CCAEURI type);
    std::string GetEnvParam(EnvParam type);
    std::string GetConfigKey(ConfigKey type);
    RoleType GetRoleTypeByStr(const std::string &str);
    MINDIE::MS::DIGSInstanceRole GetDIGSInstanceRoleByStr(const std::string &str);
    DeployMode GetDeployModeByStr(const std::string &str);
    std::string GetDigsRoleDecisionMethod() const;
    std::string GetGroupGeneratorTypeDefault() const;
    std::string GetMindIECoordinatorGroup() const;
    std::string GetMindIEServerGroup() const;
    std::string GetMindIEVersion() const;
    int32_t GetComponentType() const;
    bool IsSingleContainer(const std::string &deployModeStr) const;
    std::string GetNodeManagerURI(NodeManagerURI type);

    ControllerConstant();
    ~ControllerConstant() = default;
    ControllerConstant(const ControllerConstant &obj) = delete;
    ControllerConstant &operator=(const ControllerConstant &obj) = delete;
    ControllerConstant(ControllerConstant &&obj) = delete;
    ControllerConstant &operator=(ControllerConstant &&obj) = delete;
private:
    void InitFilePath();
    void InitRoleState();
    void InitCoordinatorURI();
    void InitServerURI();
    void InitCCAEURI();
    void InitEnvParam();
    void InitConfigKey();
    void InitRoleType();
    void InitDIGSInstanceRole();
    void InitDeployMode();
    void InitNodeManagerURI();

    std::unordered_map<FilePath, std::string> mFilePathMap {};
    std::unordered_map<RoleState, std::string> mRoleStateMap {};
    std::unordered_map<CoordinatorURI, std::string> mCoordinatorURIMap {};
    std::unordered_map<ServerURI, std::string> mServerURIMap {};
    std::unordered_map<CCAEURI, std::string> mCCAEURIMap {};
    std::unordered_map<EnvParam, std::string> mEnvParamMap {};
    std::unordered_map<ConfigKey, std::string> mConfigKeyMap {};
    std::unordered_map<std::string, RoleType> mRoleTypeMap {};
    std::unordered_map<std::string, MINDIE::MS::DIGSInstanceRole> mDigsRoleMap {};
    std::unordered_map<std::string, DeployMode> mDeployModeMap {};
    std::unordered_map<NodeManagerURI, std::string> mNodeManagerURIMap {};
};
}

#endif // MINDIE_MS_MANAGERCONSTANT_H
