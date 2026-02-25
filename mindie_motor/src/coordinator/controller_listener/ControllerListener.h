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
#ifndef MINDIE_MS_COORDINATOR_CONTROLLER_H
#define MINDIE_MS_COORDINATOR_CONTROLLER_H

#include <mutex>
#include <shared_mutex>
#include <memory>
#include <chrono>
#include "ServerConnection.h"
#include "BaseScheduler.h"
#include "boost/uuid/uuid_generators.hpp"
#include "HttpClientAsync.h"
#include "ConnectionPool.h"
#include "ClusterNodes.h"
#include "RequestMgr.h"
#include "RequestRepeater.h"
#include "IPCConfig.h"
#include "SharedMemoryUtils.h"
#include "AlarmRequestHandler.h"

namespace MINDIE::MS {
using namespace std::chrono;
/// The class that define interface of management port.
///
/// The class that define interface of management port.
class ControllerListener {
public:
    ControllerListener(std::unique_ptr<MINDIE::MS::DIGSScheduler>& schedulerInit,
        std::unique_ptr<ClusterNodes>& instancesRec, std::unique_ptr<RequestRepeater>& requestRepeater1,
        std::unique_ptr<ReqManage>& reqManageInit, std::unique_ptr<ExceptionMonitor>& exceptionMonitor);
    ~ControllerListener(){};
    int Init();
   
    // 管理面
    // 同步实例信息
    /// Request /v1/instances/refresh handler.
    ///
    /// If coordinator receive the request /v1/instances/refresh, this function will be called.
    /// Refresh instances info.
    ///
    /// \param connection Request tcp connection.
    void InstancesRefreshHandler(std::shared_ptr<ServerConnection> connection);
    // 获取调度信息
    /// Request /v1/coordinator_info handler.
    ///
    /// If coordinator receive the request /v1/coordinator_info, this function will be called.
    /// Return scheduler info.
    ///
    /// \param connection Request tcp connection.
    void CoordinatorInfoHandler(std::shared_ptr<ServerConnection> connection);
    // 实例断流
    /// Request /v1/instances/offline handler.
    ///
    /// If coordinator receive the request /v1/instances/offline, this function will be called.
    /// Close the specified instances.
    ///
    /// \param connection Request tcp connection.
    void InstancesOfflineHandler(std::shared_ptr<ServerConnection> connection);
    // 实例断流恢复
    /// Request /v1/instances/online handler.
    ///
    /// If coordinator receive the request /v1/instances/online, this function will be called.
    /// Activate the specified instances.
    ///
    /// \param connection Request tcp connection.
    void InstancesOnlineHandler(std::shared_ptr<ServerConnection> connection);
    // 查询实例任务数
    /// Request /v1/instances/tasks handler.
    ///
    /// If coordinator receive the request /v1/instances/tasks, this function will be called.
    /// Return the tasks numbers of specified instances.
    ///
    /// \param connection Request tcp connection.
    void InstancesTasksHandler(std::shared_ptr<ServerConnection> connection);
    // 查询PD节点之间任务是否结束
    /// Request /v1/instances/query_tasks handler.
    ///
    /// If coordinator receive the request /v1/instances/query_tasks, this function will be called.
    /// Return whether the tasks between the specified instances ends.
    ///
    /// \param connection Request tcp connection.
    void InstancesQueryTasksHandler(std::shared_ptr<ServerConnection> connection);

    // 探针
    // 启动探针
    /// Request /v1/startup handler.
    ///
    /// If coordinator receive the request /v1/startup, this function will be called.
    /// If coordinator start up, return http code 200.
    ///
    /// \param connection Request tcp connection.
    void StartUpProbeHandler(std::shared_ptr<ServerConnection> connection) const;
    // 就绪探针
    /// Request /v2/health/ready handler.
    ///
    /// If coordinator receive the request /v2/health/ready, this function will be called.
    /// If coordinator is ready, return http code 200, else return http code 503.
    ///
    /// \param connection Request tcp connection.
    void ReadinessProbeHandler(std::shared_ptr<ServerConnection> connection);
        // 就绪探针
    /// Request /v1/readiness.
    ///
    /// If coordinator receive the request /v1/readiness, this function will be called.
    /// If coordinator is ready(normal and master), return http code 200, else return http code 503.
    ///
    /// \param connection Request tcp connection.
    void CoordinatorReadinessProbeHandler(std::shared_ptr<ServerConnection> connection);
    // 健康探针
    /// Request /v1/health or /v2/health/live handler.
    ///
    /// If coordinator receive the request /v1/health or /v2/health/live, this function will be called.
    /// If coordinator is alive, return http code 200.
    ///
    /// \param connection Request tcp connection.
    void HealthProbeHandler(std::shared_ptr<ServerConnection> connection) const;

    // Triton健康检查
    /// Request /v2/models/{MODEL_NAME}/ready handler.
    ///
    /// If coordinator receive the request /v2/models/{MODEL_NAME}/ready, this function will be called.
    /// If coordinator is ready, return http code 200, else return http code 503.
    ///
    /// \param connection Request tcp connection.
    void TritonModelsReadyHandler(std::shared_ptr<ServerConnection> connection);

    void GetControllerIP(std::shared_ptr<ServerConnection> connection);

    void AbnormalStatusHandler(std::shared_ptr<ServerConnection> connection);

    void RecvsUpdater(std::shared_ptr<ServerConnection> connection);

    void Master2Worker();

    std::shared_ptr<std::atomic<bool>> GetDataReady()
    {
        return dataReady;
    }

private:
    std::atomic<size_t> inputLen;
    std::atomic<size_t> outputLen;

    std::atomic<bool> inCoordReadyAlarmStateCoordException = false;
    std::atomic<bool> inCoordReadyAlarmStateInstanceMiss = false;
    std::atomic<uint32_t> reportCounter = 0;
    std::atomic<bool> lastStatus = true; // Since the initialization state is usually false.
    std::unique_ptr<MINDIE::MS::DIGSScheduler>& scheduler;

    // 需要更新集群信息
    std::unique_ptr<ClusterNodes>& instancesRecord;

    // 请求转发器 是因为linkDNode引入，关注该动作是否可以挪走
    std::unique_ptr<RequestRepeater>& requestRepeater;

    std::unique_ptr<ReqManage>& reqManage;
    std::unique_ptr<ExceptionMonitor>& exceptionMonitor;
    std::atomic<bool> init = false;
    std::shared_ptr<std::atomic<bool>> dataReady = nullptr;
    std::shared_mutex refreshMtx;
    struct HttpParam {
        HttpParam(const std::string &ipInit, const std::string &portInit, const std::string &metricPortInit,
            const std::string &interCommPortInit) : ip(ipInit), port(portInit), metricPort(metricPortInit),
            interCommPort(interCommPortInit) {}
        std::string ip;
        std::string port;
        std::string metricPort;
        std::string interCommPort;
    };
    int32_t IdsTraverse(const std::vector<uint64_t> &ids, const nlohmann::json &instances);
    void IdsAddOrUpdate(const std::vector<uint64_t> &addVec, const std::vector<uint64_t> &updateVec,
        const nlohmann::json::const_iterator &it, uint64_t id);
    int32_t CheckInstance(const MINDIE::MS::DIGSInstanceStaticInfo &newInstance) const;
    int32_t ParseInstance(const nlohmann::json::const_iterator &it, MINDIE::MS::DIGSInstanceStaticInfo &newInstance,
        std::string &ip, std::string &port, std::string &modelName) const;
    int32_t AddIns(uint64_t id, const HttpParam &httpParam, const MINDIE::MS::DIGSInstanceStaticInfo &newInstance,
        const std::string &modelName, const nlohmann::json::const_iterator &it);
    int32_t Add(const nlohmann::json::const_iterator &it);
    int32_t AddInsWithoutBackup(const nlohmann::json::const_iterator &it,
        MINDIE::MS::DIGSInstanceStaticInfo newInstance, HttpParam &httpParam);
    int32_t Update(const nlohmann::json::const_iterator &it);
    void Remove(const std::vector<uint64_t> &removeVec);
    std::vector<std::pair<std::string, std::string>> ParseQuery(const std::string& url) const;
    bool InstancesQueryTasksCheck(std::unordered_set<std::string> tasksOfPeerIns, uint64_t idOfInsToChangRole,
                                  uint64_t routeIndex);
    void InstancesQueryTasksProc(std::shared_ptr<ServerConnection> connection, uint64_t pId, uint64_t dId,
                                 MINDIE::MS::DIGSRoleChangeType roleChangeType);
    void CheckAndHandleCoordinatorStateAlarm();
    void CloseConnection(std::string ip, std::string port);
    void AbnormalStatusUpdate(bool abnormal, bool master);
    std::atomic<bool> masterCheck = { true };
    void StopMasterCheck();
};

}
#endif