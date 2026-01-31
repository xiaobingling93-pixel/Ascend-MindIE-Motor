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
#include "Logger.h"
#include "HttpServer.h"
#include "Configure.h"
#include "SSLServerConnection.h"

namespace MINDIE::MS {
SSLServerConnection::SSLServerConnection(boost::asio::io_context& ioContext, ServerHandler& serverHandler,
    HttpServer &server, SSLContext &sslContext) : ServerConnection(ioContext, serverHandler, server),
    sslStream_(boost::asio::make_strand(ioContext), sslContext)
{}

SSLStream* SSLServerConnection::GetSSLSocket()
{
    return &sslStream_;
}

void SSLServerConnection::DoRead()
{
    std::lock_guard<std::mutex> lck(resMtx);
    asyncReadPending = true;
    auto self(shared_from_this());
    std::shared_ptr<SSLServerConnection> sslPtr = std::dynamic_pointer_cast<SSLServerConnection>(self);
    auto &tcpStream = boost::beast::get_lowest_layer(sslStream_);
    req_.emplace();
    req_->body_limit(Configure::Singleton()->reqLimit.bodyLimit);
    req_->header_limit(ServerConnection::HEADER_LIMIT);
    if (keepAliveS > 0) {
        tcpStream.expires_after(std::chrono::seconds(keepAliveS));
    } else {
        size_t timeoutOneYear = 31622400;
        tcpStream.expires_at(std::chrono::steady_clock::now() + std::chrono::seconds(timeoutOneYear));
    }

    boost::beast::http::async_read(sslStream_, buffer_, *req_, boost::beast::bind_front_handler(
        &SSLServerConnection::OnSSLRead, sslPtr));
}

void SSLServerConnection::OnSSLRead(boost::beast::error_code ec, std::size_t bytes)
{
    ServerConnection::OnRead(ec, bytes);
}

void SSLServerConnection::DoWriteFinishRes(const ServerRes &res)
{
    asyncNotChunkPending = true;
    auto self(shared_from_this());
    std::shared_ptr<SSLServerConnection> sslPtr = std::dynamic_pointer_cast<SSLServerConnection>(self);
    res_ = {};
    res_.set(boost::beast::http::field::server, Configure::Singleton()->httpConfig.serverName);
    res_.set(boost::beast::http::field::content_type, res.contentType);
    res_.version(version_);
    res_.keep_alive(keepAlive_);
    res_.result(res.state);
    boost::beast::ostream(res_.body()) << res.body;
    res_.prepare_payload();
    auto &tcpStream = boost::beast::get_lowest_layer(sslStream_);
    if (timeout_ > 0) {
        tcpStream.expires_after(std::chrono::seconds(timeout_));
    } else {
        size_t timeoutOneYear = 31622400;
        tcpStream.expires_at(std::chrono::steady_clock::now() + std::chrono::seconds(timeoutOneYear));
    }
    boost::beast::http::async_write(sslStream_, res_,
        boost::beast::bind_front_handler(&SSLServerConnection::OnSSLWriteFinishRes, sslPtr, keepAlive_));
}

void SSLServerConnection::OnSSLWriteFinishRes(bool keepAlive, boost::beast::error_code ec, size_t bytes)
{
    ServerConnection::OnWriteFinishRes(keepAlive, ec, bytes);
}

void SSLServerConnection::WriteChunkHeader(const ServerRes &res)
{
    boost::beast::http::response<boost::beast::http::empty_body> headerRes{
    boost::beast::http::status::ok, version_};
    headerRes.set(boost::beast::http::field::server, Configure::Singleton()->httpConfig.serverName);
    headerRes.set(boost::beast::http::field::content_type, res.contentType);
    headerRes.chunked(true);
    headerRes.keep_alive(keepAlive_);
    boost::beast::http::response_serializer<boost::beast::http::empty_body> sr{headerRes};
    auto &tcpStream = boost::beast::get_lowest_layer(sslStream_);
    if (timeout_ > 0) {
        tcpStream.expires_after(std::chrono::seconds(timeout_));
    } else {
        size_t timeoutOneYear = 31622400;
        tcpStream.expires_at(std::chrono::steady_clock::now() + std::chrono::seconds(timeoutOneYear));
    }
    boost::beast::error_code ec;
    boost::beast::http::write_header(sslStream_, sr, ec);
    if (ec) {
        if (ec != boost::beast::http::error::end_of_stream && ec != boost::asio::ssl::error::stream_truncated) {
            LOG_E("[%s] [SSLServerConnection] Write header error %s.",
                GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::SSLSERVER_CONNECTION).c_str(),
                ec.what().c_str());
            auto fun = serverHandler_.GetFun(ServerHandlerType::RES_ERROR);
            if (fun != nullptr) {
                fun(shared_from_this());
            }
        }
        return DoClose(ec);
    }
}

void SSLServerConnection::WriteChunk(const ServerRes &res)
{
    auto self(shared_from_this());
    std::shared_ptr<SSLServerConnection> sslPtr = std::dynamic_pointer_cast<SSLServerConnection>(self);
    auto& body = res.body;
    auto& isLastChunk = res.isFinish;
    auto &tcpStream = boost::beast::get_lowest_layer(sslStream_);
    if (timeout_ > 0) {
        tcpStream.expires_after(std::chrono::seconds(timeout_));
    } else {
        size_t timeoutOneYear = 31622400;
        tcpStream.expires_at(std::chrono::steady_clock::now() + std::chrono::seconds(timeoutOneYear));
    }
    boost::asio::async_write(sslStream_, boost::beast::http::make_chunk(GetChunk(body)),
        boost::beast::bind_front_handler(&SSLServerConnection::OnSSLWriteChunk, sslPtr, isLastChunk));
}

void SSLServerConnection::OnSSLWriteChunk(bool isLastChunk, boost::beast::error_code ec, size_t bytes)
{
    ServerConnection::OnWriteChunk(isLastChunk, ec, bytes);
}

void SSLServerConnection::WriteChunkTailer()
{
    auto self = shared_from_this();
    std::shared_ptr<SSLServerConnection> sslPtr = std::dynamic_pointer_cast<SSLServerConnection>(self);
    auto &tcpStream = boost::beast::get_lowest_layer(sslStream_);
    if (timeout_ > 0) {
        tcpStream.expires_after(std::chrono::seconds(timeout_));
    } else {
        size_t timeoutOneYear = 31622400;
        tcpStream.expires_at(std::chrono::steady_clock::now() + std::chrono::seconds(timeoutOneYear));
    }

    boost::asio::async_write(sslStream_, boost::beast::http::make_chunk_last(),
        boost::beast::bind_front_handler(&SSLServerConnection::OnSSLWriteChunkTailer, sslPtr, keepAlive_));
}

void SSLServerConnection::OnSSLWriteChunkTailer(bool keepAlive, boost::beast::error_code ec, size_t bytes)
{
    ServerConnection::OnWriteChunkTailer(keepAlive, ec, bytes);
}

void SSLServerConnection::DoClose(boost::beast::error_code ec)
{
    if (!isClose) {
        isClose = true;
        server_.CloseOne(GetConnectionId());
        mTimer.cancel();
        boost::beast::error_code shutdownEc;
        sslStream_.shutdown(shutdownEc);
        if (shutdownEc) {
            LOG_D("[SSLServerConnection] Connection shutdown error is %s.", shutdownEc.what().c_str());
        }
        if (ec && ec != boost::beast::http::error::end_of_stream && ec != boost::asio::ssl::error::stream_truncated) {
            auto fun = serverHandler_.GetFun(ServerHandlerType::EXCEPTION_CLOSE);
            if (fun != nullptr) {
                fun(shared_from_this());
            }
        } else {
            auto fun = serverHandler_.GetFun(ServerHandlerType::CLOSE);
            if (fun != nullptr) {
                fun(shared_from_this());
            }
        }
    }
}

}