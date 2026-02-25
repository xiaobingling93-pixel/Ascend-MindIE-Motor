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
#include "ProbeServer.h"
#include "Logger.h"
#include "ConfigParams.h"
#include "ControllerConfig.h"
namespace MINDIE::MS {
int32_t ProbeServer::Init()
{
    try {
        mServer = std::make_unique<HttpServer>(1, 10); // 1个线程，最大支持10个并发链接
    } catch (const std::exception& e) {
        LOG_E("[%s] [ProbeServer] Create http server failed, initialize prober server failed.",
            GetErrorCode(ErrorType::RESOURCE_EXHAUSTED, ControllerFeature::PROBE_SERVER).c_str());
        return -1;
    }
    std::string startupUrl = "/v1/startup";
    std::string healthUrl = "/v1/health";
    if ((mServer->RegisterGetUrlHandler(startupUrl, std::bind(&ProbeServer::GetStartupHandler,
        this, std::placeholders::_1)) != 0) ||
        (mServer->RegisterGetUrlHandler(healthUrl, std::bind(&ProbeServer::GetHealthHandler,
        this, std::placeholders::_1)) != 0)) {
        LOG_E("[%s] [ProbeServer] Failed to register the URL handler, initialize prober server failed.",
            GetErrorCode(ErrorType::OPERATION_REPEAT, ControllerFeature::PROBE_SERVER).c_str());
        return -1;
    }
    try {
        mMainThread = std::make_unique<std::thread>([this]() {
            if (Run() != 0) {
                LOG_E("[%s] [ProbeServer] Probe server run failed.",
                    GetErrorCode(ErrorType::CALL_ERROR, ControllerFeature::PROBE_SERVER).c_str());
            }
            mRun.store(false);
        });
    } catch (const std::exception& e) {
        LOG_E("[%s] [ProbeServer] Failed to create main thread.",
            GetErrorCode(ErrorType::RESOURCE_EXHAUSTED, ControllerFeature::PROBE_SERVER).c_str());
        return -1;
    }
    std::this_thread::sleep_for(std::chrono::seconds(2)); // 等待2s检查是否初始化成功
    if (!mRun.load()) {
        LOG_E("[%s] [ProbeServer] Main thread run failed, initialize prober server failed.",
            GetErrorCode(ErrorType::CALL_ERROR, ControllerFeature::PROBE_SERVER).c_str());
        return -1;
    }
    return 0;
}

ProbeServer::~ProbeServer()
{
    Stop();
    LOG_I("[ProbeServer] Probe Server destroy successfully.");
}

void ProbeServer::Stop()
{
    if (mServer != nullptr) {
        mServer->Stop();
    }
    if (mMainThread != nullptr && mMainThread->joinable()) {
        mMainThread->join();
    }
    LOG_I("[ProbeServer] Stop successfully.");
};

int32_t ProbeServer::Run()
{
    HttpServerParams serverParams;
    serverParams.ip = ControllerConfig::GetInstance()->GetPodIP();
    serverParams.port = ControllerConfig::GetInstance()->GetPort();
    serverParams.serverTlsItems = ControllerConfig::GetInstance()->GetHttpServerTlsItems();
    serverParams.checkSubject = false;
    LOG_M("[Start] Runing probe server, listening on IP %s, port %ld, TLS enable %d, check subject %d.",
        serverParams.ip.c_str(), serverParams.port, serverParams.serverTlsItems.tlsEnable, serverParams.checkSubject);
    if (mServer->Run(serverParams) != 0) {
        LOG_E("[%s] [ProbeServer] Failed to run the http server",
            GetErrorCode(ErrorType::CALL_ERROR, ControllerFeature::PROBE_SERVER).c_str());
        return -1;
    }
    return 0;
}

std::pair<ErrorCode, Response> ProbeServer::GetStartupHandler([[maybe_unused]]const Http::request<Http::string_body>
    &req) const
{
    Response resp;
    return std::make_pair(ErrorCode::OK, resp);
}

std::pair<ErrorCode, Response> ProbeServer::GetHealthHandler([[maybe_unused]]const Http::request<Http::string_body>
    &req) const
{
    Response resp;
    return std::make_pair(ErrorCode::OK, resp);
}
}