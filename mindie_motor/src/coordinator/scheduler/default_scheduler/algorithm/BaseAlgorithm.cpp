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
#include <map>
#include <memory>
#include <string>
#include "Logger.h"
#include "PrefixCacheExecutor.h"
#include "RoundRobinExecutor.h"
#include "LoadBalanceExecutor.h"
#include "BaseAlgorithm.h"

namespace MINDIE::MS {

// 算法executor 构造函数
std::unique_ptr<BaseAlgorithm> CreateAlgorithmExecutor(AlgorithmMode algorithmType,
    std::unique_ptr<NodeStore> &nodeStore, std::map<std::string, std::string> config)
{
    if (algorithmType == AlgorithmMode::CACHE_AFFINITY) {  // Cache亲和
        auto executor = std::make_unique<PrefixCacheExecutor>(nodeStore, config);
        return executor;
    } else if (algorithmType == AlgorithmMode::ROUND_ROBIN) {  // 轮询算法
        auto executor = std::make_unique<RoundRobinExecutor>(nodeStore);
        return executor;
    } else if (algorithmType == AlgorithmMode::LOAD_BALANCE) { // 负载均衡
        auto executor = std::make_unique<LoadBalanceExecutor>(nodeStore);
        return executor;
    } else {
        LOG_E("[%s] [CreateAlgorithmExecutor] Algorithm mode %d is invalid.",
            GetErrorCode(ErrorType::INVALID_INPUT, CoordinatorFeature::BASEALGORITHM).c_str(),
            algorithmType);
        return nullptr;
    }
}
}