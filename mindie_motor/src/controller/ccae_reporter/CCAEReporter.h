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
#ifndef MINDIE_MS_CCAE_REPORTER_H
#define MINDIE_MS_CCAE_REPORTER_H

#include <atomic>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <thread>
#include <unordered_map>
#include "CoordinatorStore.h"
#include "ControllerConfig.h"
#include "CCAEStatus.h"
#include "HttpClient.h"
#include "NodeStatus.h"

namespace MINDIE::MS {
class CCAEReporter {
public:
    CCAEReporter(const CCAEReporter &obj) = delete;
    CCAEReporter &operator=(const CCAEReporter &obj) = delete;
    CCAEReporter(CCAEReporter &&obj) = delete;
    CCAEReporter &operator=(CCAEReporter &&obj) = delete;
    CCAEReporter(std::shared_ptr<CCAEStatus> ccaeStatusInit,
                std::shared_ptr<CoordinatorStore> coordinatorStoreInit,
                std::shared_ptr<CoordinatorStore> coordinatorStoreWithMasterInfoInit,
                std::shared_ptr<NodeStatus> mNodeStatusInit);
    ~CCAEReporter();
    int32_t Init();
    void Stop();

#ifndef UT_FLAG
private:
#endif // UT_FLAG
    std::shared_ptr<HttpClient> mCCAERegisterClient = nullptr;
    std::shared_ptr<HttpClient> mCCAEInventoriesClient = nullptr;
    std::shared_ptr<HttpClient> mCoordinatorClient = nullptr;
    std::shared_ptr<CCAEStatus> mCCAEStatus = nullptr;
    std::shared_ptr<CoordinatorStore> mCoordinatorStore = nullptr;
    std::shared_ptr<CoordinatorStore> mCoordinatorStoreWithMasterInfo = nullptr;
    std::shared_ptr<NodeStatus> mNodeStatus = nullptr;
    bool mMetricCanStart { false };
    std::atomic<bool> mRun = { true };
    std::unique_ptr<std::thread> mRegisterThread = nullptr;
    std::unique_ptr<std::thread> mMetricsThread = nullptr;
    std::vector<std::string> mModelIDStack {};
    std::unordered_map<std::string, uint64_t> mMetricsPeriods {};
    std::unordered_map<std::string, uint64_t> mMemTimeStamp {};
    mutable std::string mLastMasterKey;

    void ResetReportStack();
    void StartRegisterThread();
    void StartMetricsThread();
    bool UpdateModelID2Report();
    bool ReachUpdateTime(std::string modelID);
    int32_t SendRegister();
    std::string GetMetricsInfo();
    int32_t SendInventories();
    void SelectMasterCoordinator(std::vector<std::unique_ptr<Coordinator>> &selectedCoordinators) const;
};
}
#endif // MINDIE_MS_CCAE_REPORTER_H