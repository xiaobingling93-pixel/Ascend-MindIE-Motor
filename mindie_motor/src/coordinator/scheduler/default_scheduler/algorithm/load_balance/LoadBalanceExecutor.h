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
#ifndef MINDIE_LOAD_BALANCE_EXECUTOR_H
#define MINDIE_LOAD_BALANCE_EXECUTOR_H

#include <string>
#include <vector>
#include "BaseAlgorithm.h"
#include "Logger.h"

namespace MINDIE::MS {
class LoadBalanceExecutor : public BaseAlgorithm {
public:
    explicit LoadBalanceExecutor(std::unique_ptr<NodeStore> &nodeStorePtr) : nodeStore(nodeStorePtr) {};
    int32_t Execute(__attribute__((unused)) DeployMode deployMode, __attribute__((unused)) std::string requestBody,
        __attribute__((unused)) std::vector<uint64_t> &pickedNodes, __attribute__((unused)) int type) override
    {
        LOG_E("[%s] [LoadBalanceExecutor] The selected load balancing algorithm is not supported.",
            GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::LOADBALANCE_EXECUTOR).c_str());
        return -1;
    }

private:
    std::unique_ptr<NodeStore> &nodeStore; // 节点的信息
};
}

#endif
