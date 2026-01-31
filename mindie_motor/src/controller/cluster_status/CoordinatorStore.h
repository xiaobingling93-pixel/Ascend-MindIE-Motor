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
#ifndef MINDIE_MS_COORDINATORSTORE_H
#define MINDIE_MS_COORDINATORSTORE_H

#include <memory>
#include <string>
#include <vector>
#include <shared_mutex>
namespace MINDIE::MS {
struct Coordinator {
    std::string ip {};
    std::string port {};
    bool isHealthy = false; // 所有请求的接口是否能通，请求失败代表不健康
    int32_t recvFlow = 0;       // 流量信息
    bool isMaster = false;  // 主备标志位
};

class CoordinatorStore {
public:
    void UpdateCoordinators(std::vector<std::unique_ptr<Coordinator>> &coordinatorVec);
    void UpdateCoordinatorStatus(std::string &ip, bool isHealthy);
    std::vector<std::unique_ptr<Coordinator>> GetCoordinators();
private:
    std::vector<std::unique_ptr<Coordinator>> mCoordinators {};
    std::shared_mutex mMtx {};
};
}
#endif // MINDIE_MS_COORDINATORSTORE_H
