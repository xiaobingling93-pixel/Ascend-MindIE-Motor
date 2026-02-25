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
#ifndef MINDIE_MS_COORDINATOR_HTTP_CLIENT_H
#define MINDIE_MS_COORDINATOR_HTTP_CLIENT_H

#include <mutex>
#include <memory>
#include "boost/beast.hpp"
#include "boost/asio.hpp"
#include "HttpIoCtxPool.h"
#include "ClientConnection.h"
#include "ConfigParams.h"

namespace MINDIE::MS {
class HttpClientAsync {
public:
    HttpClientAsync(const HttpClientAsync& other) = delete;
    HttpClientAsync& operator=(const HttpClientAsync& other) = delete;
    explicit HttpClientAsync();
    ~HttpClientAsync();
    int32_t Init(TlsItems &clientTlsItems, uint32_t ioContextPoolSize = 1, uint32_t maxConnection = 0xFFFF);
    void RemoveConnection(uint32_t id);
    // GetConnection的返回值只能临时变量，不能长期使用因为链接可能会断
    std::shared_ptr<ClientConnection> GetConnection(uint32_t id);
    // timeout: 设置超时时间，单位是秒；在超时时间内没有消息发送或接收，关闭连接；0代表不设置超时
    bool AddConnectionTLS(std::shared_ptr<ClientConnection> &newConnection, boost::asio::io_context& ioContext,
        const ClientHandler& clientHandler, uint32_t timeout, boost::asio::ip::tcp::resolver::results_type &results);
    bool AddConnection(boost::beast::string_view ip, boost::beast::string_view port, uint32_t &id,
        const ClientHandler& clientHandler = {}, uint32_t timeout = 0);
    std::vector<uint32_t> FindId(boost::beast::string_view ip, boost::beast::string_view port);
    int SetHandler(uint32_t id, const ClientHandler& clientHandler);
    size_t GetConnSize();
    void OnConnection(const boost::beast::error_code& ec, const boost::asio::ip::tcp::endpoint&);

private:
    void OnResolve(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type results);
    void OnConnect(std::shared_ptr<ClientConnection> newConnection, boost::beast::error_code ec,
        boost::asio::ip::tcp::resolver::results_type::endpoint_type);
    void Stop();
    int32_t SetTlsCtx(SSLContext &sslCtx, TlsItems &tlsItems, bool mUseKMC = true);
    bool CreateNewConnId(uint32_t &id, std::shared_ptr<ClientConnection> newConnection);
    int32_t ReportAbnormalNodeToController(std::string nodeInfo);
    uint32_t maxConnection_ = 0;
    std::condition_variable conV_ {};
    bool connectionCompleted_ = false;
    std::mutex conMutex_ {};
    std::atomic<bool> isException = false;
    boost::asio::io_context ioContext_;
    HttpIoCtxPool ioContextPool_;
    boost::asio::ip::tcp::resolver resolver_;
    std::map<uint32_t, std::shared_ptr<ClientConnection>> connections_;
    std::mutex mtx;
    TlsItems items;
    VerifyItems verifyItems = {false, "", "", "", "", true};
    SSLContext ctx = SSLContext(SSLContext::tlsv13_client);
};

}
#endif