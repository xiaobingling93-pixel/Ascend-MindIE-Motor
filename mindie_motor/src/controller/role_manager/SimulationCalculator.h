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
#ifndef MINDIE_DIGS_SIMULATION_CALCULATOR_H
#define MINDIE_DIGS_SIMULATION_CALCULATOR_H

#include "common.h"
#include "LlamaSimulator.h"

namespace MINDIE::MS::simulation {
class SimulationCalculator {
public:
    using Config = std::map<std::string, std::string>;
    SimulationCalculator() = default;
    ~SimulationCalculator()= default;
    int32_t Create(Config& config);
    void CalAbility(const DIGSRequestSummary& summary, DIGSGroupPDRatio& digsGroupPdRatio);
    static SimulationCalculator& GetInstance();
private:
    int32_t CreateLlamaSimulator(Config& config);
private:
    std::unique_ptr<LlamaSimulator> llamaSimulator_;
};
    
}
#endif