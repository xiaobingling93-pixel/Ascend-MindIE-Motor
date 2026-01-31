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

#ifndef MINDIE_MS_CLUSTERCLIENT_H
#define MINDIE_MS_CLUSTERCLIENT_H
#include <string>
#include <atomic>
#include <thread>
#include <grpcpp/security/tls_credentials_options.h>
#include <src/core/lib/security/credentials/tls/grpc_tls_credentials_options.h>
#include "grpcpp/grpcpp.h"
#include "grpc_proto/recover_mindie.pb.h"
#include "grpc_proto/recover_mindie.grpc.pb.h"
#include "grpc_proto/cluster_fault.pb.h"
#include "grpc_proto/cluster_fault.grpc.pb.h"
#include "NodeStatus.h"
#include "AlarmConfig.h"

namespace MINDIE::MS {

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using RankTableCallback = std::function<void(std::string &response)>;
using FaultMsgCallback = std::function<void(fault::FaultMsgSignal &nodeFaultInfo)>;
constexpr uint32_t MAX_RETRY_TIMES =
#ifdef UT_FLAG
    3;
#else
    300;
#endif // UT_FLAG
constexpr uint32_t REGISTER_MAX_RETRY_TIMES =
#ifdef UT_FLAG
    3;
#else
    60;
#endif

void SaveRankTableCallback(std::string &response);
void DealwithFaultMsgCallback(fault::FaultMsgSignal &response);

class ClusterClient {
public:
    // Get the instance of the ClusterClient class.
    static ClusterClient* GetInstance();
    // Initialize the Grpc Client object.
    int32_t Init();
    // This method is used to register the client information.
    int32_t Register();
    // This method is used to create dealwith thread.
    void CreatDealwithThread();
    // This method is used to find fault node by fault message.
    void AddFaultNodeByNodeIp(fault::NodeFaultInfo& nodeInfo);
    // This method is used to subscribe the rank table.
    int32_t SubscribeRankTable(const RankTableCallback &callback);
    // This method is used to process fault message.
    int32_t SubscribeFaultMsgSignal(const FaultMsgCallback &callback);
    // start the client.
    int32_t Start(std::shared_ptr<NodeStatus> nodeStatus);

    void SetWaitClusterDGRTSave(std::atomic<bool>* waitClusterDGRTSave)
    {
        mWaitClusterDGRTSave = waitClusterDGRTSave;
    }

#ifdef UT_FLAG
    void SetNodeStatus(std::shared_ptr<NodeStatus> mock);
    void Reset();
#endif // UT_FLAG

private:
    std::shared_ptr<NodeStatus> mNodeStatus = nullptr;
    std::unique_ptr<config::Config::Stub> mStub;
    std::shared_ptr<grpc::Channel> mGrpcChannel;
    std::atomic<bool> mIsInit = { false };
    std::atomic<bool> mIsRegister = { false };
    std::atomic<bool> mFaultStop = { false };
    std::atomic<bool> mRankTableStop = { false };
    TlsItems mTlsConfig;
    config::ClientInfo mRequest;
    fault::ClientInfo  mFaultInfo;
    std::unique_ptr<std::thread> mClientThread;
    std::unique_ptr<std::thread> mRankTableThread = nullptr;
    std::unique_ptr<std::thread> mFaultThread  = nullptr;
    ClusterClient() = default;
    ~ClusterClient() = default;
    std::shared_ptr<grpc::Channel> CreatGrpcChannel(std::string clusterIp) const;
    void PrintFaultSignal(const fault::FaultMsgSignal& signal) const;
    // 添加告警状态跟踪
    std::atomic<bool> mRankTableAlarmReported{false};    // RankTable是否已上报告警
    std::atomic<bool> mFaultMsgAlarmReported{false};     // FaultMsg是否已上报告警
    std::atomic<bool> mConnectionAlarmReported{false};   // 连接中断是否已上报告警
    void ReportAlarm(ClusterConnectionReason reason);
    void ClearAlarm(ClusterConnectionReason reason);
    bool RegisterRankTable(std::unique_ptr<config::Config::Stub>& configStub);
    bool RegisterFaultMsg(std::unique_ptr<fault::Fault::Stub>& faultStub);
    // 被ClusterClient类和NodeScheduler类共享，用于同步初始化时序，优先使用ClusterD传输过来的global ranktable，
    // 再使用deploy_acjob.py生成的global ranktable，在第一次保存ClusterD传输过来的global ranktable后，置为true
    std::atomic<bool>* mWaitClusterDGRTSave;
};
}
#endif // MINDIE_MS_CLUSTERCLIENT_H