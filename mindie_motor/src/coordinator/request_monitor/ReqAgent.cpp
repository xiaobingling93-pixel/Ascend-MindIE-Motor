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
#include "ReqAgent.h"
#include "Logger.h"
#include "Configure.h"

using namespace MINDIE::MS;

ReqAgent::ReqAgent(boost::beast::string_view reqIdInit, ReqInferType typeInit,
    std::shared_ptr<ServerConnection> connectionInit,
    boost::beast::http::request<boost::beast::http::dynamic_body> &reqInit)
    : isStream(true), type(typeInit), outputNum(0), retry(0), reqId(reqIdInit), connection(connectionInit),
      clientConn(nullptr)
{
    req = std::move(reqInit);
}

void ReqAgent::SetIsStream(bool isStreamNew)
{
    std::unique_lock<std::shared_mutex> lock(mtx);
    isStream = isStreamNew;
    LOG_D("[ReqAgent] Request ID is %s, set stream flag is %d.", reqId.c_str(), isStreamNew);
}

bool ReqAgent::GetIsStream()
{
    std::shared_lock<std::shared_mutex> lock(mtx);
    return isStream;
}

ReqInferType ReqAgent::GetType()
{
    std::shared_lock<std::shared_mutex> lock(mtx);
    return type;
}

void ReqAgent::AddOutputNum(size_t num)
{
    std::unique_lock<std::shared_mutex> lock(mtx);
    outputNum += num;
}

size_t ReqAgent::GetOutputNum()
{
    std::shared_lock<std::shared_mutex> lock(mtx);
    return outputNum;
}

void ReqAgent::ClearOutputNum()
{
    std::unique_lock<std::shared_mutex> lock(mtx);
    outputNum = 0;
}

void ReqAgent::AddRetry()
{
    std::unique_lock<std::shared_mutex> lock(mtx);
    retry++;
}

size_t ReqAgent::GetRetry()
{
    std::shared_lock<std::shared_mutex> lock(mtx);
    return retry;
}

void ReqAgent::ClearRetry()
{
    std::unique_lock<std::shared_mutex> lock(mtx);
    retry = 0;
}

std::string ReqAgent::GetReqId()
{
    std::shared_lock<std::shared_mutex> lock(mtx);
    return reqId;
}

std::shared_ptr<ServerConnection> ReqAgent::GetConnection()
{
    std::shared_lock<std::shared_mutex> lock(mtx);
    return connection;
}

boost::beast::http::request<boost::beast::http::dynamic_body> ReqAgent::GetReq()
{
    std::shared_lock<std::shared_mutex> lock(mtx);
    return req;
}

const boost::beast::http::request<boost::beast::http::dynamic_body>& ReqAgent::GetReqRef() const
{
    std::shared_lock<std::shared_mutex> lock(mtx);
    return req;
}

ReqNodeInfo ReqAgent::GetReqNodeInfo() const
{
    return reqNodeInfo;
}

void ReqAgent::SetReq(boost::beast::http::request<boost::beast::http::dynamic_body> newReq)
{
    std::unique_lock<std::shared_mutex> lock(mtx);
    req = newReq;
}

void ReqAgent::SetRoute(const std::array<uint64_t, MEMBER_NUM> &newRoute)
{
    std::unique_lock<std::shared_mutex> lock(mtx);
    route = newRoute;
}

void ReqAgent::SetRouteIp(const std::array<std::string, IP_INFO_MEMBER_NUM> &newRouteIp)
{
    std::unique_lock<std::shared_mutex> lock(mtx);
    routeIp = newRouteIp;
}

void ReqAgent::SetReqNodeInfo(std::string newPrefillIP, std::string newPrefillPort,
    std::string newDecodeIP, std::string newDecodePort)
{
    reqNodeInfo.prefillIP = std::move(newPrefillIP);
    reqNodeInfo.prefillPort = std::move(newPrefillPort);
    reqNodeInfo.decodeIP = std::move(newDecodeIP);
    reqNodeInfo.decodePort = std::move(newDecodePort);
}

void ReqAgent::SetModelName(std::string newModelName)
{
    modelName = newModelName;
}

std::string ReqAgent::GetModelName()
{
    return modelName;
}

std::array<uint64_t, MEMBER_NUM> ReqAgent::GetRoute()
{
    std::shared_lock<std::shared_mutex> lock(mtx);
    return route;
}

std::array<std::string, IP_INFO_MEMBER_NUM> ReqAgent::GetRouteIp()
{
    std::shared_lock<std::shared_mutex> lock(mtx);
    return routeIp;
}

void ReqAgent::UpdateState(ReqState state, uint64_t time)
{
    std::unique_lock<std::shared_mutex> lock(mtx);
    status[state].emplace_back(time);
}

// 只能返回值，不能返回引用，返回引用，会与UpdateState有并发读写的冲突
const std::vector<uint64_t> ReqAgent::GetStateTime(ReqState state)
{
    std::shared_lock<std::shared_mutex> lock(mtx);
    auto it = status.find(state);
    if (it == status.end()) {
        return {0};
    }
    return it->second;
}

bool ReqAgent::HasState(ReqState state)
{
    std::shared_lock<std::shared_mutex> lock(mtx);
    auto it = status.find(state);
    if (it == status.end()) {
        return false;
    }
    return true;
}

// 转发p 流式first token
void ReqAgent::RepeatPStreamToken(const ServerRes &res, bool &reqFinish)
{
    if (!connection) {
        LOG_E("[ReqAgent] Invalid null ServerConnection in RepeatPStreamToken.");
        return;
    }
    std::unique_lock<std::mutex> lock(pdSyncMtx);
    reqFinish = false;
    connection->SendRes(res);
    AddOutputNum(1);
    LOG_D("[RequestRepeater] Repeat prefill stream token, wakeup prefill wait queue: %s", reqId.data());

    ServerRes dRes; // 去清可能在在等待的D队列
    while (!waitQueue.Empty()) {
        dRes = waitQueue.Front();
        waitQueue.PopFront();
        AddOutputNum(1);
        connection->SendRes(dRes);
        if (dRes.isFinish) {
            reqFinish = true;
            return;
        }
    }
}
    
// 转发d 流式token
void ReqAgent::RepeatDStreamToken(const ServerRes &res, bool &reqFinish)
{
    if (!connection) {
        LOG_E("[ReqAgent] Invalid null ServerConnection in RepeatDStreamToken.");
        return;
    }
    // 流式推理，且尚未收到P的场景，不能直接发给用户，需要放入队列等待P
    {
        std::unique_lock<std::mutex> lock(pdSyncMtx);
        if (!HasState(ReqState::FIRST_TOKEN_FINISH)) {
            LOG_D("[RequestRepeater] Decode result normal token: push one wait message request ID: %s", reqId.data());
            waitQueue.PushBack(res); // 这种情况下在等待发送，尚未finish
            return;
        }
    }
    // 其他场景，直接发送token，且请求宣布结束。
    LOG_D("[RequestRepeater] Repeat decode stream token, send one decode token to user, ID: %s ", reqId.data());
    connection->SendRes(res);
    AddOutputNum(1);
    if (res.isFinish) {
        reqFinish = true;
    } else {
        reqFinish = false;
    }
}

void ReqAgent::SetClientConn(std::shared_ptr<ClientConnection> conn)
{
    std::unique_lock<std::shared_mutex> lock(mtx);
    clientConn = conn;
}

std::shared_ptr<ClientConnection> ReqAgent::GetClientConn()
{
    std::shared_lock<std::shared_mutex> lock(mtx);
    return clientConn;
}

void ReqAgent::ClearLargeMembers()
{
    std::unique_lock<std::shared_mutex> lock(mtx);
    // Clear the request body to release potentially large request text memory
    req.body().consume(req.body().size());
    // Clear the waiting queue to release cached response data
    waitQueue.Clear();
}