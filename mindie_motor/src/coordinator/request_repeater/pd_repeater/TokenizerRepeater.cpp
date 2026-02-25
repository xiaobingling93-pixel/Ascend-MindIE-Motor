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

void RequestRepeater::TokenizerReqHandler(std::shared_ptr<ClientConnection> connection)
{
    auto reqId = connection->GetReqId();
    if (reqId.empty()) {
        return;
    }
    reqManage->UpdateState(reqId, ReqState::REPEATED);
}

void RequestRepeater::TokenizerReqErrHandler(std::shared_ptr<ClientConnection> connection)
{
    auto reqId = connection->GetReqId();
    if (reqId.empty()) {
        return;
    }
    auto reqInfo = reqManage->GetReqInfo(reqId);
    if (reqInfo == nullptr) {
        LOG_W("[%s] [RequestRepeater] Get request %s failed",
            GetWarnCode(ErrorType::WARNING, CoordinatorFeature::TOKENIZER_REPEATER).c_str(), reqId.c_str());
        return;
    }
    if (reqInfo->GetRetry() > Configure::Singleton()->exceptionConfig.maxRetry) {
        auto insId = reqInfo->GetRoute()[0];
        auto ip = instancesRecord->GetIp(insId);
        auto port = instancesRecord->GetPort(insId);
        LOG_E("[%s] [RequestRepeater] Send request %s to instance %s:%s failed.",
            GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::TOKENIZER_REPEATER).c_str(),
            reqId.c_str(), ip.c_str(), port.c_str());
        SendErrorRes(reqInfo->GetConnection(), boost::beast::http::status::internal_server_error,
            "Send messges to instance failed\r\n");
        reqManage->UpdateState(reqId, ReqState::EXCEPTION);
        return;
    }
    exceptionMonitor->PushReqException(ReqExceptionType::SEND_TOKEN_ERR, reqId);
}

void RequestRepeater::TokenizerResHandler(std::shared_ptr<ClientConnection> connection)
{
    auto reqId = connection->GetReqId();
    auto reqInfo = reqManage->GetReqInfo(reqId);
    if (reqInfo == nullptr) {
        LOG_W("[%s] [RequestRepeater] Get request id %s information failed.",
            GetWarnCode(ErrorType::WARNING, CoordinatorFeature::TOKENIZER_REPEATER).c_str(), reqId.c_str());
        return;
    }
    if (reqInfo->HasState(ReqState::FINISH) || reqInfo->HasState(ReqState::EXCEPTION) ||
        reqInfo->HasState(ReqState::TIMEOUT)) {
        LOG_W("[%s] [Request_Trace] Request with ID is %s already ended, "
            "no need to go ahead handle tokenizer response.",
            GetWarnCode(ErrorType::WARNING, CoordinatorFeature::TOKENIZER_REPEATER).c_str(), reqId.c_str());
        return;
    }
    auto userConn = reqInfo->GetConnection();
    if (userConn == nullptr) {
        LOG_E("[%s] [RequestRepeater] User connection is nullptr.",
            GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::TOKENIZER_REPEATER).c_str());
        return;
    }
    auto &res = connection->GetResMessage();
    auto body = boost::beast::buffers_to_string(res.body().data());
    if (res.result() != boost::beast::http::status::ok) {
        LOG_E("[%s] [RequestRepeater] Filed to handle tokenizer response, error is %s.",
            GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::TOKENIZER_REPEATER).c_str(),
            body.c_str());
    }
    ServerRes serverRes;
    serverRes.body = body;
    serverRes.state = res.result();
    userConn->SendRes(serverRes);
    reqManage->UpdateState(reqId, ReqState::FINISH);
}

void RequestRepeater::TokenizerResErrHandler(std::shared_ptr<ClientConnection> connection)
{
    auto ip = connection->GetIp();
    auto port = connection->GetPort();
    LOG_E("[%s] [RequestRepeater] Read tokenizer response from node %s:%s failed.",
        GetErrorCode(ErrorType::INVALID_PARAMETER, CoordinatorFeature::TOKENIZER_REPEATER).c_str(),
        ip.c_str(), port.c_str());
    auto reqId = connection->GetReqId();
    auto reqInfo = reqManage->GetReqInfo(reqId);
    if (reqInfo == nullptr) {
        LOG_W("[%s] [RequestRepeater] Get request ID %s information failed.",
            GetWarnCode(ErrorType::WARNING, CoordinatorFeature::TOKENIZER_REPEATER).c_str(), reqId.c_str());
        return;
    }
    if (reqInfo->HasState(ReqState::FINISH) || reqInfo->HasState(ReqState::EXCEPTION) ||
        reqInfo->HasState(ReqState::TIMEOUT)) {
        LOG_W("[%s] [Request_Trace] Request with ID is %s already ended.",
            GetWarnCode(ErrorType::WARNING, CoordinatorFeature::TOKENIZER_REPEATER).c_str(), reqId.c_str());
        return;
    }
    auto userConn = reqInfo->GetConnection();
    if (userConn == nullptr) {
        LOG_E("[%s] [RequestRepeater] User connection is nullptr.",
            GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::TOKENIZER_REPEATER).c_str());
        return;
    }
    SendErrorRes(userConn, boost::beast::http::status::internal_server_error,
        "Read tokenizer response from mindie-server failed\r\n");
    reqManage->UpdateState(reqId, ReqState::EXCEPTION);
}

}