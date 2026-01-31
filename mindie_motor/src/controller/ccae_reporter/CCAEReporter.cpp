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
#include "CCAEReporter.h"
#include <string>
#include <algorithm>
#include <iterator>
#include <unordered_set>
#include <mutex>
#include <chrono>
#include <thread>
#include "nlohmann/json.hpp"
#include "Logger.h"
#include "CoordinatorRequestHandler.h"
#include "CCAERequestHandler.h"
#include "SharedMemoryUtils.h"

namespace MINDIE::MS {
constexpr uint64_t ONE_SEC_IN_MILLISEC = 1000;
constexpr uint64_t COUNT_SEC = 5;
constexpr uint64_t BUFFER_SIZE = 10 * 1024 * 1024;
const std::string SHM_NAME = "/inventory_shm";
const std::string SEM_NAME = "/inventory_sem";
static OverwriteSharedMemoryUtils shm_writer(SHM_NAME, SEM_NAME, BUFFER_SIZE);


CCAEReporter::CCAEReporter(std::shared_ptr<CCAEStatus> ccaeStatusInit,
    std::shared_ptr<CoordinatorStore> coordinatorStoreInit,
    std::shared_ptr<CoordinatorStore> coordinatorStoreWithMasterInfoInit,
    std::shared_ptr<NodeStatus> mNodeStatusInit) : mCCAEStatus(ccaeStatusInit),
    mCoordinatorStore(coordinatorStoreInit),
    mCoordinatorStoreWithMasterInfo(coordinatorStoreWithMasterInfoInit),
    mNodeStatus(mNodeStatusInit)
{
    LOG_I("[CCAEReporter] Create successfully.");
}

CCAEReporter::~CCAEReporter()
{
    Stop();
    LOG_I("[CCAEReporter] Destroy successfully.");
}

void CCAEReporter::Stop()
{
    mRun.store(false);
    if (mRegisterThread != nullptr && mRegisterThread->joinable()) {
        mRegisterThread->join();
        LOG_I("[CCAEReporter] CCAE register ends successfully.");
    }
    if (mMetricsThread != nullptr && mMetricsThread->joinable()) {
        mMetricsThread->join();
        LOG_I("[CCAEReporter] CCAE metrics ends successfully.");
    }
}

int32_t CCAEReporter::Init()
{
    try {
        mCCAERegisterClient = std::make_shared<HttpClient>();
        mCCAEInventoriesClient = std::make_shared<HttpClient>();
        mCoordinatorClient = std::make_shared<HttpClient>();
        if (mCoordinatorClient == nullptr ||
            mCoordinatorClient->Init("", "", ControllerConfig::GetInstance()->GetExternalCoordinatorTlsItems()) != 0) {
            LOG_E("[CCAEReporter] Initialize ccae reporter failed because create coordinator client failed.");
            return -1;
        }

        StartMetricsThread();
    } catch (const std::exception& e) {
        LOG_E("[CCAEReporter] Failed to create thread for CCAE Reporter. Error: %s.", e.what());
        return -1;
    }
    return 0;
}

uint64_t GetCurrentTimeInMilliSec()
{
    auto now = std::chrono::system_clock::now();
    uint64_t currTime = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return currTime;
}

bool CCAEReporter::ReachUpdateTime(std::string modelID)
{
    if (mMemTimeStamp.at(modelID) == 0 || mMetricsPeriods.at(modelID) == 0) {
        return false;
    }

    auto currTime = GetCurrentTimeInMilliSec();
    uint64_t updateDuration = ONE_SEC_IN_MILLISEC / mMetricsPeriods.at(modelID);
    bool needUpdate = (currTime - mMemTimeStamp.at(modelID)) >= updateDuration;
    if (needUpdate) {
        LOG_D("[CCAEReporter] Update memoried time stamp.");
        mMemTimeStamp.at(modelID) = currTime;
    }
    return needUpdate;
}

bool CCAEReporter::UpdateModelID2Report()
{
    auto ctrlInst = ControllerConfig::GetInstance();
    for (size_t i = 0; i < ctrlInst->GetModelNum(); ++i) {
        const char* envModelID = std::getenv("MODEL_ID");
        std::string modelID = (envModelID != nullptr) ? envModelID : ctrlInst->GetModelID(i);
        auto newMetricPeriod = static_cast<uint64_t>(mCCAEStatus->GetMetricPeriod(modelID));
        auto isForcedUpdate = true;
        bool modelIDIsInStack = std::find(mModelIDStack.begin(), mModelIDStack.end(), modelID) != mModelIDStack.end();

        // put model id in stack when forced to update
        if (isForcedUpdate) {
            if (!modelIDIsInStack) {
                mModelIDStack.emplace_back(modelID);
                LOG_D("[CCAEReporter] Forcing update, "
                    "current need to update %d models.", mModelIDStack.size());
                modelIDIsInStack = true;
            }
        }

        // put model id in stack when miss metrics period or received a new one
        if (mMetricsPeriods.find(modelID) == mMetricsPeriods.end() ||
            mMetricsPeriods.at(modelID) != newMetricPeriod) {
            mMetricsPeriods[modelID] = newMetricPeriod;
            mMemTimeStamp[modelID] = newMetricPeriod == 0 ? 0 : GetCurrentTimeInMilliSec();
            if (newMetricPeriod != 0 && !modelIDIsInStack) {
                mModelIDStack.emplace_back(modelID);
                LOG_D("[CCAEReporter] Receive new metric period, "
                    "current need to update %d models.", mModelIDStack.size());
                modelIDIsInStack = true;
            }
        }

        // put model id in stack when it is time to update
        if (mMemTimeStamp.find(modelID) != mMemTimeStamp.end() && ReachUpdateTime(modelID)) {
            if (!modelIDIsInStack) {
                mModelIDStack.emplace_back(modelID);
                LOG_D("[CCAEReporter] Reach limit of the inventories' request duration, "
                    "current need to update %d models.", mModelIDStack.size());
                modelIDIsInStack = true;
            }
        }
    }
    bool needReport = mModelIDStack.size() > 0;
    return needReport;
}

void CCAEReporter::ResetReportStack()
{
    LOG_D("[CCAEReporter] Reset status that checks if any model needs to report.");
    for (const auto &modelID : mModelIDStack) {
        if (mCCAEStatus->ISForcedUpdate(modelID)) {
            mCCAEStatus->SetForcedUpdate(modelID, false);
        }
    }
    mModelIDStack.clear();
}

void CCAEReporter::StartRegisterThread()
{
    try {
        mRegisterThread = std::make_unique<std::thread>([this]() {
            LOG_I("[CCAEReporter] Start CCAE register thread.");
            while (mRun.load()) {
                LOG_D("[CCAEReporter] RUNNING.");
                if (SendRegister() != -1) {
                    mMetricCanStart = true;
                    LOG_D("[CCAEReporter] Received from CCAE register server correctly.");
                }
                std::this_thread::sleep_for(std::chrono::seconds(5)); // check register port every 5 sec
            }
            LOG_I("[CCAEReporter] End CCAE register thread.");
        });
    } catch (const std::exception& e) {
        LOG_E("[CCAEReporter] Failed to create register thread for CCAE Reporter. Error: %s.", e.what());
    }
}

void CCAEReporter::StartMetricsThread()
{
    try {
        mMetricsThread = std::make_unique<std::thread>([this]() {
            while (mRun.load()) {
                if (ControllerConfig::GetInstance()->IsLeader() && UpdateModelID2Report() && SendInventories() != -1) {
                    LOG_D("[CCAEReporter] Received from CCAE inventory server correctly.");
                }
                std::this_thread::sleep_for(std::chrono::seconds(COUNT_SEC));
            }
        });
    } catch (const std::exception& e) {
        LOG_E("[CCAEReporter] Failed to create metrics thread for CCAE Reporter. Error: %s.", e.what());
    }
}

int32_t CCAEReporter::SendRegister()
{
    auto ret = CCAERequestHandler::GetInstance()->SendRegister2UpdateStatus(*mCCAERegisterClient, *mCCAEStatus);
    return ret;
}

void CCAEReporter::SelectMasterCoordinator(std::vector<std::unique_ptr<Coordinator>> &selectedCoordinators) const
{
    // Get current list of coordinators
    std::vector<std::unique_ptr<Coordinator>> coordinatorNodes;
    try {
        coordinatorNodes = mCoordinatorStoreWithMasterInfo->GetCoordinators();
    } catch (const std::exception& e) {
        LOG_E("[%s] [CCAEReporter] Failed to get coordinator node, error is %s.",
            GetErrorCode(ErrorType::NOT_FOUND, ControllerFeature::CONTROLLER).c_str(), e.what());
        return;
    }

    // If there is only one coordinator node available, select it directly.
    if (coordinatorNodes.size() == size_t(1)) {
        selectedCoordinators.push_back(std::move(coordinatorNodes[0]));
        LOG_I("[CCAEReporter] Selected the only available coordinator node.");
        return;
    }

    // Check if there is exactly one master coordinator in the current list
    int masterCount = 0;
    int masterIdx = -1;
    int lastMasterIdx = -1;

    for (size_t idx = 0; idx < coordinatorNodes.size(); ++idx) {
        const auto& coord = coordinatorNodes[idx];
        if (coord->isMaster) {
            masterCount += 1;
            masterIdx = idx;
        }
        std::string currentKey = coord->ip + ":" + coord->port;
        if (mLastMasterKey == currentKey) {
            lastMasterIdx = idx;
        }
    }

    // If exactly one master node exists, use it and update mLastMasterKey
    if (masterCount == 1) {
        // Update mLastMasterKey
        mLastMasterKey = coordinatorNodes[masterIdx]->ip + ":" + coordinatorNodes[masterIdx]->port;
        LOG_D("[CCAEReporter] Updated last master key: %s.", mLastMasterKey.c_str());
        selectedCoordinators.push_back(std::move(coordinatorNodes[masterIdx]));
        LOG_I("[CCAEReporter] Selected unique master coordinator: %s.", mLastMasterKey.c_str());
    } else {
        // No unique master, check if mLastMasterKey exists
        if (lastMasterIdx != -1) {
            // Reuse last known master
            selectedCoordinators.push_back(std::move(coordinatorNodes[lastMasterIdx]));
            LOG_I("[CCAEReporter] Selected previous master: %s.", mLastMasterKey.c_str());
        }
    }
}

std::string CCAEReporter::GetMetricsInfo()
{
    std::vector<std::unique_ptr<Coordinator>> selectedCoordinators;
    SelectMasterCoordinator(selectedCoordinators);
    // If no valid master found, log Error
    if (selectedCoordinators.empty()) {
        LOG_W("[%s][CCAEReporter] No valid master coordinator found.",
            GetWarnCode(ErrorType::WARNING, ControllerFeature::CONTROLLER).c_str());
        return "";
    }

    std::string response;
    CoordinatorRequestHandler::GetInstance()->SendMetricsRequest(
        mCoordinatorClient, mCoordinatorStoreWithMasterInfo, selectedCoordinators, response);
    
    return response;
}

int32_t CCAEReporter::SendInventories()
{
    std::string response = GetMetricsInfo();
    std::string jsonString =
        CCAERequestHandler::GetInstance()->FillInventoryRequest(mModelIDStack, mCCAEStatus,
            mNodeStatus, response);
    bool writeResult = shm_writer.Write(jsonString);
    if (writeResult) {
        LOG_D("[CCAEReporter] write shm success");
        return 0;
    } else {
        LOG_E("[CCAEReporter] write shm failed");
        return -1;
    }
}
}