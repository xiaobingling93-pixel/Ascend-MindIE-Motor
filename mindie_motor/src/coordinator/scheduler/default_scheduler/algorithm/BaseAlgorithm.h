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
#ifndef MINDIE_BASE_ALGORITHM_H
#define MINDIE_BASE_ALGORITHM_H

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include "digs_instance.h"
#include "NodeStore.h"

namespace MINDIE::MS {

enum class DeployMode {
    SINGLE_NODE,  // 单机
    PD_SEPARATE // PD分离
};

enum class AlgorithmMode {
    LOAD_BALANCE,    // 负载均衡
    CACHE_AFFINITY,  // Cache亲和
    ROUND_ROBIN // 轮询算法
};

// 算法的基类，支持扩展各种调度算法
class BaseAlgorithm {
public:
    BaseAlgorithm() = default;

    virtual int32_t Execute(DeployMode deployMode, std::string requestBody, std::vector<uint64_t> &pickedNodes,
        int type) = 0;

    virtual ~BaseAlgorithm() = default;
};

// 算法executor 构造函数
std::unique_ptr<BaseAlgorithm> CreateAlgorithmExecutor(AlgorithmMode algorithmType,
    std::unique_ptr<NodeStore> &nodeStore, std::map<std::string, std::string> config);
}
#endif