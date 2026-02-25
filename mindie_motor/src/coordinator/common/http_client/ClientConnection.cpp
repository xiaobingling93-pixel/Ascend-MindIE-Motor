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
#include "HttpClientAsync.h"
#include "Configure.h"
#include "ClientConnection.h"

namespace MINDIE::MS {
ClientConnection::ClientConnection(boost::asio::io_context& ioContext, const ClientHandler& clientHandler,
    size_t timeout) : isClose(false), available(true), id_(0), timeout_(timeout), ioContext_(ioContext),
    stream_(boost::asio::make_strand(ioContext)), clientHandler_(clientHandler) {}

void ClientConnection::Start(uint32_t id, boost::beast::string_view ip, boost::beast::string_view port,
    HttpClientAsync *client)
{
    auto self(shared_from_this());
    id_ = id;
    ip_ = ip;
    port_ = port;
    client_ = client;
    headerCb = std::bind(&ClientConnection::ParserChunkHeader, self, std::placeholders::_1,
        std::placeholders::_2, std::placeholders::_3);
    bodyCb = std::bind(&ClientConnection::ParserChunkBody, self, std::placeholders::_1,
        std::placeholders::_2, std::placeholders::_3);
    DoReadHeader();
}

TCPStream& ClientConnection::GetSocket()
{
    return stream_;
}

const std::string& ClientConnection::GetIp() const
{
    return ip_;
}

const std::string& ClientConnection::GetPort() const
{
    return port_;
}

void ClientConnection::SendReq(const boost::beast::http::request<boost::beast::http::dynamic_body> &req,
    boost::beast::string_view reqId)
{
    mReqId = reqId;
    if (isClose) {
        auto fun = clientHandler_.GetFun(ClientHandlerType::REQ_ERROR);
        if (fun != nullptr) {
            fun(shared_from_this());
        }
        return;
    }

    auto isBegin = writeMsgs.Empty();
    writeMsgs.PushBack(req);
    if (isBegin) {
        auto self(shared_from_this());
        boost::asio::post(ioContext_, [self]() {
            self->DoWrite();
        });
    }
}

std::string ClientConnection::GetReqId() const
{
    return mReqId;
}

void ClientConnection::DoWrite()
{
    if (isClose) {
        auto fun = clientHandler_.GetFun(ClientHandlerType::REQ_ERROR);
        if (fun != nullptr) {
            fun(shared_from_this());
        }
        return;
    }

    if (graceClose.load()) {
        return DoClose();
    }

    if (writeMsgs.Empty()) {
        return;
    }
    auto& req = writeMsgs.Front();
    req.set(boost::beast::http::field::host, ip_);
    auto self(shared_from_this());
    ResetExpiresTime();
    boost::beast::http::async_write(stream_, req,
        boost::beast::bind_front_handler(&ClientConnection::OnWrite, self));
}

void ClientConnection::OnWrite(boost::beast::error_code ec, std::size_t bytes)
{
    boost::ignore_unused(bytes);
    if (ec) {
        if (ec != boost::beast::http::error::end_of_stream) {
            LOG_E("[%s] [ClientConnection]Error occurred during write operation: %s.",
                GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::CLIENT_CONNECTION).c_str(),
                ec.what().c_str());
            auto fun = clientHandler_.GetFun(ClientHandlerType::REQ_ERROR);
            if (fun != nullptr) {
                fun(shared_from_this());
            }
        }
        return DoClose();
    }
    if (isClose) {
        return;
    }

    if (graceClose.load()) {
        return DoClose();
    }

    auto fun = clientHandler_.GetFun(ClientHandlerType::REQ);
    if (fun != nullptr) {
        fun(shared_from_this());
    }
    if (!writeMsgs.Empty()) {
        writeMsgs.PopFront();
    }
    if (!writeMsgs.Empty()) {
        DoWrite();
    }
}

void ClientConnection::DoReadHeader()
{
    if (isClose) {
        return;
    }

    if (graceClose.load()) {
        return DoClose();
    }

    auto self(shared_from_this());
    p_.emplace();
    p_->header_limit(ClientConnection::HEADER_LIMIT);
    ResetExpiresTime();
    boost::beast::http::async_read_header(stream_, buffer_, *p_,
        boost::beast::bind_front_handler(&ClientConnection::OnReadHeader, self));
}

void ClientConnection::OnReadHeader(boost::beast::error_code ec, std::size_t bytes)
{
    boost::ignore_unused(bytes);
    auto self(shared_from_this());
    if (ec) {
        if (ec != boost::beast::http::error::end_of_stream) {
            LOG_E("[%s] [ClientConnection] Error occurred during read operation: %s.",
                GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::CLIENT_CONNECTION).c_str(), ec.what().c_str());
            auto fun = clientHandler_.GetFun(ClientHandlerType::HEADER_RES_ERROR);
            if (fun != nullptr) {
                fun(self);
            }
        }
        return DoClose();
    }
    if (isClose) {
        return;
    }

    if (graceClose.load()) {
        return DoClose();
    }

    auto fun = clientHandler_.GetFun(ClientHandlerType::HEADER_RES);
    if (fun != nullptr) {
        fun(self);
    }
    if (p_->chunked()) {
        LOG_D("[ClientConnection] Response is chunked transfer encoding.");
        p_->body_limit(boost::none);
        p_->on_chunk_header(headerCb);
        p_->on_chunk_body(bodyCb);
        if (!p_->is_done()) {
            ResetExpiresTime();
            boost::beast::http::async_read(stream_, buffer_, *p_,
                boost::beast::bind_front_handler(&ClientConnection::OnChunkBody, self));
        }
    } else {
        LOG_D("[ClientConnection] Response is normal transfer encoding.");
        p_->body_limit(Configure::Singleton()->reqLimit.bodyLimit);
        if (!p_->is_done() || p_->get().result() == boost::beast::http::status::continue_) {
            ResetExpiresTime();
            boost::beast::http::async_read(stream_, buffer_, *p_,
                boost::beast::bind_front_handler(&ClientConnection::OnReadBody, self));
        }
    }
}

void ClientConnection::ParserChunkHeader(uint64_t size, boost::beast::string_view extensions,
    boost::beast::error_code& ec)
{
    boost::ignore_unused(extensions);
    if (ec) {
        LOG_E("[%s] [ClientConnection] Error occurred when parsing chunk header, error is %s.",
            GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::CLIENT_CONNECTION).c_str(),
            ec.what().c_str());
        return;
    }
    if (size > Configure::Singleton()->reqLimit.bodyLimit) {
        ec = boost::beast::http::error::body_limit;
        return;
    }
    chunk.reserve(size);
    chunk.clear();
}

size_t ClientConnection::ParserChunkBody(uint64_t remain, boost::beast::string_view body, boost::beast::error_code& ec)
{
    if (ec) {
        LOG_E("[%s] [ClientConnection]  Error occurred when parsing chunk body, error is %s.",
            GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::CLIENT_CONNECTION).c_str(),
            ec.what().c_str());
        return body.size();
    }
    if (remain == body.size()) {
        ec = boost::beast::http::error::end_of_chunk;
    }
    chunk.append(body.data(), body.size());
    return body.size();
}

void ClientConnection::OnChunkBody(boost::beast::error_code ec, size_t bytes)
{
    boost::ignore_unused(bytes);
    auto self(shared_from_this());
    if (ec) {
        if (ec != boost::beast::http::error::end_of_chunk) {
            LOG_E("[%s] [ClientConnection] Error occurred while processing chunk body, error is %s.",
                GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::CLIENT_CONNECTION).c_str(),
                ec.what().c_str());
            auto fun = clientHandler_.GetFun(ClientHandlerType::CHUNK_BODY_RES_ERROR);
            if (fun != nullptr) {
                fun(self);
            }
            return DoClose();
        }
    }
    if (isClose) {
        return;
    }

    if (graceClose.load()) {
        return DoClose();
    }

    auto fun = clientHandler_.GetFun(ClientHandlerType::CHUNK_BODY_RES);
    if (fun != nullptr) {
        fun(self);
    }

    if (!p_) {
        return;
    }
    if (p_->is_done()) {
        // Gracefully close the socket
        if (timeout_ != 0 && !p_->keep_alive()) {
            LOG_D("[ClientConnection] Chunked response read finished, closing connection. address: %s.",
                GetAddress().c_str());
            return DoClose();
        } else {
            LOG_D("[ClientConnection] Chunked response read finished, keep connection alive. address: %s.",
                GetAddress().c_str());
            p_.reset();
            DoReadHeader();
        }
    } else {
        ResetExpiresTime();
        boost::beast::http::async_read(stream_, buffer_, *p_,
            boost::beast::bind_front_handler(&ClientConnection::OnChunkBody, self));
    }
}

void ClientConnection::OnReadBody(boost::beast::error_code ec, std::size_t bytes)
{
    if (ec) {
        LOG_E("[%s] [ClientConnection] Error occurred while processing read body, error is %s.",
            GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::CLIENT_CONNECTION).c_str(),
            ec.what().c_str());
        return DoClose();
    }

    if (isClose) {
        return;
    }

    if (graceClose.load()) {
        return DoClose();
    }

    if (bytes > 0) {
        auto fun = clientHandler_.GetFun(ClientHandlerType::RES);
        if (fun != nullptr) {
            fun(shared_from_this());
        }
    }
    if (timeout_ != 0 && !p_->keep_alive()) {
        return DoClose();
    }
    p_.reset();
    DoReadHeader();
}

boost::beast::http::response<boost::beast::http::dynamic_body>& ClientConnection::GetResMessage()
{
    return p_->get();
}

const std::string& ClientConnection::GetResChunkedBody() const
{
    return chunk;
}

void ClientConnection::GraceClose()
{
    std::unique_lock<std::mutex> lock(mtx);
    if (available) {
        graceClose.store(true);
    }
}

void ClientConnection::DoClose()
{
    this->Close();
    if (client_ != nullptr) {
        client_->RemoveConnection(id_);
    }
}

bool ClientConnection::ResIsFinish() const
{
    return p_->is_done();
}

uint32_t ClientConnection::GetConnId() const
{
    return id_;
}

void ClientConnection::Close()
{
    if (isClose) {
        return;
    }
    isClose = true;
    // Send a TCP shutdown
    boost::beast::error_code ec;
    stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    if (ec) {
        LOG_E("[%s] [ClientConnection] Connection shutdown failed with error %s",
            GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::CLIENT_CONNECTION).c_str(),
            ec.what().c_str());
    }
    // At this point the connection is closed gracefully
    if (p_) {
        p_.reset();
    }
    headerCb = nullptr;
    bodyCb = nullptr;
}

boost::beast::string_view ClientConnection::GetContentType() const
{
    return p_->get().at(boost::beast::http::field::content_type);
}

bool ClientConnection::IsAvailable() const
{
    return available.load();
}

bool ClientConnection::IsClose() const
{
    return graceClose.load();
}

void ClientConnection::SetHandler(const ClientHandler& clientHandler)
{
    clientHandler_ = clientHandler;
}

void ClientConnection::SetAvailable(bool isAvailable)
{
    std::unique_lock<std::mutex> lock(mtx);
    available = isAvailable;
}

boost::beast::http::request<boost::beast::http::dynamic_body>& ClientConnection::GetReq()
{
    return writeMsgs.Front();
}

void ClientConnection::SetTimeout(size_t timeout)
{
    timeout_ = timeout;
}

void ClientConnection::ResetExpiresTime()
{
    if (isClose) {
        return;
    }
    LOG_D("[ClientConnection] Reset connection expires time. address: %s.", GetAddress().c_str());
    if (timeout_ > 0) {
        stream_.expires_after(std::chrono::seconds(timeout_));
    } else {
        size_t timeoutOneYear = 31622400;
        stream_.expires_at(std::chrono::steady_clock::now() + std::chrono::seconds(timeoutOneYear));
    }
}

SSLClientConnection::SSLClientConnection(boost::asio::io_context& ioContext, const ClientHandler& clientHandler,
    SSLContext &sslContext, size_t timeout) : ClientConnection(ioContext, clientHandler, timeout),
    sslStream_(boost::asio::make_strand(ioContext), sslContext) {}

SSLStream* SSLClientConnection::GetSSLSocket()
{
    return &sslStream_;
}

void SSLClientConnection::DoWrite()
{
    if (isClose) {
        auto fun = clientHandler_.GetFun(ClientHandlerType::REQ_ERROR);
        if (fun != nullptr) {
            fun(shared_from_this());
        }
        return;
    }
    if (writeMsgs.Empty()) {
        return;
    }
    auto& req = writeMsgs.Front();
    req.set(boost::beast::http::field::host, ip_);
    auto self(shared_from_this());
    std::shared_ptr<SSLClientConnection> sslPtr = std::dynamic_pointer_cast<SSLClientConnection>(self);
    auto &tcpStream = boost::beast::get_lowest_layer(sslStream_);
    if (timeout_ > 0) {
        tcpStream.expires_after(std::chrono::seconds(timeout_));
    } else {
        size_t timeoutOneYear = 31622400;
        tcpStream.expires_at(std::chrono::steady_clock::now() + std::chrono::seconds(timeoutOneYear));
    }
    boost::beast::http::async_write(sslStream_, req,
        boost::beast::bind_front_handler(&SSLClientConnection::OnSSLWrite, sslPtr));
}

void SSLClientConnection::OnSSLWrite(boost::beast::error_code ec, std::size_t bytes)
{
    ClientConnection::OnWrite(ec, bytes);
}

void SSLClientConnection::DoReadHeader()
{
    if (isClose) {
        return;
    }
    auto self(shared_from_this());
    std::shared_ptr<SSLClientConnection> sslPtr = std::dynamic_pointer_cast<SSLClientConnection>(self);
    p_.emplace();
    p_->header_limit(ClientConnection::HEADER_LIMIT);
    auto &tcpStream = boost::beast::get_lowest_layer(sslStream_);
    if (timeout_ > 0) {
        tcpStream.expires_after(std::chrono::seconds(timeout_));
    } else {
        size_t timeoutOneYear = 31622400;
        tcpStream.expires_at(std::chrono::steady_clock::now() + std::chrono::seconds(timeoutOneYear));
    }
    boost::beast::http::async_read_header(sslStream_, buffer_, *p_,
        boost::beast::bind_front_handler(&SSLClientConnection::OnReadHeader, sslPtr));
}

void SSLClientConnection::OnSSLReadBody(boost::beast::error_code ec, std::size_t bytes)
{
    ClientConnection::OnReadBody(ec, bytes);
}

void SSLClientConnection::OnReadHeader(boost::beast::error_code ec, std::size_t bytes)
{
    boost::ignore_unused(bytes);
    auto self(shared_from_this());
    std::shared_ptr<SSLClientConnection> sslPtr = std::dynamic_pointer_cast<SSLClientConnection>(self);
    if (ec) {
        if (ec != boost::beast::http::error::end_of_stream && ec != boost::asio::ssl::error::stream_truncated) {
            LOG_E("[%s] [SSLClientConnection] Error occurred while reading header, error is %s.",
                GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::CLIENT_CONNECTION).c_str(),
                ec.what().c_str());
            auto fun = clientHandler_.GetFun(ClientHandlerType::HEADER_RES_ERROR);
            if (fun != nullptr) {
                fun(self);
            }
        }
        return DoClose();
    }
    if (isClose) {
        return;
    }
    auto fun = clientHandler_.GetFun(ClientHandlerType::HEADER_RES);
    if (fun != nullptr) {
        fun(self);
    }
    auto &tcpStream = boost::beast::get_lowest_layer(sslStream_);
    if (p_->chunked()) {
        p_->body_limit(boost::none);
        p_->on_chunk_header(headerCb);
        p_->on_chunk_body(bodyCb);
        if (!p_->is_done()) {
            if (timeout_ > 0) {
                tcpStream.expires_after(std::chrono::seconds(timeout_));
            } else {
                size_t timeoutOneYear = 31622400;
                tcpStream.expires_at(std::chrono::steady_clock::now() + std::chrono::seconds(timeoutOneYear));
            }
            boost::beast::http::async_read(sslStream_, buffer_, *p_,
                boost::beast::bind_front_handler(&SSLClientConnection::OnChunkBody, sslPtr));
        }
    } else {
        p_->body_limit(Configure::Singleton()->reqLimit.bodyLimit);
        if (!p_->is_done() || p_->get().result() == boost::beast::http::status::continue_) {
            if (timeout_ > 0) {
                tcpStream.expires_after(std::chrono::seconds(timeout_));
            } else {
                size_t timeoutOneYear = 31622400;
                tcpStream.expires_at(std::chrono::steady_clock::now() + std::chrono::seconds(timeoutOneYear));
            }
            boost::beast::http::async_read(sslStream_, buffer_, *p_,
                boost::beast::bind_front_handler(&SSLClientConnection::OnSSLReadBody, sslPtr));
        }
    }
}

void SSLClientConnection::OnChunkBody(boost::beast::error_code ec, size_t bytes)
{
    boost::ignore_unused(bytes);
    auto self(shared_from_this());
    std::shared_ptr<SSLClientConnection> sslPtr = std::dynamic_pointer_cast<SSLClientConnection>(self);
    if (ec) {
        if (ec != boost::beast::http::error::end_of_chunk && ec != boost::asio::ssl::error::stream_truncated) {
            LOG_E("[%s] [SSLClientConnection] Error occurred while processing chunk body, error is %s.",
                GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::CLIENT_CONNECTION).c_str(),
                ec.what().c_str());
            auto fun = clientHandler_.GetFun(ClientHandlerType::CHUNK_BODY_RES_ERROR);
            if (fun != nullptr) {
                fun(self);
            }
            return DoClose();
        }
    }
    if (isClose) {
        return;
    }
    auto fun = clientHandler_.GetFun(ClientHandlerType::CHUNK_BODY_RES);
    if (fun != nullptr) {
        fun(self);
    }
    if (p_->is_done()) {
        // Gracefully close the socket
        if (timeout_ != 0 && !p_->keep_alive()) {
            return DoClose();
        } else {
            p_.reset();
            DoReadHeader();
        }
    } else {
        auto &tcpStream = boost::beast::get_lowest_layer(sslStream_);
        if (timeout_ > 0) {
            tcpStream.expires_after(std::chrono::seconds(timeout_));
        } else {
            size_t timeoutOneYear = 31622400;
            tcpStream.expires_at(std::chrono::steady_clock::now() + std::chrono::seconds(timeoutOneYear));
        }
        boost::beast::http::async_read(sslStream_, buffer_, *p_,
            boost::beast::bind_front_handler(&SSLClientConnection::OnChunkBody, sslPtr));
    }
}

void SSLClientConnection::Close()
{
    if (isClose) {
        return;
    }
    isClose = true;
    // Send a TCP shutdown
    boost::beast::error_code ec;
    sslStream_.shutdown(ec);
    if (ec) {
        LOG_E("[%s] [SSLClientConnection] Connection shutdown failed with error %s",
            GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::CLIENT_CONNECTION).c_str(),
            ec.what().c_str());
    }
    // At this point the connection is closed gracefully
    if (p_) {
        p_.reset();
    }
    headerCb = nullptr;
    bodyCb = nullptr;
}

}