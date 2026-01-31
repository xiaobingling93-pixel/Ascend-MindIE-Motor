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
#include "CoordinatorStore.h"
#include <mutex>
#include "Logger.h"

namespace MINDIE::MS {
void CoordinatorStore::UpdateCoordinators(std::vector<std::unique_ptr<Coordinator>> &coordinatorVec)
{
    std::unique_lock<std::shared_mutex> lock(mMtx);
    mCoordinators.clear();
    mCoordinators.insert(mCoordinators.end(),
                         std::make_move_iterator(coordinatorVec.begin()),
                         std::make_move_iterator(coordinatorVec.end()));
}

std::vector<std::unique_ptr<Coordinator>> CoordinatorStore::GetCoordinators()
{
    std::vector<std::unique_ptr<Coordinator>> ret;
    std::shared_lock<std::shared_mutex> lock(mMtx);
    for (auto &coordinator : mCoordinators) {
        auto newCoordinator = std::make_unique<Coordinator>();
        if (newCoordinator == nullptr) {
            LOG_E("[%s] [CoordinatorStore] Create coordinator failed.",
                  GetErrorCode(ErrorType::RESOURCE_EXHAUSTED, ControllerFeature::COORDINATOR_STORE).c_str());
            return {};
        }
        newCoordinator->ip = coordinator->ip;
        newCoordinator->port = coordinator->port;
        newCoordinator->isHealthy = coordinator->isHealthy;
        newCoordinator->isMaster = coordinator->isMaster;
        ret.push_back(std::move(newCoordinator));
    }
    return ret;
}

void CoordinatorStore::UpdateCoordinatorStatus(std::string &ip, bool isHealthy)
{
    std::unique_lock<std::shared_mutex> lock(mMtx);
    for (auto &coordinator : mCoordinators) {
        if (coordinator->ip == ip) {
            coordinator->isHealthy = isHealthy;
            return;
        }
    }
    LOG_E("[%s] [CoordinatorStore] Updating coordinator status, IP %s is not found.",
        GetErrorCode(ErrorType::NOT_FOUND, ControllerFeature::COORDINATOR_STORE).c_str(), ip.c_str());
}
}