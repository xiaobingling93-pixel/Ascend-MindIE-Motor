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

void RequestRepeater::ConnMixErrHandler(uint64_t insId)
{
    auto ip = instancesRecord->GetIp(insId);
    auto port = instancesRecord->GetPort(insId);
    for (size_t i = 0; i < Configure::Singleton()->exceptionConfig.maxRetry; ++i) {
        auto conn = connectionPool->ApplyConn(ip, port, singleHandler);
        if (conn != nullptr) {
            conn->SetAvailable(true);
            return;
        }
    }
    LOG_E("[%s] [RequestRepeater] Instance %s:%s connect failed after maximum retry attempts.",
        GetErrorCode(ErrorType::UNAVAILABLE, CoordinatorFeature::SINGLE_EXCEPTIONHANDLER).c_str(),
        ip.c_str(), port.c_str());
    scheduler->RemoveInstance({insId});
    instancesRecord->RemoveInstance(insId);
}

void RequestRepeater::ReqSendMixErrHandler(const std::string &reqId)
{
    auto reqInfo = reqManage->GetReqInfo(reqId);
    if (reqInfo == nullptr) {
        LOG_E("[%s] [RequestRepeater] Failed to get request information for the request ID %s. "
            "Request processing stop.",
            GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::SINGLE_EXCEPTIONHANDLER).c_str(),
            reqId.c_str());
        return;
    }
    auto insId = reqInfo->GetRoute()[0];
    auto ip = instancesRecord->GetIp(insId);
    auto port = instancesRecord->GetPort(insId);
    auto serverConn = reqInfo->GetConnection();
    auto req = reqInfo->GetReq();
    for (uint32_t i = 1; i <= 4; ++i) { // 重试4次
        auto conn = connectionPool->ApplyConn(ip, port, singleHandler);
        if (conn != nullptr) {
            reqInfo->SetClientConn(conn);
            reqInfo->AddRetry();
            conn->SendReq(req, reqId);
            return;
        }
        LOG_W("[%s] [RequestRepeater] Connect to instance %s:%s failed, try %u times.",
            GetWarnCode(ErrorType::WARNING, CoordinatorFeature::SINGLE_EXCEPTIONHANDLER).c_str(), ip.c_str(),
            port.c_str(), i);
    }
    LOG_E("[%s] [RequestRepeater] Apply connection to node %s:%s failed after maximum retry attempts.",
        GetErrorCode(ErrorType::UNAVAILABLE, CoordinatorFeature::SINGLE_EXCEPTIONHANDLER).c_str(),
        ip.c_str(), port.c_str());
    SendErrorRes(serverConn, boost::beast::http::status::internal_server_error,
        "Send message to instance failed\r\n");
    exceptionMonitor->PushInsException(InsExceptionType::CONN_MIX_ERR, insId);
    reqManage->UpdateState(reqId, ReqState::EXCEPTION);
}
}