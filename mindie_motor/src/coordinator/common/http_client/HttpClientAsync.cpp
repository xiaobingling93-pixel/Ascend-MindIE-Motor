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
#include <algorithm>
#include "Logger.h"
#include "HttpClient.h"
#include "Configure.h"
#include "HttpClientAsync.h"

namespace MINDIE::MS {
HttpClientAsync::HttpClientAsync() : resolver_(ioContext_) {}

HttpClientAsync::~HttpClientAsync()
{
    Stop();
}

int32_t HttpClientAsync::Init(TlsItems &clientTlsItems, uint32_t ioContextPoolSize, uint32_t maxConnection)
{
    items = clientTlsItems;
    if (clientTlsItems.tlsEnable) {
        if (SetTlsCtx(ctx, clientTlsItems) != 0) {
            LOG_E("[%s] [HttpClientAsync] Set TLS context for client failed.",
                GetErrorCode(ErrorType::CALL_ERROR, CoordinatorFeature::HTTPCLIENT_ASYNC).c_str());
            return -1;
        }
    }
    maxConnection_ = maxConnection;
    auto ret = ioContextPool_.Init(ioContextPoolSize);
    if (ret != 0) {
        LOG_E("[%s] [HttpClientAsync] Failed to initialize I/O context pool.",
            GetErrorCode(ErrorType::INVALID_INPUT, CoordinatorFeature::HTTPCLIENT_ASYNC).c_str());
        return -1;
    }
    ioContextPool_.Run();
    LOG_M("[Start] HTTP client start.");
    return 0;
}

bool HttpClientAsync::CreateNewConnId(uint32_t &id, std::shared_ptr<ClientConnection> newConnection)
{
    std::unique_lock<std::mutex> lock(mtx);
    for (id = 0; id <= maxConnection_; ++id) {
        auto iter = connections_.find(id);
        if (iter == connections_.end()) {
            break;
        }
    }
    if (id > maxConnection_) {
        LOG_E("[%s] [HttpClientAsync] Connection id cannot be larger than %u, but got %u",
            GetErrorCode(ErrorType::RESOURCE_LIMIT, CoordinatorFeature::HTTPCLIENT_ASYNC).c_str(), maxConnection_, id);
        return false;
    }
    connections_[id] = newConnection;
    return true;
}

void HttpClientAsync::RemoveConnection(uint32_t id)
{
    std::unique_lock<std::mutex> lock(mtx);
    auto iter = connections_.find(id);
    if (iter != connections_.end()) {
        connections_.erase(iter);
    }
}

std::shared_ptr<ClientConnection> HttpClientAsync::GetConnection(uint32_t id)
{
    std::unique_lock<std::mutex> lock(mtx);
    auto iter = connections_.find(id);
    if (iter != connections_.end()) {
        return iter->second;
    }
    return nullptr;
}

bool HttpClientAsync::AddConnectionTLS(std::shared_ptr<ClientConnection> &newConnection,
    boost::asio::io_context& ioContext, const ClientHandler& clientHandler, uint32_t timeout,
    boost::asio::ip::tcp::resolver::results_type &results)
{
    boost::beast::error_code ec;
    std::string errorCode = GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::HTTPCLIENT_ASYNC);
    try {
        newConnection = std::make_shared<SSLClientConnection>(ioContext, clientHandler, ctx, timeout);
    } catch (const std::exception& e) {
        LOG_E("[%s] [HttpClientAsync] Exception while creating SSL connection: %s.",
            GetErrorCode(ErrorType::EXCEPTION, CoordinatorFeature::HTTPCLIENT_ASYNC).c_str(), e.what());
        return false;
    }
    auto stream = newConnection->GetSSLSocket();
    if (stream == nullptr) {
        LOG_E("[%s] [HttpClientAsync] Add TLS connection failed, failed to retrieve SSL socket.", errorCode.c_str());
        return false;
    }
    connectionCompleted_ = false;
    auto& tcpStream = boost::beast::get_lowest_layer(*stream);
    tcpStream.expires_after(std::chrono::seconds(5)); // 超时5秒
    tcpStream.async_connect(results,
        std::bind(&HttpClientAsync::OnConnection, this, std::placeholders::_1, std::placeholders::_2));
    std::unique_lock<std::mutex> lock(conMutex_);
    conV_.wait(lock, [this] { return connectionCompleted_; });
    if (isException) {
        return false;
    }
    stream->handshake(boost::asio::ssl::stream_base::client, ec);
    if (ec) {
        LOG_E("[%s] [HttpClientAsync] Add TLS connection failed, connection handshake failed, error is %s",
            errorCode.c_str(), ec.what().c_str());
        return false;
    }
    return true;
}

bool HttpClientAsync::AddConnection(boost::beast::string_view ip, boost::beast::string_view port, uint32_t &id,
    const ClientHandler& clientHandler, uint32_t timeout)
{
    boost::beast::error_code ec;
    auto results = resolver_.resolve(ip, port, ec);
    std::string errorCode = GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::HTTPCLIENT_ASYNC);
    if (ec) {
        LOG_E("[%s] [HttpClientAsync] Resolve failed for %s:%s, error is %s.",
            errorCode.c_str(), ip.data(), port.data(), ec.what().c_str());
        return false;
    }
    auto &ioContext = ioContextPool_.GetHttpIoCtx();
    std::shared_ptr<ClientConnection> newConnection;
    if (items.tlsEnable) {
        if (!AddConnectionTLS(newConnection, ioContext, clientHandler, timeout, results)) {
            LOG_E("[%s] [HttpClientAsync] Add TLS connection failed for %s:%s.",
                errorCode.c_str(), ip.data(), port.data());
            return false;
        }
    } else {
        try {
            newConnection = std::make_shared<ClientConnection>(ioContext, clientHandler, timeout);
        } catch (const std::exception& e) {
            LOG_E("[%s] [HttpClientAsync::AddConnection] Exception while creating connection for %s:%s, error is %s.",
                GetErrorCode(ErrorType::EXCEPTION, CoordinatorFeature::HTTPCLIENT_ASYNC).c_str(),
                ip.data(), port.data(), e.what());
            return false;
        }
        connectionCompleted_ = false;
        auto& stream = newConnection->GetSocket();
        stream.expires_after(std::chrono::seconds(5)); // 超时5秒
        stream.async_connect(results,
            std::bind(&HttpClientAsync::OnConnection, this, std::placeholders::_1, std::placeholders::_2));
        std::unique_lock<std::mutex> lock(conMutex_);
        conV_.wait(lock, [this] { return connectionCompleted_; });
        if (isException) {
            return false;
        }
    }
    if (!CreateNewConnId(id, newConnection)) {
        return false;
    }
    newConnection->Start(id, ip, port, this);
    return true;
}

void HttpClientAsync::OnConnection(const boost::beast::error_code& ec, const boost::asio::ip::tcp::endpoint&)
{
    std::lock_guard<std::mutex> lock(conMutex_);
    isException = false;
    if (ec) {
        LOG_E("[%s] [HttpClientAsync] Connection accept failed, error is %s.",
            GetErrorCode(ErrorType::UNREACHABLE, CoordinatorFeature::HTTPCLIENT_ASYNC).c_str(), ec.what().c_str());
        isException = true;
    }
    connectionCompleted_ = true;
    conV_.notify_one();
}

class FindIpPort {
public:
    FindIpPort(const std::string& ip, const std::string& port)
    {
        cmpString = ip + ":" + port;
    }
    bool operator ()(const std::map<uint32_t, std::shared_ptr<ClientConnection>>::value_type& pair)
    {
        auto& connection = pair.second;
        if (!connection) {
            LOG_W("[ClientConnectionChecker] Encountered null ClientConnection in map, skipping.");
            return false; // 空连接不匹配任何目标，返回false
        }
        auto& ip = connection->GetIp();
        auto& port = connection->GetPort();
        std::string tmp = ip + ":" + port;
        if (tmp == cmpString) {
            return true;
        }
        return false;
    }

private:
    std::string cmpString;
};

std::vector<uint32_t> HttpClientAsync::FindId(boost::beast::string_view ip, boost::beast::string_view port)
{
    FindIpPort find(ip, port);
    std::vector<uint32_t> idVec;
    std::unique_lock<std::mutex> lock(mtx);
    auto iter = std::find_if(connections_.begin(), connections_.end(), find);
    while (iter != connections_.end()) {
        idVec.emplace_back(iter->first);
        iter++;
        iter = std::find_if(iter, connections_.end(), find);
    }
    return idVec;
}

void HttpClientAsync::Stop()
{
    ioContextPool_.Stop();
    std::unique_lock<std::mutex> lock(mtx);
    connections_.clear();
}

int HttpClientAsync::SetHandler(uint32_t id, const ClientHandler& clientHandler)
{
    std::unique_lock<std::mutex> lock(mtx);
    auto iter = connections_.find(id);
    if (iter != connections_.end()) {
        iter->second->SetHandler(clientHandler);
        return 0;
    }
    return -1;
}

static bool ClientDecrypt(SSLContext &ctx, TlsItems &mTlsConfig, std::pair<char *, int32_t> &mPassword)
{
    if (DecryptPassword(1, mPassword, mTlsConfig)) {
        auto sslCtx = ctx.native_handle();
        SSL_CTX_set_default_passwd_cb_userdata(sslCtx, mPassword.first);
        return true;
    } else {
        LOG_E("[%s] Decrypt password failed.",
            GetErrorCode(ErrorType::CALL_ERROR, CoordinatorFeature::HTTPCLIENT_ASYNC).c_str());
        return false;
    }
}

static int32_t ClientSetCipherSuites(SSLContext &ctx)
{
    int32_t ret = 0;
    ret = SSL_CTX_set_ciphersuites(ctx.native_handle(),
        "TLS_AES_128_GCM_SHA256:"
        "TLS_AES_256_GCM_SHA384:"
        "TLS_CHACHA20_POLY1305_SHA256:"
        "TLS_AES_128_CCM_SHA256");
    return ret;
}

static int32_t UseCert(SSLContext &ctx, const TlsItems &mTlsConfig)
{
    std::string caStr = "";
    uint32_t mode = mTlsConfig.checkFiles ? 0400 : 0777; // 证书的权限要求是0400, 不校验是0777
    if (FileToBuffer(mTlsConfig.caCert, caStr, mode, mTlsConfig.checkFiles) != 0) {
        LOG_E("[%s] Failed to convert CA certificate content to buffer from file %s.",
            GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::HTTPCLIENT_ASYNC).c_str(),
            mTlsConfig.caCert.c_str());
        return -1;
    }
    boost::beast::error_code ec;
    ctx.add_certificate_authority(boost::asio::buffer(caStr.data(), caStr.size()), ec);
    if (ec) {
        LOG_E("[%s] Http client failed to add CA certificate authority from %s, reason is %s.",
            GetErrorCode(ErrorType::INVALID_INPUT, CoordinatorFeature::HTTPCLIENT_ASYNC).c_str(),
            mTlsConfig.caCert.c_str(), ec.message().c_str());
        return -1;
    }
    std::string certPath = mTlsConfig.tlsCert;
    bool isCertExist = false;
    if (!PathCheck(certPath, isCertExist, mode, mTlsConfig.checkFiles)) {
        LOG_E("[%s] Certificate path check failed for: %s.",
            GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::HTTPCLIENT_ASYNC).c_str(), certPath.c_str());
        return -1;
    }
    if (!isCertExist) {
        LOG_E("[%s] Certificate file does not exist at %s.",
            GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::HTTPCLIENT_ASYNC).c_str(), certPath.c_str());
        return -1;
    }
    ctx.use_certificate_chain_file(certPath, ec);
    if (ec) {
        LOG_E("[%s] Failed to load certificate chain from file: %s, reason: %s.",
            GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::HTTPCLIENT_ASYNC).c_str(),
            certPath.c_str(), ec.message().c_str());
        return -1;
    }
    return 0;
}

static int32_t UseKey(SSLContext &ctx, const TlsItems &mTlsConfig)
{
    std::string keyPath = mTlsConfig.tlsKey;
    bool isKeyExist = false;
    boost::beast::error_code ec;
    uint32_t mode = mTlsConfig.checkFiles ? 0400 : 0640; // 密钥的权限要求是0400, 不校验是0640
    if (!PathCheck(keyPath, isKeyExist, mode, mTlsConfig.checkFiles)) {
        LOG_E("[%s] Key file path check failed.",
            GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::HTTPCLIENT_ASYNC).c_str());
        return -1;
    }
    if (!isKeyExist) {
        LOG_E("[%s] Key file is not exist.",
            GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::HTTPCLIENT_ASYNC).c_str());
        return -1;
    }
    ctx.use_private_key_file(keyPath, SSLContext::pem, ec);
    if (ec) {
        LOG_E("[%s] HTTP client failed on use private key file, reason is %s.",
            GetErrorCode(ErrorType::INVALID_INPUT, CoordinatorFeature::HTTPCLIENT_ASYNC).c_str(),
            ec.message().c_str());
        return -1;
    }
    auto sslCtx = ctx.native_handle();
    if (SSL_CTX_check_private_key(sslCtx) == 0) {
        LOG_E("[%s] [HttpClient] Failed to check the private key for the HTTP client.",
            GetErrorCode(ErrorType::INVALID_PARAMETER, CoordinatorFeature::HTTPCLIENT_ASYNC).c_str());
        return -1;
    }
    return 0;
}

int32_t HttpClientAsync::SetTlsCtx(SSLContext &sslCtx, TlsItems &tlsItems, bool mUseKMC)
{
    auto ret = ClientSetCipherSuites(sslCtx);
    if (ret == 0) {
        ERR_print_errors_fp(stderr);
        LOG_E("[%s] Server Failed to set cipher suites to TLS context.",
            GetErrorCode(ErrorType::INVALID_PARAMETER, CoordinatorFeature::HTTPCLIENT_ASYNC).c_str());
        return -1;
    }

    STACK_OF(SSL_CIPHER) *ciphers = SSL_CTX_get_ciphers(sslCtx.native_handle());
    if (!ciphers) {
        perror("SSL_CTX_get_ciphersuites");
        return 1;
    }

    if (UseCert(sslCtx, tlsItems) != 0) {
        return -1;
    }

    std::pair<char *, int32_t> mPassword = {nullptr, 0};
    if (mUseKMC) {
        if (!ClientDecrypt(sslCtx, tlsItems, mPassword)) {
            return -1;
        }
        if (UseKey(sslCtx, tlsItems) != 0) {
            EraseDecryptedData(mPassword);
            SSL_CTX_set_default_passwd_cb_userdata(sslCtx.native_handle(), nullptr);
            return -1;
        }
        EraseDecryptedData(mPassword);
    } else if (UseKey(sslCtx, tlsItems) != 0) {
        SSL_CTX_set_default_passwd_cb_userdata(sslCtx.native_handle(), nullptr);
        return -1;
    }
    SSL_CTX_set_default_passwd_cb_userdata(sslCtx.native_handle(), nullptr);
    boost::beast::error_code ec;
    sslCtx.set_verify_mode(boost::asio::ssl::verify_peer | boost::asio::ssl::verify_fail_if_no_peer_cert, ec);
    if (ec) {
        LOG_E("[%s] HTTP client failed on set verify mode, reason is %s.",
            GetErrorCode(ErrorType::INVALID_PARAMETER, CoordinatorFeature::HTTPCLIENT_ASYNC).c_str(),
            ec.message().c_str());
        return -1;
    }
    verifyItems.checkSubject = false;
    verifyItems.crlFile = tlsItems.tlsCrl;
    verifyItems.checkFiles = tlsItems.checkFiles;
    SSL_CTX_set_cert_verify_callback(sslCtx.native_handle(), VerifyCallback, static_cast<void *>(&verifyItems));
    return 0;
}

size_t HttpClientAsync::GetConnSize()
{
    std::unique_lock<std::mutex> lock(mtx);
    return connections_.size();
}

}