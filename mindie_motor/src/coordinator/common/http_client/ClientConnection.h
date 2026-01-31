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
#ifndef MINDIE_MS_COORDINATOR_CLIENT_CONNECTION_H
#define MINDIE_MS_COORDINATOR_CLIENT_CONNECTION_H

#include <mutex>
#include <deque>
#include "boost/asio.hpp"
#include "boost/asio/ssl/stream.hpp"
#include "boost/asio/steady_timer.hpp"
#include "boost/asio/ssl/error.hpp"
#include "boost/beast/core.hpp"
#include "boost/beast/http.hpp"
#include "boost/beast/version.hpp"
#include "boost/beast/ssl.hpp"
#include "ClientHandler.h"
#include "WriteDeque.h"

namespace MINDIE::MS {
constexpr uint32_t HTTP_VERSION_11 = 11;

using TCPStream = boost::beast::tcp_stream;
using SSLStream = boost::beast::ssl_stream<boost::beast::tcp_stream>;
using SSLContext = boost::asio::ssl::context;

class HttpClientAsync;

/// Connection of http client.
///
/// Connection of http client.
class ClientConnection : public virtual std::enable_shared_from_this<ClientConnection> {
public:
    ClientConnection(const ClientConnection& other) = delete;
    ClientConnection& operator=(const ClientConnection& other) = delete;
    explicit ClientConnection(boost::asio::io_context& ioContext, const ClientHandler& clientHandler,
        size_t timeout);
    ~ClientConnection()
    {
        DoClose();
    }
    void Start(uint32_t id, boost::beast::string_view ip, boost::beast::string_view port, HttpClientAsync *client);
    TCPStream& GetSocket();
    virtual SSLStream* GetSSLSocket()
    {
        return nullptr;
    }
    const std::string& GetIp() const;
    const std::string& GetPort() const;
    /// Send http request.
    ///
    /// Send request from http client to http server.
    ///
    /// \param req http request.
    /// \param reqId request id.
    void SendReq(const boost::beast::http::request<boost::beast::http::dynamic_body> &req,
        boost::beast::string_view reqId = "");
    boost::beast::http::response<boost::beast::http::dynamic_body>& GetResMessage();
    const std::string& GetResChunkedBody() const;
    bool ResIsFinish() const;
    uint32_t GetConnId() const;

    boost::beast::string_view GetContentType() const;
    bool IsAvailable() const;
    bool IsClose() const;
    void SetHandler(const ClientHandler& clientHandler);
    void SetAvailable(bool isAvailable);
    boost::beast::http::request<boost::beast::http::dynamic_body>& GetReq();
    std::string GetReqId() const;
    void GraceClose();
    void SetTimeout(size_t timeout);
    void DoClose();
    void ResetExpiresTime();
    std::string GetAddress() const
    {
        return ip_ + ":" + port_;
    }

protected:
    void DoRead();
    void OnRead(boost::beast::error_code ec, std::size_t bytes);
    virtual void DoWrite();
    /// Write http request handler.
    ///
    /// When a write event is triggered, this function will be called.
    ///
    /// \param ec error code.
    /// \param bytes write bytes.
    void OnWrite(boost::beast::error_code ec, std::size_t bytes);
    virtual void DoReadHeader();
    virtual void OnReadHeader(boost::beast::error_code ec, std::size_t bytes);
    void ParserChunkHeader(uint64_t size, boost::beast::string_view extensions, boost::beast::error_code& ec);
    size_t ParserChunkBody(uint64_t remain, boost::beast::string_view body, boost::beast::error_code& ec);
    virtual void OnChunkBody(boost::beast::error_code ec, size_t bytes);
    void OnReadBody(boost::beast::error_code ec, std::size_t bytes);
    virtual void Close();
    bool isClose;
    std::atomic<bool> graceClose = false;
    std::atomic<bool> available;
    uint32_t id_;
    HttpClientAsync *client_ = nullptr;
    size_t timeout_;
    boost::asio::io_context& ioContext_;
    TCPStream stream_;
    boost::beast::flat_buffer buffer_;
    ClientHandler clientHandler_;
    boost::optional<boost::beast::http::parser<false, boost::beast::http::dynamic_body>> p_;
    std::string chunk;
    std::function<void(uint64_t size, boost::beast::string_view extensions, boost::beast::error_code& ec)> headerCb;
    std::function<size_t(uint64_t remain, boost::beast::string_view body, boost::beast::error_code& ec)> bodyCb;
    std::string ip_;
    std::string port_;
    WriteDeque<boost::beast::http::request<boost::beast::http::dynamic_body>> writeMsgs;
    std::string mReqId = "";
    static constexpr uint32_t HEADER_LIMIT = 8 * 1024; // 响应头的最大上限8 * 1024
    std::mutex mtx;
};

/// TLS connection of http client.
///
/// TLS connection of http client.
class SSLClientConnection : public ClientConnection {
public:
    explicit SSLClientConnection(boost::asio::io_context& ioContext, const ClientHandler& clientHandler,
        SSLContext &sslContext, size_t timeout);
    SSLStream* GetSSLSocket() override;
    
private:
    void DoWrite() override;
    /// Write https request handler.
    ///
    /// When a write event is triggered, this function will be called.
    ///
    /// \param ec error code.
    /// \param bytes write bytes.
    void OnSSLWrite(boost::beast::error_code ec, std::size_t bytes);
    void OnSSLReadBody(boost::beast::error_code ec, std::size_t bytes);
    void DoReadHeader() override;
    void OnReadHeader(boost::beast::error_code ec, std::size_t bytes) override;
    void OnChunkBody(boost::beast::error_code ec, size_t bytes) override;
    void Close() override;

    SSLStream sslStream_;
};

}
#endif