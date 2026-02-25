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
#include <memory>
#include <cstdio>
#include <string>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include "Logger.h"
#include "Util.h"
#include "GrpcTlsFunctionExpansion.h"

namespace MINDIE::MS {
bool GrpcTlsFunctionExpansion::CheckTlsOption(const std::vector<std::string>& caPath, const std::string& cert,
    const std::vector<std::string>& crlPath, bool checkFiles)
{
    uint32_t mode = checkFiles ? 0400 : 0777; // Ca cert crl file permission 400
    bool isFilesExist = false;
    std::string realCertPath = cert;
    for (const auto& ca : caPath) {
        std::string realCaPath = ca;
        if (!PathCheck(realCaPath, isFilesExist, mode, checkFiles) || !CheckCert(realCaPath)) {
            return false;
        }
    }
    if (!PathCheck(realCertPath, isFilesExist, mode, checkFiles) || !CheckCert(realCertPath)) {
        return false;
    }
    for (const auto& crl : crlPath) {
        std::string realCrlPath = crl;
        if (!PathCheck(realCrlPath, isFilesExist, mode, checkFiles) || !CheckCrl(realCrlPath)) {
            return false;
        }
    }
    return true;
}

bool GrpcTlsFunctionExpansion::CheckCert(const std::string& realCertPath)
{
    // Read cert file
    std::unique_ptr<FILE, FileDeleter> certFile(fopen(realCertPath.data(), "r"));
    if (certFile == nullptr) {
        LOG_E("[%s] Failed to open cert file.",
            GetErrorCode(ErrorType::NOT_FOUND, ControllerFeature::CONTROLLER).c_str());
        return false;
    }

    std::unique_ptr<X509, X509CertDeleter> cert(PEM_read_X509(certFile.get(), nullptr, nullptr, nullptr));
    if (cert == nullptr) {
        LOG_E("[%s] Failed to read cert file.",
            GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::CONTROLLER).c_str());
        return false;
    }

    // Check x509 version
    if (X509_get_version(cert.get()) != X509_VERSION_3) {
        LOG_E("[%s] Cert version is invalid.",
            GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::CONTROLLER).c_str());
        return false;
    }

    // Validity period of the proofreading certificate
    if (X509_cmp_current_time(X509_getm_notAfter(cert.get())) < 0) {
        LOG_E("[%s] Cert has expired! current time after cert time.",
            GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::CONTROLLER).c_str());
        return false;
    }

    if (X509_cmp_current_time(X509_getm_notBefore(cert.get())) > 0) {
        LOG_E("[%s] Cert has expired! current time before cert time.",
            GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::CONTROLLER).c_str());
        return false;
    }

    return true;
}

bool GrpcTlsFunctionExpansion::CheckCrl(const std::string& realCrlPath)
{
    // Read crl file
    std::unique_ptr<FILE, FileDeleter> crlFile(fopen(realCrlPath.data(), "r"));
    if (crlFile == nullptr) {
        LOG_E("[%s] Failed to open crl file.",
            GetErrorCode(ErrorType::NOT_FOUND, ControllerFeature::CONTROLLER).c_str());
        return false;
    }
    std::unique_ptr<X509_CRL, X509CrlDeleter> crl(PEM_read_X509_CRL(crlFile.get(), nullptr, nullptr, nullptr));
    if (crl == nullptr) {
        LOG_E("[%s] Failed to read crl file.",
            GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::CONTROLLER).c_str());
        return false;
    }

    // Check crl time
    if (X509_cmp_current_time(X509_CRL_get0_nextUpdate(crl.get())) < 0) {
        LOG_E("[%s] Crl has expired! current time after next update time.",
            GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::CONTROLLER).c_str());
        return false;
    }
    return true;
}

}