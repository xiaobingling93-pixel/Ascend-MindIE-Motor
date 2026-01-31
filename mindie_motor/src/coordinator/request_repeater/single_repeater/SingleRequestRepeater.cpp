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

void RequestRepeater::SingleSendHandler(std::shared_ptr<ClientConnection> connection)
{
    auto reqId = connection->GetReqId();
    if (reqId.empty()) {
        return;
    }
    reqManage->UpdateState(reqId, ReqState::REPEATED);
}

void RequestRepeater::SingleResHandler(std::shared_ptr<ClientConnection> connection)
{
    auto reqId = connection->GetReqId();
    if (reqId.empty()) {
        return;
    }
    auto reqInfo = reqManage->GetReqInfo(reqId);
    if (reqInfo == nullptr) {
        LOG_W("[%s] [RequestRepeater] Get request id %s failed.",
            GetWarnCode(ErrorType::WARNING, CoordinatorFeature::SINGLEREQUEST_REPEATER).c_str(), reqId.c_str());
        return;
    }
    // 请求已终止，不必往下走
    if (reqInfo->HasState(ReqState::FINISH) || reqInfo->HasState(ReqState::EXCEPTION) ||
        reqInfo->HasState(ReqState::TIMEOUT)) {
        LOG_W("[%s] [Request_Trace] Requrdy with ID is%s already ended.",
            GetWarnCode(ErrorType::WARNING, CoordinatorFeature::SINGLEREQUEST_REPEATER).c_str(), reqId.c_str());
        return;
    }
    reqManage->UpdateState(reqId, ReqState::RECV_TOKENS_FROM_INS);
    auto &res = connection->GetResMessage();
    auto serverConn = reqInfo->GetConnection();
    if (serverConn == nullptr) {
        LOG_E("[%s] [RequestRepeater] Get connection failed, request ID is %s.",
            GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::SINGLEREQUEST_REPEATER).c_str(),
            reqId.c_str());
        SendErrorRes(serverConn, boost::beast::http::status::internal_server_error, "Connection record failed\r\n");
        SingleResFinish(connection, ReqState::EXCEPTION);
        return;
    }
    ServerRes serverRes;
    ReqState state = ReqState::FINISH;
    auto body = boost::beast::buffers_to_string(res.body().data());
    if (res.result() != boost::beast::http::status::ok) {
        LOG_E("[%s] [RequestRepeater] Failed to handle sinlge node response, error is %s.",
            GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::SINGLEREQUEST_REPEATER).c_str(),
            body.c_str());
        serverRes.state = res.result();
        state = ReqState::EXCEPTION;
    }
    serverRes.body = body;
    serverRes.contentType = connection->GetContentType();
    LOG_D("[Request_Trace] Sent token back to user for request ID %s. %s.", reqId.c_str(),
        serverRes.isFinish ? "This token marks the end of the response" : "This is an intermediate token");
    serverConn->SendRes(serverRes);
    SingleResFinish(connection, state);
}

void RequestRepeater::SingleResChunkHandler(std::shared_ptr<ClientConnection> connection)
{
    auto reqId = connection->GetReqId();
    if (reqId.empty()) {
        return;
    }
    auto reqInfo = reqManage->GetReqInfo(reqId);
    if (reqInfo == nullptr) {
        LOG_W("[%s] [RequestRepeater] Get request id %s failed",
            GetWarnCode(ErrorType::WARNING, CoordinatorFeature::SINGLEREQUEST_REPEATER).c_str(), reqId.c_str());
        return;
    }
    // 请求已终止，不必往下走
    if (reqInfo->HasState(ReqState::FINISH) || reqInfo->HasState(ReqState::EXCEPTION) ||
        reqInfo->HasState(ReqState::TIMEOUT)) {
        LOG_W("[%s] [Request_Trace] Request ID is %s already ended.",
            GetWarnCode(ErrorType::WARNING, CoordinatorFeature::SINGLEREQUEST_REPEATER).c_str(), reqId.c_str());
        return;
    }
    reqManage->UpdateState(reqId, ReqState::RECV_TOKENS_FROM_INS);
    if (!reqInfo->HasState(ReqState::FIRST_TOKEN_FINISH)) {
        reqManage->UpdateState(reqId, ReqState::FIRST_TOKEN_FINISH);
    }
    boost::beast::string_view body = connection->GetResChunkedBody();
    auto serverConn = reqInfo->GetConnection();
    if (serverConn == nullptr) {
        LOG_E("[%s] [RequestRepeater] Get connection failed, request ID is %s.",
            GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::SINGLEREQUEST_REPEATER).c_str(),
            reqId.c_str());
        return;
    }
    ServerRes serverRes;
    serverRes.isFinish = connection->ResIsFinish();
    serverRes.body = body;
    serverRes.contentType = connection->GetContentType();
    LOG_D("[RequestRepeater] Sent token back to user for request ID %s. %s.",
        reqId.c_str(), serverRes.isFinish ? "This token marks the end of the response" :
        "This is an intermediate token");
    serverConn->SendRes(serverRes);
    if (serverRes.isFinish) {
        SingleResFinish(connection, ReqState::FINISH);
    }
}

void RequestRepeater::SingleResFinish(std::shared_ptr<ClientConnection> connection, ReqState state)
{
    auto reqId = connection->GetReqId();
    reqManage->UpdateState(reqId, state);
}

void RequestRepeater::SingleResErrorHandler(std::shared_ptr<ClientConnection> connection)
{
    auto reqId = connection->GetReqId();
    if (reqId.empty()) {
        return;
    }
    auto reqInfo = reqManage->GetReqInfo(reqId);
    if (reqInfo == nullptr) {
        LOG_W("[%s] [RequestRepeater] Get request ID %s failed.",
            GetWarnCode(ErrorType::WARNING, CoordinatorFeature::SINGLEREQUEST_REPEATER).c_str(), reqId.c_str());
        return;
    }
    if (reqInfo->HasState(ReqState::FINISH) || reqInfo->HasState(ReqState::EXCEPTION) ||
        reqInfo->HasState(ReqState::TIMEOUT)) {
        LOG_W("[%s] [Request_Trace] Request with ID %s is already ended.",
            GetWarnCode(ErrorType::WARNING, CoordinatorFeature::SINGLEREQUEST_REPEATER).c_str(), reqId.c_str());
        return;
    }
    auto serverConn = reqInfo->GetConnection();
    LOG_E("[%s] [RequestRepeater] Failed to handle single node response error, request id is %s.",
        GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::SINGLEREQUEST_REPEATER).c_str(),
        reqId.c_str());
    SendErrorRes(serverConn, boost::beast::http::status::internal_server_error,
        "Read messages from instance failed\r\n");
    reqManage->UpdateState(reqId, ReqState::EXCEPTION);
}

void RequestRepeater::SingleSendErrorHandler(std::shared_ptr<ClientConnection> connection)
{
    auto reqId = connection->GetReqId();
    if (reqId.empty()) {
        return;
    }
    auto reqInfo = reqManage->GetReqInfo(reqId);
    if (reqInfo == nullptr) {
        LOG_W("[%s] [RequestRepeater] SingleSendErrorHandler: Get request %s failed",
            GetWarnCode(ErrorType::WARNING, CoordinatorFeature::SINGLEREQUEST_REPEATER).c_str(), reqId.c_str());
        return;
    }
    if (reqInfo->GetRetry() > Configure::Singleton()->exceptionConfig.maxRetry) {
        auto insId = reqInfo->GetRoute()[0];
        auto ip = instancesRecord->GetIp(insId);
        auto port = instancesRecord->GetPort(insId);
        LOG_E("[%s] [RequestRepeater] SingleSendErrorHandler: Send req %s to instance %s:%s failed",
            GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::SINGLEREQUEST_REPEATER).c_str(),
            reqId.c_str(), ip.c_str(), port.c_str());
        SendErrorRes(reqInfo->GetConnection(), boost::beast::http::status::internal_server_error,
            "Send messges to instance failed\r\n");
        return reqManage->UpdateState(reqId, ReqState::EXCEPTION);
    }
    exceptionMonitor->PushReqException(ReqExceptionType::SEND_MIX_ERR, reqId);
}
}