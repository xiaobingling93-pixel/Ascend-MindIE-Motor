/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MindIE is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef MINDIE_MS_GRPCCLUSTERCLIENT_H
#define MINDIE_MS_GRPCCLUSTERCLIENT_H
#include <string>
#include <grpcpp/security/tls_credentials_options.h>
#include <src/core/lib/security/credentials/tls/grpc_tls_credentials_options.h>
#include "grpcpp/grpcpp.h"
#include "grpc_proto/recover_mindie.pb.h"
#include "grpc_proto/recover_mindie.grpc.pb.h"
#include "grpc_proto/cluster_fault.pb.h"
#include "grpc_proto/cluster_fault.grpc.pb.h"
#include "ConfigParams.h"

namespace MINDIE::MS {
constexpr uint32_t MAX_KEEPALIVE_TIME_MS = 5000;
constexpr uint32_t MAX_KEEPALIVE_TIMEOUT_MS = 60000;

std::shared_ptr<grpc::Channel> CreateGrpcChannel(const std::string serverAddr, const TlsItems& mTlsConfig);
class GrpcClusterClient {
};
}
#endif // MINDIE_MS_GRPCCLUSTERCLIENT_H
