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
#ifndef MINDIE_MS_COORDINATOR_HTTP_SERVER_H
#define MINDIE_MS_COORDINATOR_HTTP_SERVER_H

#include <mutex>
#include <atomic>
#include <set>
#include <cstdint>
#include "boost/asio.hpp"
#include "HttpIoCtxPool.h"
#include "ServerConnection.h"
#include "SSLServerConnection.h"
#include "ServerHandler.h"
#include "ConfigParams.h"

namespace MINDIE::MS {
struct HttpServerParm {
    std::string address{};
    std::string port{};
    ServerHandler serverHandler{};
    size_t timeout = 0;
    size_t maxKeepAliveReqs = 1;
    size_t keepAliveS = 0;
    TlsItems tlsItems;
};

class HttpServer {
public:
    HttpServer(const HttpServer& other) = delete;
    HttpServer& operator=(const HttpServer& other) = delete;
    HttpServer();
    ~HttpServer()
    {
        Stop();
    }
    int32_t Init(size_t ioContextPoolSize = 1, size_t maxConn = 10000);
    void Run(const std::vector<HttpServerParm>& parms, std::shared_ptr<std::atomic<bool>> ready = nullptr);
    void Stop()
    {
        ioContextPool_.Stop();
        ioContext_.stop();
    }
    void CloseOne(uint32_t connId);

private:
    void DoAccept(uint32_t i, const TlsItems &tlsItems);
    void DoAwaitStop();
    void HandleHandshake(std::shared_ptr<ServerConnection> newConnection, uint32_t i, TlsItems tlsItems,
        const boost::system::error_code& ec);
    void HandleAccept(std::shared_ptr<ServerConnection> newConnection, uint32_t i, TlsItems tlsItems,
        const boost::system::error_code& ec);
    int32_t SetTlsCtx(SSLContext &sslCtx, const TlsItems &tlsItems, VerifyItems &verifyItem) const;
    uint32_t GetConnectionId();
    boost::asio::io_context ioContext_;
    // serverConnection使用该Pool当中的ioContext;
    HttpIoCtxPool ioContextPool_;
    std::vector<std::unique_ptr<boost::asio::ip::tcp::acceptor>> acceptor_;
    boost::asio::signal_set signals_;
    std::vector<ServerHandler> serverHandler_;
    std::vector<size_t> timeout_;
    std::vector<size_t> maxKeepAliveReqs_;
    std::vector<size_t> keepAliveS;
    std::atomic<uint32_t> connectionIDIndex = 0;
    std::vector<std::unique_ptr<SSLContext>> ctx;
    std::vector<VerifyItems> verifyItems;
    size_t maxConnection;
    std::mutex mtx;
    std::set<uint32_t> connIdSet;
};

}
#endif