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
#ifndef MINDIE_MS_COORDINATOR_H
#define MINDIE_MS_COORDINATOR_H

#include "HttpServer.h"
#include "HttpClientAsync.h"
#include "RequestRepeater.h"
#include "BaseScheduler.h"
#include "RequestListener.h"
#include "ControllerListener.h"
#include "ConfigParams.h"
#include "RequestMonitor.h"
#include "MetricListener.h"
#include "HeartbeatProducer.h"

namespace MINDIE::MS {
class Coordinator {
public:
    Coordinator();
    Coordinator(const Coordinator& other) = delete;
    Coordinator& operator=(const Coordinator& other) = delete;
    Coordinator(Coordinator&& other) = delete;
    Coordinator& operator=(Coordinator&& other) = delete;
    ~Coordinator();
    int32_t Run();

private:
    void InitGlobleInfoStore();
    // wait and listen msg from controller
    HttpServerParm InitControllerListener(std::unique_ptr<ControllerListener>& statusListener);
    HttpServerParm InitStatusListener(std::unique_ptr<ControllerListener>& controlListener);
    HttpServerParm InitExternalListener();

    // wait and listen msg from predictor
    HttpServerParm InitRequestListener();

    // init scheduler to pick execute node for request
    int32_t InitScheduler();

    // init repeater to send request and rcv result from mindie server
    int32_t InitRepeater();

    void InitExceptionMonitor();

    void InitLeader();

    int32_t StartManagerServer();

    // 数据面http server
    HttpServer dataHttpServer;
    // 管理面http server
    HttpServer managerHttpServer;
    HttpServer externalHttpServer;
    HttpServer statusHttpServer;

    // 管理面数据监听
    std::unique_ptr<ControllerListener> controllerListener;
    // 推理请求监听
    std::unique_ptr<RequestListener> requestListener;
    // 服务化监控指标监听
    std::unique_ptr<MetricListener> metricListener;
    // 推理请求调度
    std::unique_ptr<MINDIE::MS::DIGSScheduler> scheduler;
    // 推理请求调度后转发
    std::unique_ptr<RequestRepeater> requestRepeater;

    // 一些全局信息管理
    std::unique_ptr<ClusterNodes> instancesRecord; // 全局集群信息
    std::unique_ptr<ReqManage> reqManager;  // 请求状态管理监控等
    std::unique_ptr<ExceptionMonitor> exceptionMonitor; // 异常监控
    std::unique_ptr<PerfMonitor> perfMonitor; // 性能监控
    std::unique_ptr<RequestMonitor> timeoutMonitor; // 超时监控

    std::unique_ptr<std::thread> managerThread = nullptr;
    std::unique_ptr<std::thread> externalThread = nullptr;
    std::unique_ptr<std::thread> statusThread = nullptr;

    std::unique_ptr<MINDIE::MS::HeartbeatProducer> m_heartbeatProducer; // coordinator心跳
};
}
#endif