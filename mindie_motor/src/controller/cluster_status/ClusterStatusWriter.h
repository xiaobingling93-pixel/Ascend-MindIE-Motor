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
#ifndef MINDIE_MS_CONTROLLER_CLUSTER_STATUS_WRITER_H
#define MINDIE_MS_CONTROLLER_CLUSTER_STATUS_WRITER_H
#include <atomic>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <thread>
#include <nlohmann/json.hpp>
#include <sys/types.h>
#include "Logger.h"
#include "NodeStatus.h"
#include "CoordinatorStore.h"
namespace MINDIE::MS {

/// The writer of cluster status.
///
/// This class outputs the cluster status to a file, including server and coordinator status.
class ClusterStatusWriter {
public:
    /// Get the instance of the ClusterStatusWriter class.
    ///
    /// \return The reference to the object.
    static ClusterStatusWriter *GetInstance()
    {
        static ClusterStatusWriter singleton;
        return &singleton;
    }

    /// Initialize the ClusterStatusWriter object.
    ///
    /// If the to_file config of cluster_status is false, the function ends directly.
    /// If the to_file config of cluster_status is true, a thread will be created. This allows the ClusterStatusWriter
    /// object to periodically output the server status and coordinator status in the format of a JSON file.
    /// \param nodeStatus The shared pointer of nodeStatus which is globally managed in the controller.
    /// \param coordinatorStore The shared pointer of coordinatorStore which is globally managed in the controller.
    /// \return The result of the initialization. 0 indicates success. -1 indicates failure.
    int32_t Init(const std::shared_ptr<NodeStatus> &nodeStatus,
        const std::shared_ptr<CoordinatorStore> &coordinatorStore);

    /// Stop the thread that is created using the Init method.
    void Stop();

    ClusterStatusWriter() = default;

    /// Destructor.
    ///
    /// This destructor calls the Stop method to stop the thread.
    ~ClusterStatusWriter()
    {
        Stop();
    };
    ClusterStatusWriter(const ClusterStatusWriter &obj) = delete;
    ClusterStatusWriter &operator=(const ClusterStatusWriter &obj) = delete;
    ClusterStatusWriter(ClusterStatusWriter &&obj) = delete;
    ClusterStatusWriter &operator=(ClusterStatusWriter &&obj) = delete;
private:

    /// Save the cluster status to a file, including server and coordinator status.
    ///
    /// \param nodeStatus The shared pointer of nodeStatus which is globally managed in the controller.
    /// \param coordinatorStore The shared pointer of coordinatorStore which is globally managed in the controller.
    /// \return The result of the function. 0 indicates success. -1 indicates failure.
    int32_t SaveStatusToFile(const std::shared_ptr<NodeStatus> &nodeStatus,
        const std::shared_ptr<CoordinatorStore> &coordinatorStore) const;

    /// Generate a JSON object according to current server status and coordinator status.
    ///
    /// \param nodeStatus The shared pointer of nodeStatus which is globally managed in the controller.
    /// \param coordinatorStore The shared pointer of coordinatorStore which is globally managed in the controller.
    /// \return The result of the function. 0 indicates success. -1 indicates failure.
    nlohmann::json GenerateClusterInfo(const std::shared_ptr<NodeStatus> &nodeStatus,
        const std::shared_ptr<CoordinatorStore> &coordinatorStore) const;
    void GenerateServerInfo(const std::shared_ptr<NodeStatus> &nodeStatus,
        nlohmann::json &file) const;
    void GenerateCoordinatorInfo(const std::shared_ptr<CoordinatorStore> &coordinatorStore,
        nlohmann::json &file) const;
    void GenerateSingleServerInfo(const std::shared_ptr<NodeStatus> &nodeStatus, nlohmann::json &node,
        const NodeInfo &nodeInfo, bool isFaulty) const;
    void Wait();
    std::atomic<bool> mRun = { true };                   /// The running flag of the while loop in the thread.
    std::atomic<uint32_t> mWaitSeconds = { 0 };          /// The time interval for executing the GenerateClusterInfo.
    std::unique_ptr<std::thread> mMainThread = nullptr;  /// The thread that dumps cluster status to a JSON file.
};
}
#endif // MINDIE_MS_CONTROLLER_CLUSTER_STATUS_WRITER_H
