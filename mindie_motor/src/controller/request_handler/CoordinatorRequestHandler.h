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
#ifndef MINDIE_MS_CONTROLLER_COORDINATORREQUESTHANDLER_H
#define MINDIE_MS_CONTROLLER_COORDINATORREQUESTHANDLER_H

#include <memory>
#include "NodeStatus.h"
#include "HttpClient.h"
#include "CoordinatorStore.h"
#include "Logger.h"
#include "ControllerConfig.h"
namespace MINDIE::MS {

/// The class that sends HTTP requests to the coordinator.
///
/// This class sends HTTP requests to the coordinator to synchronize usable servers for inference.
class CoordinatorRequestHandler {
public:

    /// Get the instance of the CoordinatorRequestHandler class.
    ///
    /// \return The reference to the object.
    static CoordinatorRequestHandler *GetInstance()
    {
        static CoordinatorRequestHandler instance;
        return &instance;
    }

    /// Synchronize usable servers for inference.
    ///
    /// \param client The HTTP client.
    /// \param nodeStatus The shared pointer of nodeStatus which is globally managed in the controller.
    /// \param coordinatorStore The shared pointer of coordinatorStore which is globally managed in the controller.
    void SendNodeStatus(std::shared_ptr<HttpClient> client, std::shared_ptr<NodeStatus> nodeStatus,
        std::shared_ptr<CoordinatorStore> coordinatorStore) const;
    void SendMetricsRequest(std::shared_ptr<HttpClient> client, std::shared_ptr<CoordinatorStore> coordinatorStore,
        std::vector<std::unique_ptr<Coordinator>> &coordinatorNodes, std::string &response) const;

    /// Set the running state of synchronization.
    ///
    /// If turned off, the SendNodeStatus method will not send the HTTP request.
    /// \param run The running state.
    void SetRunStatus(bool run);
    CoordinatorRequestHandler(const CoordinatorRequestHandler &obj) = delete;
    CoordinatorRequestHandler &operator=(const CoordinatorRequestHandler &obj) = delete;
    CoordinatorRequestHandler(CoordinatorRequestHandler &&obj) = delete;
    CoordinatorRequestHandler &operator=(CoordinatorRequestHandler &&obj) = delete;
private:
    std::string GenerateNodeStatusStr(std::shared_ptr<NodeStatus> nodeStatus,
        DeployMode deployMode, bool printInfoLog) const;
    bool SendNodeStatus2SingleCoordinatorNode(std::shared_ptr<HttpClient> client,
        std::shared_ptr<NodeStatus> nodeStatus, std::shared_ptr<CoordinatorStore> coordinatorStore,
        const std::unique_ptr<Coordinator>& node, bool printInfoLog) const;
    bool ShouldPrintInfoLogWhenSendNodeStatus() const;
    std::atomic<bool> mRun = { false };      /// The running state of synchronization.
    mutable std::atomic<uint64_t> mSendStatusRoundCounter = {0};
    mutable std::atomic<uint64_t> mLastPDNodeCounter = {0};
    CoordinatorRequestHandler() = default;
    ~CoordinatorRequestHandler() = default;
};
}

#endif // MINDIE_MS_CONTROLLER_COORDINATORREQUESTHANDLER_H
