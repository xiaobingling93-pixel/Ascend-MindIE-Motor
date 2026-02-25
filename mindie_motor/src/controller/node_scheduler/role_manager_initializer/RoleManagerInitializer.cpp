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
#include "RoleManagerInitializer.h"
#include "Util.h"
#include "ControllerConfig.h"
#include "ConfigParams.h"

namespace MINDIE::MS {
static double g_minDouble = 1e-6;
static double g_maxEff = 1.;
static double g_maxDouble = 2147483647.;

static bool IsStringParamsValid(const nlohmann::json &jsonObj, const std::string &key, uint32_t minLen = 1,
    uint32_t maxLen = 4096)
{
    if (!jsonObj.contains(key)) {
        return false;
    }
    if (!jsonObj[key].is_string()) {
        return false;
    }
    std::string val = jsonObj[key];
    if (val.size() < minLen || val.size() > maxLen) {
        return false;
    }
    return true;
}

static bool IsModelParamsValid(const nlohmann::json &config)
{
    std::vector<std::string> keys {"hidden_size", "initializer_range", "intermediate_size", "max_position_embeddings",
        "num_attention_heads", "num_hidden_layers", "num_key_value_heads"};
    std::vector<std::string> validType {"float16", "bfloat16"};
    try {
        for (auto &key : std::as_const(keys)) {
            if (IsStringParamsValid(config, key)) {
                auto value = std::stod(config[key].get<std::string>());
                if (value >= g_minDouble && value <= g_maxDouble) {
                    continue;
                }
                LOG_E("[%s] [RoleManagerInitializer] Model config JSON object %s should be in range of [%f, %f].",
                      GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::ROLE_MANAGER_INITIALIZER).c_str(),
                      key.c_str(), g_minDouble, g_maxDouble);
                return false;
            }
            if (!IsJsonDoubleValid(config, key, g_minDouble, g_maxDouble)) {
                LOG_E("[%s] [RoleManagerInitializer] Model config JSON. object has invalid %s.",
                    GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::ROLE_MANAGER_INITIALIZER).c_str(),
                    key.c_str());
                return false;
            }
        }
        if (!IsJsonStringValid(config, "torch_dtype")) {
            return false;
        }
        if (std::find(validType.begin(), validType.end(), config["torch_dtype"]) == validType.end()) {
            LOG_E("[%s] [ControllerConfig] Model config JSON torch data type is invalid.",
                GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::ROLE_MANAGER_INITIALIZER).c_str());
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        LOG_E("[%s] [RoleManagerInitializer] Get model config error %s",
            GetErrorCode(ErrorType::EXCEPTION, ControllerFeature::ROLE_MANAGER_INITIALIZER).c_str(),
            e.what());
        return false;
    }
}

static bool IsMachineEffParamsValid(const nlohmann::json &config)
{
    std::vector<std::string> effKeys {"BWeff", "TFLOPSeff", "MBW_TBeff", "eta_OOM"};
    for (auto &key : std::as_const(effKeys)) {
        if (IsStringParamsValid(config, key)) {
            auto value = std::stod(config[key].get<std::string>());
            if (value >= g_minDouble && value <= g_maxEff) {
                continue;
            }
            LOG_E("[%s] [RoleManagerInitializer] IsMachineParamsValid: json object %s should be in range of [%f, %f]",
                  GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::ROLE_MANAGER_INITIALIZER).c_str(),
                  key.c_str(), g_minDouble, g_maxEff);
            return false;
        }
        if (!IsJsonDoubleValid(config, key, g_minDouble, g_maxEff)) {
            LOG_E("[%s] [RoleManagerInitializer] IsMachineParamsValid: json object has invalid %s",
                  GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::ROLE_MANAGER_INITIALIZER).c_str(),
                  key.c_str());
            return false;
        }
    }
    return true;
}

static bool IsMachineParamsValid(const nlohmann::json &config)
{
    std::vector<std::string> keys {"BW_GB", "BW_RDMA_Gb", "TFLOPS", "MBW_TB", "alpha", "MEMCapacity",
        "staticTransferDelay"};
    try {
        for (auto &key : std::as_const(keys)) {
            if (IsStringParamsValid(config, key)) {
                auto value = std::stod(config[key].get<std::string>());
                if (value >= g_minDouble && value <= g_maxDouble) {
                    continue;
                }
                LOG_E("[%s] [RoleManagerInitializer] Machine parameter '%s' is out of the valid range [%f, %f]",
                      GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::ROLE_MANAGER_INITIALIZER).c_str(),
                      key.c_str(), g_minDouble, g_maxDouble);
                return false;
            }
            if (!IsJsonDoubleValid(config, key, g_minDouble, g_maxDouble)) {
                LOG_E("[%s] [RoleManagerInitializer] Machine parameter '%s' is invalid.",
                    GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::ROLE_MANAGER_INITIALIZER).c_str(),
                    key.c_str());
                return false;
            }
        }
        return IsMachineEffParamsValid(config);
    } catch (const std::exception& e) {
        LOG_E("[%s] Failed to get machine configuration, error is %s.",
            GetErrorCode(ErrorType::EXCEPTION, ControllerFeature::ROLE_MANAGER_INITIALIZER).c_str(),
            e.what());
        return false;
    }
}

static std::string BuildModelParams(const nlohmann::json &config)
{
    nlohmann::json modelParams;
    std::vector<std::string> keys {"hidden_size", "initializer_range", "intermediate_size", "max_position_embeddings",
        "num_attention_heads", "num_hidden_layers", "num_key_value_heads", "torch_dtype"};
    for (auto key : std::as_const(keys)) {
        modelParams[key] = config[key];
    }
    return modelParams.dump(4); // 4 json格式行缩进为4
}

static std::string BuildMachineParams(const nlohmann::json &config)
{
    nlohmann::json machineParams;
    std::vector<std::string> keys {"BW_GB", "BW_RDMA_Gb", "BWeff", "TFLOPS", "TFLOPSeff", "MBW_TB",
        "MBW_TBeff", "alpha", "MEMCapacity", "eta_OOM", "staticTransferDelay"};
    for (auto key : std::as_const(keys)) {
        machineParams[key] = config[key];
    }
    return machineParams.dump(4); // 4 json格式行缩进为4
}

MINDIE::MS::roleManager::InstanceRoleManager::Config BuildInstanceRoleManagerConfig(uint32_t tp)
{
    MINDIE::MS::roleManager::InstanceRoleManager::Config config;
    std::string modelPath = ControllerConfig::GetInstance()->GetModelConfigFilePath();
    auto modelParams = FileToJsonObj(modelPath, 0640); // 配置文件的权限要求是0640
    if (modelParams.empty() || !IsModelParamsValid(modelParams)) {
        LOG_E("[%s] [RoleManagerInitializer] Build DIGS role manager config failed, model config is invalid.",
            GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::ROLE_MANAGER_INITIALIZER).c_str());
        return {};
    }
    std::string machinePath = ControllerConfig::GetInstance()->GetMachineConfigFilePath();
    auto machineParams = FileToJsonObj(machinePath, 0640); // 配置文件的权限要求是0640
    if (machineParams.empty() || !IsMachineParamsValid(machineParams)) {
        LOG_E("[%s] [RoleManagerInitializer] Build DIGS role manager config failed, machine config is invalid.",
            GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::ROLE_MANAGER_INITIALIZER).c_str());
        return {};
    }
    config["model_params"] = BuildModelParams(modelParams);
    config["machine_params"] = BuildMachineParams(machineParams);
    config["prefill_slo"] = ControllerConfig::GetInstance()->GetDIGSPrefillSLO();
    config["decode_slo"] = ControllerConfig::GetInstance()->GetDIGSDecodeSLO();
    config["time_period"] = ControllerConfig::GetInstance()->GetDIGSTimePeriod();
    config["is_heterogeneous"] = ControllerConfig::GetInstance()->GetDIGSIsHeterogeneous() ? "true" : "false";
    config["model_type"] = ControllerConfig::GetInstance()->GetModelType();
    config["transfer_type"] = ControllerConfig::GetInstance()->GetDIGSTransferType();
    config["pp"] = ControllerConfig::GetInstance()->GetDIGSPP();
    config["tp"] = std::to_string(tp);
    config["is_auto_pd_role_switching"] = ControllerConfig::GetInstance()->GetDIGSIsAutoSwitching() ? "true" : "false";
    config["hardware_card_nums"] = "8"; // 当前仅支持8卡
    uint32_t pNodeSize = ControllerConfig::GetInstance()->GetPNodeNum();
    uint32_t dNodeSize = ControllerConfig::GetInstance()->GetDNodeNum();
    std::string skipDecision = pNodeSize != dNodeSize ? "true" : "false";
    config["is_skip_decision_for_cross_node_mode"] = ControllerConfig::GetInstance()->IsMultiNodeMode() ?
        skipDecision : "false";
    config["has_flex"] = ControllerConfig::GetInstance()->GetHasFlex() ? "true" : "false";
    return config;
}
}