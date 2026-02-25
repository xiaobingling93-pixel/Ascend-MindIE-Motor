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
#include <string>
#include "boost/uuid/uuid_io.hpp"
#include "Logger.h"
#include "nlohmann/json.hpp"
#include "Configure.h"
#include "Communication.h"
#include "Communication.h"
#include "RequestRepeater.h"

namespace MINDIE::MS {

RequestRepeater::RequestRepeater(std::unique_ptr<MINDIE::MS::DIGSScheduler> &schedulerInit,
    std::unique_ptr<ReqManage>& reqManager, std::unique_ptr<ClusterNodes>& instancesRec,
    std::unique_ptr<ExceptionMonitor> &exceptionMonitorInit)
    : scheduler(schedulerInit), reqManage(reqManager), instancesRecord(instancesRec),
      exceptionMonitor(exceptionMonitorInit)
{
    // 链接池创建
    connectionPool = std::make_unique<ConnectionPool>(httpClient);
    // 向mindie-server发送计算token的请求成功
    tokenizerHandler.RegisterFun(ClientHandlerType::REQ,
        std::bind(&RequestRepeater::TokenizerReqHandler, this, std::placeholders::_1));
    // 向mindie-server发送计算token的请求失败
    tokenizerHandler.RegisterFun(ClientHandlerType::REQ_ERROR,
        std::bind(&RequestRepeater::TokenizerReqErrHandler, this, std::placeholders::_1));
    // 从mindie-server读取计算token的结果成功
    tokenizerHandler.RegisterFun(ClientHandlerType::RES,
        std::bind(&RequestRepeater::TokenizerResHandler, this, std::placeholders::_1));
    // 从mindie-server读取计算token的结果失败
    tokenizerHandler.RegisterFun(ClientHandlerType::HEADER_RES_ERROR,
        std::bind(&RequestRepeater::TokenizerResErrHandler, this, std::placeholders::_1));
    if (Configure::Singleton()->schedulerConfig["deploy_mode"] == "pd_separate" ||
        Configure::Singleton()->schedulerConfig["deploy_mode"] == "pd_disaggregation" ||
        Configure::Singleton()->schedulerConfig["deploy_mode"] == "pd_disaggregation_single_container") {
        // 向P实例发送请求成功
        pHandler.RegisterFun(ClientHandlerType::REQ,
            std::bind(&RequestRepeater::PSendHandler, this, std::placeholders::_1));
        // 向P实例发送请求失败
        pHandler.RegisterFun(ClientHandlerType::REQ_ERROR,
            std::bind(&RequestRepeater::PSendErrorHandler, this, std::placeholders::_1));
        // 从P实例读到响应成功
        pHandler.RegisterFun(ClientHandlerType::RES,
            std::bind(&RequestRepeater::PResHandler, this, std::placeholders::_1));
        // 从P实例读响应失败
        pHandler.RegisterFun(ClientHandlerType::HEADER_RES_ERROR,
            std::bind(&RequestRepeater::PResErrorHandler, this, std::placeholders::_1));

        // 向D实例发送请求失败
        dHandler.RegisterFun(ClientHandlerType::REQ_ERROR,
            std::bind(&RequestRepeater::DSendErrorHandler, this, std::placeholders::_1));
        // 从D实例读HEADER RES失败
        dHandler.RegisterFun(ClientHandlerType::HEADER_RES_ERROR,
            std::bind(&RequestRepeater::DResErrorHandler, this, std::placeholders::_1));
        // 从D实例读到流式响应成功（每收到一个chunk包触发一次）
        dHandler.RegisterFun(ClientHandlerType::CHUNK_BODY_RES,
            std::bind(&RequestRepeater::DResChunkHandler, this, std::placeholders::_1));
        // 从D实例读流式响应失败
        dHandler.RegisterFun(ClientHandlerType::CHUNK_BODY_RES_ERROR,
                             std::bind(&RequestRepeater::DResErrorHandler, this, std::placeholders::_1));
    }
    // 从混合实例读到响应成功
    singleHandler.RegisterFun(ClientHandlerType::RES,
        std::bind(&RequestRepeater::SingleResHandler, this, std::placeholders::_1));
    // 向混合实例发送请求成功
    singleHandler.RegisterFun(ClientHandlerType::REQ,
        std::bind(&RequestRepeater::SingleSendHandler, this, std::placeholders::_1));
    // 向混合实例发送请求失败
    singleHandler.RegisterFun(ClientHandlerType::REQ_ERROR,
        std::bind(&RequestRepeater::SingleSendErrorHandler, this, std::placeholders::_1));

    // 从混合实例读非流式响应失败
    singleHandler.RegisterFun(ClientHandlerType::HEADER_RES_ERROR,
        std::bind(&RequestRepeater::SingleResErrorHandler, this, std::placeholders::_1));
    // 从混合实例读到流式响应成功（每收到一个chunk包触发一次）
    singleHandler.RegisterFun(ClientHandlerType::CHUNK_BODY_RES,
        std::bind(&RequestRepeater::SingleResChunkHandler, this, std::placeholders::_1));
    // 从混合实例读流式响应失败
    singleHandler.RegisterFun(ClientHandlerType::CHUNK_BODY_RES_ERROR,
        std::bind(&RequestRepeater::SingleResErrorHandler, this, std::placeholders::_1));
}

int RequestRepeater::Init()
{
    auto ret = httpClient.Init(Configure::Singleton()->mindieClientTlsItems,
        Configure::Singleton()->httpConfig.clientThreadNum, Configure::Singleton()->httpConfig.connectionPoolMaxConn);
    if (ret != 0) {
        LOG_E("[%s] [RequestRepeater] Initialize HTTP client failed.",
            GetErrorCode(ErrorType::CALL_ERROR, CoordinatorFeature::REQUEST_REPEATER).c_str());
        return -1;
    }
    timer.Start();
    return 0;
}

bool IsRouteNeeded(const std::string &reqId, std::shared_ptr<ReqAgent> reqInfo,
    const std::string &warnCode)
{
    if (reqInfo == nullptr) {
        LOG_W("[%s] [RequestRepeater] Failed to retrieve request information for request ID %s.",
            warnCode.c_str(), reqId.c_str());
        return false; // 判断请求已删除，则不再转发
    }

    // 判断已超时，则不再转发
    // bug 这里，如果请求不存在HasState 返回的是false,会继续往下走
    if (reqInfo->HasState(ReqState::TIMEOUT) || reqInfo->HasState(ReqState::EXCEPTION) ||
        reqInfo->HasState(ReqState::FINISH)) {
        LOG_W("[%s] [Request_Trace] Request ID %s has already ended, no need to handle prefill-decode routing.",
            warnCode.c_str(), reqId.c_str());
        return false;
    }

    return true;
}

bool IsFlex(std::string newPrefillIP, std::string newPrefillPort, std::string newDecodeIP, std::string newDecodePort)
{
    bool condition = (newPrefillIP == newDecodeIP && newPrefillPort == newDecodePort);
    if (condition) {
        LOG_I("Prefill node and decode node are the same, using single handler.");
    }
    return condition;
}

// 触发PD Route转发执行
int32_t RequestRepeater::PDRouteHandler(std::string reqId, uint64_t prefill, uint64_t decode)
{
    std::string warnCode = GetWarnCode(ErrorType::WARNING, CoordinatorFeature::REQUEST_REPEATER);
    auto reqInfo = reqManage->GetReqInfo(reqId);
    if (!IsRouteNeeded(reqId, reqInfo, warnCode)) {
        return 0;
    }

    reqManage->UpdateState(reqId, ReqState::SCHEDULED);
    auto prefillIp = instancesRecord->GetIp(prefill);
    auto prefillPort = instancesRecord->GetPort(prefill);
    auto decodeIp = instancesRecord->GetIp(decode);
    auto decodePort = instancesRecord->GetPort(decode);
    auto interCommPort = instancesRecord->GetInterCommPort(decode);
    auto dTarget = interCommPort.empty() ? decodeIp : decodeIp + ";" + interCommPort;
    reqInfo->SetReqNodeInfo(prefillIp, prefillPort, decodeIp, decodePort);
    auto condition = IsFlex(prefillIp, prefillPort, decodeIp, decodePort);
    // 若P和D实例IP相同，认为是Flex实例自身转发场景，基于Server实现，这里使用PD混合的回调函数处理及异常类型
    ClientHandler processHandler = condition ? singleHandler : pHandler;
    InsExceptionType eType = condition ? InsExceptionType::CONN_MIX_ERR : InsExceptionType::CONN_P_ERR;
    LOG_I("ScheduleHandler request ID is %s, prefill IP and port is %s:%s, decode IP and port is %s:%s",
        reqId.c_str(), prefillIp.c_str(), prefillPort.c_str(), decodeIp.c_str(), decodePort.c_str());
    auto req = reqInfo->GetReq();
    req.set("req-type", "prefill");
    req.set("req-id", reqId);
    req.set("d-target", dTarget);
    req.set("d-port", decodePort);
    reqInfo->SetReq(req);
    auto userConn = reqInfo->GetConnection();
    for (uint32_t i = 1; i <= 4; ++i) { // 重试4次
        auto connP = connectionPool->ApplyConn(prefillIp, prefillPort, processHandler);
        if (connP != nullptr) {
            reqInfo->SetClientConn(connP);
            reqManage->SetRoute(reqId, {prefill, decode});
            reqManage->SetRouteIp(reqId, {prefillIp, prefillPort, decodeIp, decodePort});
            reqManage->SetModelName(reqId, instancesRecord->GetModelName(prefill));
            // --- 记录域、信息和事件 ---
            PROF(INFO, Domain("Coordinator").Resource(reqId).Attr("PrefillAddress", prefillIp + ":" + prefillPort)
                .Attr("DecodeAddress", decodeIp + ":" + decodePort).Event("RequestDispatch"));
            LOG_I("Connect to prefill instance %s:%s start.", prefillIp.c_str(), prefillPort.c_str());
            connP->SendReq(req, reqId); // 请求发送： 发送P推理请求
            LOG_I("Connect to prefill instance %s:%s success.", prefillIp.c_str(), prefillPort.c_str());
            return 0;
        }
        LOG_W("[%s] [RequestRepeater] Handle pd routing, connect to prefill instance %s:%s failed, try %u times.",
            warnCode.c_str(), prefillIp.c_str(), prefillPort.c_str(), i);
    }
    // 请求中止： 获得向P的链路失败，异常中止
    LOG_E("[%s] [RequestRepeater] Handle pd routing, connect to prefill instance %s:%s failed",
        GetErrorCode(ErrorType::UNAVAILABLE, CoordinatorFeature::REQUEST_REPEATER).c_str(), prefillIp.c_str(),
        prefillPort.c_str());
    SendErrorRes(userConn, boost::beast::http::status::internal_server_error, "Connect to p instance failed\r\n");
    reqManage->UpdateState(reqId, ReqState::EXCEPTION);
    exceptionMonitor->PushInsException(eType, prefill);
    return 0;
}

int32_t RequestRepeater::SingleNodeHandler(std::string reqId, uint64_t nodeIndex)
{
    auto reqInfo = reqManage->GetReqInfo(reqId);
    if (reqInfo == nullptr) {
        LOG_W("[%s] [RequestRepeater] Handling single node, get request info failed.",
            GetWarnCode(ErrorType::WARNING, CoordinatorFeature::REQUEST_REPEATER).c_str());
        return 0;
    }
    if (reqInfo->HasState(ReqState::TIMEOUT) || reqInfo->HasState(ReqState::EXCEPTION) ||
        reqInfo->HasState(ReqState::FINISH)) {
        LOG_W("[%s] [Request_Trace] Request with ID %s already ended, no need to continue handle single node.",
            GetWarnCode(ErrorType::WARNING, CoordinatorFeature::REQUEST_REPEATER).c_str(), reqId.c_str());
        return 0;
    }
    reqManage->UpdateState(reqId, ReqState::SCHEDULED);
    auto ip = instancesRecord->GetIp(nodeIndex);
    auto port = instancesRecord->GetPort(nodeIndex);
    
    LOG_I("[RequestRepeater] Handling single node request, request id is %s, node is %s:%s.",
        reqId.c_str(), ip.c_str(), port.c_str());
    auto req = reqInfo->GetReq();
    req.set("req-id", reqId);
    reqInfo->SetReq(req);

    auto userConn = reqInfo->GetConnection();
    std::shared_ptr<ClientConnection> connect;
    for (uint32_t i = 1; i <= 4; ++i) { // 重试4次
        if (reqInfo->GetType() == ReqInferType::TOKENIZER) {
            connect = connectionPool->ApplyConn(ip, port, tokenizerHandler);
        } else {
            connect = connectionPool->ApplyConn(ip, port, singleHandler);
        }
        if (connect != nullptr) {
            reqInfo->SetClientConn(connect);
            reqManage->SetRoute(reqId, {nodeIndex, 0});
            connect->SendReq(req, reqId);
            return 0;
        }
        LOG_W("[%s] [RequestRepeater] Handling single node request, connect to instance %s:%s failed, try %u times",
            GetWarnCode(ErrorType::WARNING, CoordinatorFeature::REQUEST_REPEATER).c_str(), ip.c_str(),
            port.c_str(), i);
    }
    LOG_E("[%s] [RequestRepeater] Handling single node request, connect to instance %s:%s failed.",
        GetErrorCode(ErrorType::UNAVAILABLE, CoordinatorFeature::REQUEST_REPEATER).c_str(), ip.c_str(), port.c_str());
    SendErrorRes(userConn, boost::beast::http::status::internal_server_error, "Connect to MindIE-Server failed\r\n");
    reqManage->UpdateState(reqId, ReqState::EXCEPTION);
    if (reqInfo->GetType() == ReqInferType::TOKENIZER) {
        exceptionMonitor->PushInsException(InsExceptionType::CONN_TOKEN_ERR, nodeIndex);
    } else {
        exceptionMonitor->PushInsException(InsExceptionType::CONN_MIX_ERR, nodeIndex);
    }
    return 0;
}

void RequestRepeater::LogRequestCompletionStatus(std::string reqId, RequestPhase phase, bool isSuccess)
{
    // Retrieve request and node information
    auto reqInfo = reqManage->GetReqInfo(reqId);
    if (reqInfo == nullptr) {
        LOG_W("[%s] [RequestRepeater] Failed to retrieve request information for request ID %s.",
            GetWarnCode(ErrorType::WARNING, CoordinatorFeature::REQUEST_REPEATER).c_str(), reqId.c_str());
        return;
    }
    auto reqNodeInfo = reqInfo->GetReqNodeInfo();

    // Set result string based on the success flag
    const char* resultStr = isSuccess ? "success" : "failed";

    // Determine the request phase (prefill or decode)
    const char* requestPhaseStr = (phase == RequestPhase::PREFILL) ? "prefill" : "decode";

    LOG_I("ScheduleHandler request ID is %s, prefill IP and port is %s:%s, decode IP and port is %s:%s, %s %s",
        reqId.c_str(),
        reqNodeInfo.prefillIP.c_str(),
        reqNodeInfo.prefillPort.c_str(),
        reqNodeInfo.decodeIP.c_str(),
        reqNodeInfo.decodePort.c_str(),
        requestPhaseStr,
        resultStr);
}

int32_t RequestRepeater::ReportAbnormalNodeToController(std::string nodeInfo)
{
    int32_t code = 400; // 400 bad request
    std::map<boost::beast::http::field, std::string> headMap;
    headMap[boost::beast::http::field::content_type] = "";
    Request req = {"/v1/terminate-service", boost::beast::http::verb::post, headMap, nodeInfo};

    auto ip = Configure::Singleton()->httpConfig.controllerIP;
    auto alarmPort = Configure::Singleton()->httpConfig.alarmPort;

    LOG_I("[%s][RequestRepeater] Send terminate cmd to: ip %s, port %s.",
        GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::HTTPCLIENT_ASYNC).c_str(),
        ip.c_str(), alarmPort.c_str());
    std::string response = "";
    HttpClient client;
    client.Init(ip, alarmPort, Configure::Singleton()->alarmClientTlsItems);
    auto httpRet = client.SendRequest(req, Configure::Singleton()->httpConfig.httpTimeoutS,
        Configure::Singleton()->exceptionConfig.maxRetry, response, code);
    if (httpRet != 0 || code != 200 || response.empty()) { // 200 ok
        LOG_E("[%s] [RequestRepeater] Send terminate cmd failed, ip %s, port %s, "
            "ret code %d, request ret %d.",
            GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::HTTPCLIENT_ASYNC).c_str(),
            ip.c_str(), alarmPort.c_str(), code, httpRet);
        return -1;
    }
    LOG_I("[%s] [RequestRepeater] Send terminate cmd successfully, %s",
        GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::HTTPCLIENT_ASYNC).c_str(), response.c_str());
    return 0;
}
}