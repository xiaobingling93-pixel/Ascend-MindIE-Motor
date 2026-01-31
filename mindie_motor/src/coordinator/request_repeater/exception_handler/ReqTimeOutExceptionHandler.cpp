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
void RequestRepeater::ReqScheduleTimeoutHandler(const std::string &reqId)
{
    LOG_E("[%s] [RequestRepeater] Scheduling timeout occurred for the request. Request ID: %s.",
        GetErrorCode(ErrorType::DEADLINE_EXCEEDED, CoordinatorFeature::REQTIMEOUT_EXCEPTIONHANDLER).c_str(),
        reqId.c_str());
    auto reqInfo = reqManage->GetReqInfo(reqId);
    if (reqInfo == nullptr) {
        LOG_E("[%s] [RequestRepeater] Failed to get request information for the given ID: %s.",
            GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::REQTIMEOUT_EXCEPTIONHANDLER).c_str(),
            reqId.c_str());
        return;
    }
    LOG_D("[Request_Trace] Sending scheduling timeout error response for request ID: %s.", reqId.c_str());
    SendErrorRes(reqInfo->GetConnection(), boost::beast::http::status::request_timeout, "Request schedule timeout\r\n");
    reqManage->UpdateState(reqId, ReqState::EXCEPTION);
}

void RequestRepeater::ReqFirstTokenTimeoutHandler(const std::string &reqId)
{
    LOG_E("[%s] [RequestRepeater] Timeout occurred while waiting for the first token for the request. "
        "Request ID is %s.",
        GetErrorCode(ErrorType::DEADLINE_EXCEEDED, CoordinatorFeature::REQTIMEOUT_EXCEPTIONHANDLER).c_str(),
        reqId.c_str());
    auto reqInfo = reqManage->GetReqInfo(reqId);
    if (reqInfo == nullptr) {
        return;
    }
    LOG_D("[Request_Trace] Sending first-token timeout error response for request ID %s.", reqId.c_str());
    SendErrorRes(reqInfo->GetConnection(), boost::beast::http::status::request_timeout,
        "Request first token timeout\r\n");
    UserDisConnHandler(reqId);
    reqManage->UpdateState(reqId, ReqState::EXCEPTION);
}

void RequestRepeater::ReqInferTimeoutHandler(const std::string &reqId)
{
    LOG_E("[%s] [RequestRepeater] Inferencing exceeded the allowed time for the request. Request ID: %s.",
        GetErrorCode(ErrorType::DEADLINE_EXCEEDED, CoordinatorFeature::REQTIMEOUT_EXCEPTIONHANDLER).c_str(),
        reqId.c_str());
    auto reqInfo = reqManage->GetReqInfo(reqId);
    if (reqInfo == nullptr) {
        return;
    }
    LOG_D("[Request_Trace] Sending inference timeout error response for request ID: %s.", reqId.c_str());
    SendErrorRes(reqInfo->GetConnection(), boost::beast::http::status::request_timeout,
        "Request inference timeout\r\n");
    UserDisConnHandler(reqId);
    reqManage->UpdateState(reqId, ReqState::EXCEPTION);
}

void RequestRepeater::ReqTokenizerTimeoutHandler(const std::string &reqId)
{
    LOG_E("[%s] [RequestRepeater] Tokenizer processing exceeded the allowed time for the request. Request ID:is %s.",
        GetErrorCode(ErrorType::DEADLINE_EXCEEDED, CoordinatorFeature::REQTIMEOUT_EXCEPTIONHANDLER).c_str(),
        reqId.c_str());
    auto reqInfo = reqManage->GetReqInfo(reqId);
    if (reqInfo == nullptr) {
        return;
    }
    SendErrorRes(reqInfo->GetConnection(), boost::beast::http::status::request_timeout,
        "Request tokenizer timeout\r\n");
    reqManage->UpdateState(reqId, ReqState::EXCEPTION);
}
}