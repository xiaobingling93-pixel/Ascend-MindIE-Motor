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
#ifndef MIES_GRPC_TLS_FUNCTION_EXPANSION_H
#define MIES_GRPC_TLS_FUNCTION_EXPANSION_H

#include <vector>
#include <memory>
#include <cstdio>
#include <string>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

namespace MINDIE::MS {
// Custom deleters for releasing X509 objects
struct FileDeleter {
    void operator()(FILE* ptr) const
    {
        if (ptr != nullptr) {
            fclose(ptr);
            ptr = nullptr;
        }
    }
};

struct X509CertDeleter {
    void operator()(X509* ptr) const
    {
        if (ptr != nullptr) {
            X509_free(ptr);
            ptr = nullptr;
        }
    }
};

struct X509CrlDeleter {
    void operator()(X509_CRL* ptr) const
    {
        if (ptr != nullptr) {
            X509_CRL_free(ptr);
            ptr = nullptr;
        }
    }
};

class GrpcTlsFunctionExpansion {
public:
    static bool CheckTlsOption(const std::vector<std::string>& caPath, const std::string& cert,
        const std::vector<std::string>& crlPath, bool checkFiles = true);
private:
    static bool CheckCert(const std::string& realCertPath);
    static bool CheckCrl(const std::string& realCrlPath);
};
}

#endif // MIES_GRPC_TLS_FUNCTION_EXPANSION_H