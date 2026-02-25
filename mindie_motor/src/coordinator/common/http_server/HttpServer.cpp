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
#include <boost/beast/ssl.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/core.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/config.hpp>
#include <boost/asio.hpp>

#include "Logger.h"
#include "HttpServer.h"

namespace MINDIE::MS {
HttpServer::HttpServer() : signals_(ioContext_) {}

int32_t HttpServer::Init(size_t ioContextPoolSize, size_t maxConn)
{
    auto ret = ioContextPool_.Init(ioContextPoolSize);
    if (ret != 0) {
        LOG_E("[%s] [HttpServer] Iocontext pool initialize failed.",
            GetErrorCode(ErrorType::INVALID_PARAMETER, CoordinatorFeature::HTTPSERVER).c_str());
        return -1;
    }
    signals_.add(SIGINT);
    signals_.add(SIGTERM);
#if defined(SIGQUIT)
    signals_.add(SIGQUIT);
#endif // defined(SIGQUIT)
    DoAwaitStop();
    maxConnection = maxConn;
    return 0;
}

void HttpServer::Run(const std::vector<HttpServerParm>& parms, std::shared_ptr<std::atomic<bool>> ready)
{
    LOG_M("[Start] Http server start.");
    ioContextPool_.Run();
    auto size = parms.size();
    acceptor_.resize(size);
    serverHandler_.resize(size);
    timeout_.resize(size);
    maxKeepAliveReqs_.resize(size);
    keepAliveS.resize(size);
    ctx.resize(size);
    verifyItems.resize(size);
    try {
        for (uint32_t i = 0; i < size; ++i) {
            auto acceptor = std::make_unique<boost::asio::ip::tcp::acceptor>(ioContext_);
            boost::asio::ip::tcp::resolver resolver(acceptor->get_executor());
            boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(parms[i].address, parms[i].port).begin();
            acceptor->open(endpoint.protocol());
            acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
            acceptor->bind(endpoint);
            acceptor->listen();
            serverHandler_[i] = parms[i].serverHandler;
            acceptor_[i] = std::move(acceptor);
            timeout_[i] = parms[i].timeout;
            maxKeepAliveReqs_[i] = parms[i].maxKeepAliveReqs;
            keepAliveS[i] = parms[i].keepAliveS;
            ctx[i] = std::make_unique<SSLContext>(SSLContext::tlsv13_server);
            if (parms[i].tlsItems.tlsEnable && SetTlsCtx(*(ctx[i]), parms[i].tlsItems, verifyItems[i]) != 0) {
                LOG_E("[%s] [HttpServer] Set TLS context %u failed.",
                    GetErrorCode(ErrorType::CALL_ERROR, CoordinatorFeature::HTTPSERVER).c_str(), i);
                return;
            }
            DoAccept(i, parms[i].tlsItems);
        }
        if (ready != nullptr) {
            ready->store(true);
        }
        ioContext_.run(); // 这里是阻塞的
    } catch (const std::exception& e) {
        LOG_E("[%s] [HttpServer] Run error: %s",
            GetErrorCode(ErrorType::EXCEPTION, CoordinatorFeature::HTTPSERVER).c_str(),
            e.what());
    }
}

void HttpServer::HandleAccept(std::shared_ptr<ServerConnection> newConnection, uint32_t i, TlsItems tlsItems,
    const boost::system::error_code& ec)
{
    if (!acceptor_[i]->is_open()) {
        return;
    }
    if (ec) {
        LOG_E("[%s] [HttpServer] Connection accept failed, error is %s\n",
            GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::HTTPSERVER).c_str(),
            ec.what().c_str());
    } else {
        std::unique_lock<std::mutex> lock(mtx);
        auto connId = newConnection->GetConnectionId();
        auto ret = connIdSet.insert(connId);
        if (!ret.second) {
            lock.unlock();
            LOG_E("[%s] [HttpServer] Failed to add new connection. Connection attempt failed.",
                GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::HTTPSERVER).c_str());
            newConnection->DoClose(ec);
        } else {
            auto nowSize = connIdSet.size();
            lock.unlock();
            if (nowSize > maxConnection) {
                LOG_E("[%s] [HttpServer] Connection size(%u) is out of limit(%u).",
                    GetErrorCode(ErrorType::RESOURCE_LIMIT, CoordinatorFeature::HTTPSERVER).c_str(), nowSize,
                    maxConnection);
                newConnection->DoClose(ec);
            } else {
                newConnection->Start(timeout_[i], maxKeepAliveReqs_[i], keepAliveS[i]);
            }
        }
    }
    DoAccept(i, tlsItems);
}

void HttpServer::HandleHandshake(std::shared_ptr<ServerConnection> newConnection, uint32_t i, TlsItems tlsItems,
    const boost::system::error_code& ec)
{
    if (ec) {
        LOG_E("[%s] [HttpServer] Connection accept failed before handshake, error is %s\n",
            GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::HTTPSERVER).c_str(),
            ec.what().c_str());
    } else {
        auto sslStream = newConnection->GetSSLSocket();
        if (sslStream == nullptr) {
            LOG_E("[%s] [HttpServer] Failed to retrieve SSL socket during handshake.",
                GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::HTTPSERVER).c_str());
            return;
        }
        sslStream->async_handshake(
            boost::asio::ssl::stream_base::server,
            boost::beast::bind_front_handler(&HttpServer::HandleAccept, this, newConnection, i, tlsItems)
        );
    }
}

uint32_t HttpServer::GetConnectionId()
{
    connectionIDIndex++;
    if (connectionIDIndex == UINT32_MAX) {
        connectionIDIndex = 0;
    }
    return connectionIDIndex;
}

void HttpServer::DoAccept(uint32_t i, const TlsItems& tlsItems)
{
    auto &ioContext = ioContextPool_.GetHttpIoCtx();

    auto id = GetConnectionId();
    // count_ 作为ServerConnection 的id传下去
    LOG_D("[HttpServer] Async accept success, create server connection with id %u", id);
    if (tlsItems.tlsEnable) {
        std::shared_ptr<ServerConnection> newConnection = std::make_shared<SSLServerConnection>(ioContext,
            serverHandler_[i], *this, *(ctx[i]));
        auto stream = newConnection->GetSSLSocket();
        if (stream == nullptr) {
            LOG_E("[%s] [HttpServer] Failed to get SSL socket for connection id %u during handshake.",
                GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::HTTPSERVER).c_str(), id);
            return;
        }
        auto &tcpStream = boost::beast::get_lowest_layer(*stream);
        acceptor_[i]->async_accept(tcpStream.socket(),
            boost::beast::bind_front_handler(&HttpServer::HandleHandshake, this, newConnection, i, tlsItems));
        newConnection->SetConnectionID(id);
    } else {
        std::shared_ptr<ServerConnection> newConnection = std::make_shared<ServerConnection>(ioContext,
            serverHandler_[i], *this);
        acceptor_[i]->async_accept(newConnection->GetSocket().socket(),
            boost::beast::bind_front_handler(&HttpServer::HandleAccept, this, newConnection, i, tlsItems));
        newConnection->SetConnectionID(id);
    }
}

void HttpServer::DoAwaitStop()
{
    signals_.async_wait([this](boost::system::error_code ec, int) {
        if (!ec) {
            ioContextPool_.Stop();
            ioContext_.stop();
            LOG_M("[Stop] Http server stop\n");
        }
    });
}

bool ServerDecrypt(SSLContext &mSslCtx, const TlsItems &tlsItems,
    std::pair<char *, int32_t> &mPassword)
{
    if (DecryptPassword(1, mPassword, tlsItems)) {
        auto sslCtx = mSslCtx.native_handle();
        SSL_CTX_set_default_passwd_cb_userdata(sslCtx, mPassword.first);
        return true;
    } else {
        LOG_E("[%s] Decrypt password failed.",
            GetErrorCode(ErrorType::CALL_ERROR, CoordinatorFeature::HTTPSERVER).c_str());
        return false;
    }
}

static int32_t SetCipherSuites(SSLContext &mSslCtx)
{
    int32_t ret = 0;
    ret = SSL_CTX_set_ciphersuites(mSslCtx.native_handle(),
        "TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_CCM_SHA256");
    return ret;
}

static int32_t UseCertificate(SSLContext &mSslCtx, const TlsItems &tlsItems)
{
    std::string caStr = "";
    uint32_t mode = tlsItems.checkFiles ? 0400 : 0777; // 证书的权限要求是0400, 不校验是0777
    if (FileToBuffer(tlsItems.caCert, caStr, mode, tlsItems.checkFiles) != 0) {
        LOG_E("[%s] Cannot convert CA file %s to buffer.",
            GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::HTTPSERVER).c_str(),
            tlsItems.caCert.c_str());
        return -1;
    }
    boost::system::error_code ec;
    mSslCtx.add_certificate_authority(boost::asio::buffer(caStr.data(), caStr.size()), ec);
    if (ec) {
        LOG_E("[%s] HTTP server failed to add certificate authority from file %s. Reason: %s.",
            GetErrorCode(ErrorType::INVALID_INPUT, CoordinatorFeature::HTTPSERVER).c_str(),
            tlsItems.caCert.c_str(), ec.message().c_str());
        return -1;
    }
    std::string certPath = tlsItems.tlsCert;
    bool isFileExist = false;
    if (!PathCheck(certPath, isFileExist, mode, tlsItems.checkFiles)) {
        LOG_E("[%s] Certificate path check failed.",
            GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::HTTPSERVER).c_str());
        return -1;
    }
    if (!isFileExist) {
        LOG_E("[%s] Certificate file is not exist.",
            GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::HTTPSERVER).c_str());
        return -1;
    }
    mSslCtx.use_certificate_chain_file(certPath, ec);
    if (ec) {
        LOG_E("[%s] Http server failed on use certificate chain file, reason: %s.",
            GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::HTTPSERVER).c_str(),
            ec.message().c_str());
        return -1;
    }
    return 0;
}

static int32_t UsePrivateKey(SSLContext &mSslCtx, const TlsItems &tlsItems)
{
    boost::system::error_code ec;
    std::string keyPath = tlsItems.tlsKey;
    bool isFileExist = false;
    uint32_t mode = tlsItems.checkFiles ? 0400 : 0640; // 密钥的权限要求是0400, 不校验是0640
    if (!PathCheck(keyPath, isFileExist, mode, tlsItems.checkFiles)) {
        LOG_E("[%s] Key file path check failed.",
            GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::HTTPSERVER).c_str());
        return -1;
    }
    if (!isFileExist) {
        LOG_E(
            "[%s] Key file is not exist.", GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::HTTPSERVER).c_str());
        return -1;
    }
    mSslCtx.use_private_key_file(keyPath, SSLContext::pem, ec);
    if (ec) {
        LOG_E("[%s] HTTP server failed on use private key file, reason: %s.",
            GetErrorCode(ErrorType::INVALID_INPUT, CoordinatorFeature::HTTPSERVER).c_str(),
            ec.message().c_str());
        return -1;
    }
    return 0;
}

static void SetVerifyCallback(SSLContext &mSslCtx, const TlsItems &tlsItems,
    VerifyItems &verifyItems)
{
    verifyItems.checkSubject = false;
    verifyItems.crlFile = tlsItems.tlsCrl;
    verifyItems.checkFiles = tlsItems.checkFiles;
    SSL_CTX_set_cert_verify_callback(mSslCtx.native_handle(), VerifyCallback, static_cast<void *>(&verifyItems));
}

int32_t HttpServer::SetTlsCtx(SSLContext &sslCtx, const TlsItems &tlsItems, VerifyItems &verifyItem) const
{
    auto ret = SetCipherSuites(sslCtx);
    if (ret == 0) {
        LOG_E("[%s] Server Failed to set cipher suites to TLS context",
            GetErrorCode(ErrorType::INVALID_PARAMETER, CoordinatorFeature::HTTPSERVER).c_str());
        return -1;
    }

    ret = UseCertificate(sslCtx, tlsItems);
    if (ret != 0) {
        return -1;
    }
    std::pair<char *, int32_t> mPassword = {nullptr, 0};
    if (!ServerDecrypt(sslCtx, tlsItems, mPassword)) {
        return -1;
    }

    LOG_M("[Load] Load TLS private key file %s.", tlsItems.tlsKey.c_str());
    ret = UsePrivateKey(sslCtx, tlsItems);
    if (ret != 0) {
        EraseDecryptedData(mPassword);
        return -1;
    }
    EraseDecryptedData(mPassword);
    boost::system::error_code ec;
    sslCtx.set_verify_mode(boost::asio::ssl::verify_peer | boost::asio::ssl::verify_fail_if_no_peer_cert, ec);
    if (ec) {
        LOG_E("[%s] HTTP server failed on set verification mode, reason: %s.",
            GetErrorCode(ErrorType::INVALID_PARAMETER, CoordinatorFeature::HTTPSERVER).c_str(),
            ec.message().c_str());
        return -1;
    }
    SetVerifyCallback(sslCtx, tlsItems, verifyItem);
    return 0;
}

void HttpServer::CloseOne(uint32_t connId)
{
    std::unique_lock<std::mutex> lock(mtx);
    auto iter = connIdSet.find(connId);
    if (iter != connIdSet.end()) {
        connIdSet.erase(iter);
    }
}

}