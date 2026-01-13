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
#ifndef MINDIE_MS_REQUEST_AGENT_H
#define MINDIE_MS_REQUEST_AGENT_H
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <array>
#include <limits>
#include "ServerConnection.h"
#include "BaseScheduler.h"
#include "boost/uuid/uuid_generators.hpp"
#include "HttpClientAsync.h"
#include "ConnectionPool.h"
#include "ClusterNodes.h"
#include "PerfMonitor.h"

namespace MINDIE::MS {

class PerfMonitor;

enum class ReqInferType {
    TGI,
    VLLM,
    OPENAI,
    TRITON,
    MINDIE,
    TOKENIZER
};

enum class ReqState {
    ARRIVE, // 请求到达
    SCHEDULED, // 调度成功
    REPEATED, // 转发成功
    FIRST_TOKEN_FINISH, // 首token处理完成
    FINISH, // 请求推理完成
    SEND_TOKENS_TO_USER, // 每个token返回给用户，都会刷新一次状态
    RECV_TOKENS_FROM_INS, // 从集群收到每个token结果，都会刷新一次状态
    EXCEPTION, // 请求已出现异常
    TIMEOUT, // 请求已出现超时
    RETRY, // 请求重计算
};

struct ReqNodeInfo {
    std::string prefillIP;
    std::string prefillPort;
    std::string decodeIP;
    std::string decodePort;
};

constexpr uint64_t UNKNOWN_ID = std::numeric_limits<uint64_t>::max();
constexpr uint64_t MEMBER_NUM = 2;
constexpr uint64_t IP_INFO_MEMBER_NUM = 4;
class ReqAgent {
public:
    ReqAgent(boost::beast::string_view reqIdInit, ReqInferType typeInit,
        std::shared_ptr<ServerConnection> connectionInit,
        boost::beast::http::request<boost::beast::http::dynamic_body> &reqInit);
    ~ReqAgent() = default;
    void SetIsStream(bool isStreamNew);
    bool GetIsStream();
    ReqInferType GetType();
    void AddOutputNum(size_t num);
    size_t GetOutputNum();
    void ClearOutputNum();
    void AddRetry();
    size_t GetRetry();
    void ClearRetry();
    std::string GetReqId();
    std::shared_ptr<ServerConnection> GetConnection();
    boost::beast::http::request<boost::beast::http::dynamic_body> GetReq();
    const boost::beast::http::request<boost::beast::http::dynamic_body>& GetReqRef() const;
    ReqNodeInfo GetReqNodeInfo() const;
    
    void SetReq(boost::beast::http::request<boost::beast::http::dynamic_body> newReq);
    void SetRoute(const std::array<uint64_t, MEMBER_NUM> &newRoute);
    void SetRouteIp(const std::array<std::string, IP_INFO_MEMBER_NUM> &newRouteIp);
    void SetReqNodeInfo(std::string newPrefillIP, std::string newPrefillPort,
        std::string newDecodeIP, std::string newDecodePort);
    std::array<uint64_t, MEMBER_NUM> GetRoute();
    std::array<std::string, IP_INFO_MEMBER_NUM> GetRouteIp();
    void UpdateState(ReqState state, uint64_t time);
    const std::vector<uint64_t> GetStateTime(ReqState state);
    bool HasState(ReqState state);

    void SetModelName(std::string newModelName);
    std::string GetModelName();
    
    // 转发p 流式first token
    void RepeatPStreamToken(const ServerRes &res, bool &reqFinish);
    
    // 转发d 流式token
    void RepeatDStreamToken(const ServerRes &res, bool &reqFinish);
    void SetClientConn(std::shared_ptr<ClientConnection> conn);
    std::shared_ptr<ClientConnection> GetClientConn();

    // Actively release large memory objects to reclaim memory earlier when the request ends
    void ClearLargeMembers();

private:
    bool isStream;
    ReqInferType type;
    size_t outputNum;
    size_t retry;
    std::string reqId;
    ReqNodeInfo reqNodeInfo;
    std::string modelName;

    std::shared_ptr<ServerConnection> connection;
    boost::beast::http::request<boost::beast::http::dynamic_body> req;
    std::array<uint64_t, MEMBER_NUM> route;
    std::array<std::string, IP_INFO_MEMBER_NUM> routeIp;
    std::map<ReqState, std::vector<uint64_t>> status;
    mutable std::shared_mutex mtx;

    std::mutex pdSyncMtx;

    WriteDeque<ServerRes> waitQueue; // 用于缓存该消息中等待的队列
    std::shared_ptr<ClientConnection> clientConn;
};
}
#endif