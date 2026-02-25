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
#ifndef MINDIE_MS_COORDINATOR_SSL_SERVER_CONNECTION_H
#define MINDIE_MS_COORDINATOR_SSL_SERVER_CONNECTION_H

#include "ServerConnection.h"

namespace MINDIE::MS {

/// TLS connection of http server.
///
/// TLS connection of http server.
class SSLServerConnection : public ServerConnection {
public:
    explicit SSLServerConnection(boost::asio::io_context& ioContext, ServerHandler& serverHandler, HttpServer &server,
        SSLContext &sslContext);

    SSLStream* GetSSLSocket() override;
    void DoClose(boost::beast::error_code ec) override;

private:
    void DoRead() override;
    /// Read https request handler.
    ///
    /// When a read event is triggered, this function will be called.
    ///
    /// \param ec error code.
    /// \param bytes read bytes.
    void OnSSLRead(boost::beast::error_code ec, std::size_t bytes);
    void DoWriteFinishRes(const ServerRes &res) override;
    void OnSSLWriteFinishRes(bool keepAlive, boost::beast::error_code ec, size_t bytes);
    void WriteChunkHeader(const ServerRes &res) override;
    void WriteChunk(const ServerRes &res) override;
    void WriteChunkTailer() override;
    void OnSSLWriteChunk(bool isLastChunk, boost::beast::error_code ec, size_t bytes);
    void OnSSLWriteChunkTailer(bool keepAlive, boost::beast::error_code ec, size_t bytes);

    SSLStream sslStream_;
};
}
#endif