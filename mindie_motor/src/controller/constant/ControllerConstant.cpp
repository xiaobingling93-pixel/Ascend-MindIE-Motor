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
#include "ControllerConstant.h"
#include "Logger.h"
namespace MINDIE::MS {
ControllerConstant::ControllerConstant()
{
    InitFilePath();
    InitRoleState();
    InitCoordinatorURI();
    InitServerURI();
    InitCCAEURI();
    InitEnvParam();
    InitConfigKey();
    InitDIGSInstanceRole();
    InitRoleType();
    InitDeployMode();
    InitNodeManagerURI();
}

void ControllerConstant::InitFilePath()
{
    mFilePathMap[FilePath::CONFIG_PATH] = "./conf/ms_controller.json";
}

void ControllerConstant::InitRoleState()
{
    mRoleStateMap[RoleState::UNKNOWN] = "RoleUnknown";
    mRoleStateMap[RoleState::SWITCHING] = "RoleSwitching";
    mRoleStateMap[RoleState::READY] = "RoleReady";
}

void ControllerConstant::InitCoordinatorURI()
{
    mCoordinatorURIMap[CoordinatorURI::POST_REFRESH] = "/v1/instances/refresh";
    mCoordinatorURIMap[CoordinatorURI::GET_INFO] = "/v1/coordinator_info";
    mCoordinatorURIMap[CoordinatorURI::POST_OFFLINE] = "/v1/instances/offline";
    mCoordinatorURIMap[CoordinatorURI::POST_ONLINE] = "/v1/instances/online";
    mCoordinatorURIMap[CoordinatorURI::GET_TASK] = "/v1/instances/tasks";
    mCoordinatorURIMap[CoordinatorURI::POST_QUERY_TASK] = "/v1/instances/query_tasks";
    mCoordinatorURIMap[CoordinatorURI::GET_METRICS] = "/metrics";
    mCoordinatorURIMap[CoordinatorURI::POST_BACKUP_INFO] = "/backup_info";
    mCoordinatorURIMap[CoordinatorURI::GET_RECVS_INFO] = "/recvs_info";
    mCoordinatorURIMap[CoordinatorURI::GET_HEALTH_READY] = "/v2/health/ready";
}

void ControllerConstant::InitServerURI()
{
    mServerURIMap[ServerURI::GET_CONFIG] = "/v1/config";
    mServerURIMap[ServerURI::GET_STATUS_V1] = "/v1/status";
    mServerURIMap[ServerURI::GET_STATUS_V2] = "/v2/status";
    mServerURIMap[ServerURI::POST_DECODE_ROLE_V1] = "/v1/role/decode";
    mServerURIMap[ServerURI::POST_PREFILL_ROLE_V1] = "/v1/role/prefill";
    mServerURIMap[ServerURI::POST_DECODE_ROLE_V2] = "/v2/role/decode";
    mServerURIMap[ServerURI::POST_PREFILL_ROLE_V2] = "/v2/role/prefill";
    mServerURIMap[ServerURI::POST_FLEX_ROLE_V1] = "/v1/role/flex";
    mServerURIMap[ServerURI::POST_FLEX_ROLE_V2] = "/v2/role/flex";
    mServerURIMap[ServerURI::STOP_SERVICE] = "/stopService";
}

void ControllerConstant::InitCCAEURI()
{
    mCCAEURIMap[CCAEURI::REGISTER] = "/rest/ccaeommgmt/v1/managers/mindie/register";
    mCCAEURIMap[CCAEURI::INVENTORY] = "/rest/ccaeommgmt/v1/managers/mindie/inventory";
}

void ControllerConstant::InitEnvParam()
{
    mEnvParamMap[EnvParam::CONTROLLER_CONFIG_FILE_PATH] = "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH";
    mEnvParamMap[EnvParam::GLOBAL_RANK_TABLE] = "GLOBAL_RANK_TABLE_FILE_PATH";
    mEnvParamMap[EnvParam::P_RATE] = "MINDIE_MS_P_RATE";
    mEnvParamMap[EnvParam::D_RATE] = "MINDIE_MS_D_RATE";
    mEnvParamMap[EnvParam::POD_IP] = "POD_IP";
    mEnvParamMap[EnvParam::CHECK_FILES] = "MINDIE_CHECK_INPUTFILES_PERMISSION";
}

void ControllerConstant::InitConfigKey()
{
    mConfigKeyMap[ConfigKey::CONFIG_KEY_GLOBAL_RANK_TABLE_PATH] = "global_rank_table_file_path";
    mConfigKeyMap[ConfigKey::CONFIG_KEY_SERVER_PORT] = "mindie_server_port";
    mConfigKeyMap[ConfigKey::CONFIG_KEY_SERVER_CONTROL_PORT] = "mindie_server_control_port";
    mConfigKeyMap[ConfigKey::CONFIG_KEY_SERVER_METRIC_PORT] = "mindie_server_metric_port";
    mConfigKeyMap[ConfigKey::CONFIG_KEY_COORDINATOR_PORT] = "mindie_ms_coordinator_port";
    mConfigKeyMap[ConfigKey::CONFIG_KEY_COORDINATOR_EXTERNAL_PORT] = "mindie_ms_coordinator_external_port";
    mConfigKeyMap[ConfigKey::CONFIG_KEY_CONTROLLER_ALARM_PORT] = "controller_alarm_port";
    mConfigKeyMap[ConfigKey::CONFIG_KEY_MODEL_CONFIG_FILE_PATH] = "digs_model_config_path";
    mConfigKeyMap[ConfigKey::CONFIG_KEY_MACHINE_CONFIG_FILE_PATH] = "digs_machine_config_path";
    mConfigKeyMap[ConfigKey::CONFIG_KEY_DIGS_PREFILL_SLO] = "digs_prefill_slo";
    mConfigKeyMap[ConfigKey::CONFIG_KEY_DIGS_DECODE_SLO] = "digs_decode_slo";
    mConfigKeyMap[ConfigKey::CONFIG_KEY_MODEL_TYPE] = "model_type";
    mConfigKeyMap[ConfigKey::CONFIG_KEY_TRANSFER_TYPE] = "transfer_type";
    mConfigKeyMap[ConfigKey::CONFIG_KEY_DIGS_PP] = "digs_pp";
    mConfigKeyMap[ConfigKey::CONFIG_KEY_ROLE_DECISION_METHODS] = "role_decision_methods";
    mConfigKeyMap[ConfigKey::CONFIG_KEY_IP] = "ip";
    mConfigKeyMap[ConfigKey::CONFIG_KEY_NODE_MANAGER_PORT] = "node_manager_port";
}

void ControllerConstant::InitRoleType()
{
    mRoleTypeMap["prefill"] = RoleType::PREFILL;
    mRoleTypeMap["decode"] = RoleType::DECODE;
    mRoleTypeMap["flex"] = RoleType::FLEX;
    mRoleTypeMap["none"] = RoleType::NONE;
}

void ControllerConstant::InitDeployMode()
{
    mDeployModeMap["pd_separate"] = DeployMode::PD_SEPARATE;
    mDeployModeMap["pd_disaggregation"] = DeployMode::PD_SEPARATE;
    mDeployModeMap["pd_disaggregation_single_container"] = DeployMode::PD_SEPARATE;
    mDeployModeMap["single_node"] = DeployMode::SINGLE_NODE;
}

void ControllerConstant::InitDIGSInstanceRole()
{
    mDigsRoleMap["prefill"] = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
    mDigsRoleMap["decode"] = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
    mDigsRoleMap["flex"] = MINDIE::MS::DIGSInstanceRole::FLEX_INSTANCE;
    mDigsRoleMap["none"] = MINDIE::MS::DIGSInstanceRole::UN_DEF_INSTANCE;
}

std::string ControllerConstant::GetFilePath(FilePath type)
{
    auto iter = mFilePathMap.find(type);
    if (iter == mFilePathMap.end()) {
        LOG_E("[%s] [ControllerConstant] File path type %d is not found.",
            GetErrorCode(ErrorType::NOT_FOUND, ControllerFeature::CONSTANT).c_str(),
            type);
        return {};
    }
    return iter->second;
}

std::string ControllerConstant::GetRoleState(RoleState type)
{
    auto iter = mRoleStateMap.find(type);
    if (iter == mRoleStateMap.end()) {
        LOG_E("[%s] [ControllerConstant] Role state type %d is not found.",
            GetErrorCode(ErrorType::NOT_FOUND, ControllerFeature::CONSTANT).c_str(),
            type);
        return {};
    }
    return iter->second;
}

std::string ControllerConstant::GetCoordinatorURI(CoordinatorURI type)
{
    auto iter = mCoordinatorURIMap.find(type);
    if (iter == mCoordinatorURIMap.end()) {
        LOG_E("[%s] [ControllerConstant] Coordinator URI type %d is not found.",
            GetErrorCode(ErrorType::NOT_FOUND, ControllerFeature::CONSTANT).c_str(),
            type);
        return {};
    }
    return iter->second;
}

std::string ControllerConstant::GetServerURI(ServerURI type)
{
    auto iter = mServerURIMap.find(type);
    if (iter == mServerURIMap.end()) {
        LOG_E("[%s] [ControllerConstant] Server URI type %d is not found.",
            GetErrorCode(ErrorType::NOT_FOUND, ControllerFeature::CONSTANT).c_str(),
            type);
        return {};
    }
    return iter->second;
}

std::string ControllerConstant::GetCCAEURI(CCAEURI type)
{
    auto iter = mCCAEURIMap.find(type);
    if (iter == mCCAEURIMap.end()) {
        LOG_E("[%s] [ControllerConstant] CCAE URI type %d is not found.",
            GetErrorCode(ErrorType::NOT_FOUND, ControllerFeature::CONSTANT).c_str(),
            type);
        return {};
    }
    return iter->second;
}

std::string ControllerConstant::GetEnvParam(EnvParam type)
{
    auto iter = mEnvParamMap.find(type);
    if (iter == mEnvParamMap.end()) {
        LOG_E("[%s] [ControllerConstant] Environment parameter type %d is not found.",
            GetErrorCode(ErrorType::NOT_FOUND, ControllerFeature::CONSTANT).c_str(),
            type);
        return {};
    }
    return iter->second;
}

std::string ControllerConstant::GetConfigKey(ConfigKey type)
{
    auto iter = mConfigKeyMap.find(type);
    if (iter == mConfigKeyMap.end()) {
        LOG_E("[%s] [ControllerConstant] Config key type %d is not found.",
            GetErrorCode(ErrorType::NOT_FOUND, ControllerFeature::CONSTANT).c_str(),
            type);
        return {};
    }
    return iter->second;
}

RoleType ControllerConstant::GetRoleTypeByStr(const std::string &str)
{
    auto iter = mRoleTypeMap.find(str);
    if (iter == mRoleTypeMap.end()) {
        LOG_E("[%s] [ControllerConstant] Role type string %s is not found.",
            GetErrorCode(ErrorType::NOT_FOUND, ControllerFeature::CONSTANT).c_str(),
            str.c_str());
        return {};
    }
    return iter->second;
}


MINDIE::MS::DIGSInstanceRole ControllerConstant::GetDIGSInstanceRoleByStr(const std::string &str)
{
    auto iter = mDigsRoleMap.find(str);
    if (iter == mDigsRoleMap.end()) {
        LOG_E("[%s] [ControllerConstant] DIGS instance role string %s is not found.",
            GetErrorCode(ErrorType::NOT_FOUND, ControllerFeature::CONSTANT).c_str(),
            str.c_str());
        return {};
    }
    return iter->second;
}

DeployMode ControllerConstant::GetDeployModeByStr(const std::string &str)
{
    auto iter = mDeployModeMap.find(str);
    if (iter == mDeployModeMap.end()) {
        LOG_E("[%s] [ControllerConstant] Deploy mode string %s is not found.",
            GetErrorCode(ErrorType::NOT_FOUND, ControllerFeature::CONSTANT).c_str(),
            str.c_str());
        return {};
    }
    return iter->second;
}

bool ControllerConstant::IsSingleContainer(const std::string &deployModeStr) const
{
    bool isSingleContainer = false;
    if (deployModeStr == "pd_disaggregation_single_container") {
        isSingleContainer = true;
    }
    return isSingleContainer;
}

std::string ControllerConstant::GetDigsRoleDecisionMethod() const
{
    return "digs";
}

std::string ControllerConstant::GetGroupGeneratorTypeDefault() const
{
    return "Default";
}

std::string ControllerConstant::GetMindIECoordinatorGroup() const
{
    return "0";
}

std::string ControllerConstant::GetMindIEServerGroup() const
{
    return "2";
}

std::string ControllerConstant::GetMindIEVersion() const
{
    return "";
}

int32_t ControllerConstant::GetComponentType() const
{
    return 0;
}

void ControllerConstant::InitNodeManagerURI()
{
    mNodeManagerURIMap[NodeManagerURI::SEND_FAULT_COMMAND] = "/v1/node-manager/fault-handling-command";
    mNodeManagerURIMap[NodeManagerURI::GET_NODE_STATUS] = "/v1/node-manager/running-status";
}

std::string ControllerConstant::GetNodeManagerURI(NodeManagerURI type)
{
    auto iter = mNodeManagerURIMap.find(type);
    if (iter == mNodeManagerURIMap.end()) {
        LOG_E("[%s] [ControllerConstant] NodeManager URI type %d is not found.",
            GetErrorCode(ErrorType::NOT_FOUND, ControllerFeature::CONSTANT).c_str(),
            static_cast<int32_t>(type));
        return {};
    }
    return iter->second;
}

}