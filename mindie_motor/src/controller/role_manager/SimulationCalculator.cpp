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
#include "SimulationCalculator.h"
#include "LlamaSimulator.h"

using namespace MINDIE::MS;

namespace MINDIE::MS::simulation {

int32_t SimulationCalculator::Create(Config& config)
{
    if (CreateLlamaSimulator(config) != static_cast<int32_t>(common::Status::OK)) {
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }
    return static_cast<int32_t>(common::Status::OK);
}

void SimulationCalculator::CalAbility(const DIGSRequestSummary &summary, DIGSGroupPDRatio &digsGroupPdRatio)
{
    llamaSimulator_->CalAbility(summary, digsGroupPdRatio);
    digsGroupPdRatio.tAbility = llamaSimulator_->CalTransferAbility(summary.inputLength);
}

SimulationCalculator &SimulationCalculator::GetInstance()
{
    static SimulationCalculator singleObj;
    return singleObj;
}

int32_t SimulationCalculator::CreateLlamaSimulator(Config &config)
{
    std::string modelType;
    std::unique_ptr<LlamaSimulator> lSimulator;
    do {
        if (!common::GetConfig("model_type", modelType, config)) {
            if (simulation::LlamaSimulator::Create(config, lSimulator) != static_cast<int32_t>(common::Status::OK)) {
                return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
            }
            break;
        }
        std::transform(modelType.begin(), modelType.end(), modelType.begin(), ::tolower);
        if (modelType.find("llama") != std::string::npos) {
            if (simulation::LlamaSimulator::Create(config, lSimulator) != static_cast<int32_t>(common::Status::OK)) {
                return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
            }
            break;
        }
        LOG_W("DIGS: Cannot Find Model Type, Create By Default Model(Llama) Type");
        if (simulation::LlamaSimulator::Create(config, lSimulator) != static_cast<int32_t>(common::Status::OK)) {
            return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
        }
    } while (false);
    llamaSimulator_ = std::move(lSimulator);
    return static_cast<int32_t>(common::Status::OK);
}
}