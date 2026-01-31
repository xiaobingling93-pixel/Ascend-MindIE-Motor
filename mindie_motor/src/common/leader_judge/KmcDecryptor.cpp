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

#include "KmcDecryptor.h"
#include <fstream>
#include <sstream>
#include <atomic>
#include <securec.h>
#include "Logger.h"

namespace MINDIE {
namespace MS {

constexpr int MAX_TOKEN_LEN = 128; // 私钥口令上限是128字符
constexpr int MIN_TOKEN_LEN = 8; // 私钥口令下限是8字符

static int PasswordCallback(char *buf, int size, int rwflag, void *userdata)
{
    (void)rwflag;

    if (userdata == nullptr || buf == nullptr || size <= 0) {
        if (buf != nullptr && size > 0) {
            buf[0] = '\0';
        }
        return 0;
    }

    KmcSecureString* password = static_cast<KmcSecureString*>(userdata);
    const char* content = password->GetSensitiveInfoContent();
    int contentLen = password->GetSensitiveInfoSize();

    int len = (contentLen < size) ? contentLen : size - 1;

    if (len > 0) {
        // 检查 memcpy_s 返回值
        errno_t result = memcpy_s(buf, size, content, len);
        if (result != 0) {
            // 复制失败，清理缓冲区
            errno_t ret = memset_s(buf, size, 0, size);
            if (ret != 0) {
                LOG_E("[KmcDecryptor] buffer erase failed, Len=%d, ErrCode=%d",
                    size, ret);
            }
            buf[0] = '\0';
            len = 0;
        } else {
            buf[len] = '\0';
        }
    } else {
        buf[0] = '\0';
        len = 0;
    }

    // 清理局部敏感变量指针
    const int pointerSize = sizeof(KmcSecureString*);
    errno_t ret = memset_s(&password, pointerSize, 0, pointerSize);
    if (ret != 0) {
        LOG_E("[KmcDecryptor] Fail to erase password pointer, ErrCode=%d", ret);
    }

    return len;
}

KmcDecryptor::KmcDecryptor(const TlsItems& config) : tlsCfg(config) {}

KmcDecryptor::~KmcDecryptor() {}

// 解析秘钥主接口
KmcSecureString KmcDecryptor::DecryptPrivateKey()
{
    try {
        std::unique_ptr<KmcSecureString> passwordData;
        std::unique_ptr<KmcSecureString> keyData;
        if (GetDecryptionPassword(passwordData) && LoadEncryptedKey(keyData)) {
            return ParsePemPrivateKey(keyData, passwordData);
        }
    } catch (const std::exception& e) {
        LOG_E("[%s] [NodeScheduler] Initialize failed because create pointer failed.",
            GetErrorCode(ErrorType::RESOURCE_EXHAUSTED, ControllerFeature::NODE_SCHEDULER).c_str());
    }
    return KmcSecureString(nullptr, 0);
}

bool KmcDecryptor::GetDecryptionPassword(std::unique_ptr<KmcSecureString>& passwordData)
{
    std::pair<char *, int32_t> mPassword = {nullptr, 0};
    if (!DecryptPassword(1, mPassword, tlsCfg)) {
        LOG_E("[KmcDecryptor] Filed to get password.");
        return false;
    }
    // 对密码长度进行校验
    if (mPassword.second < MIN_TOKEN_LEN || mPassword.second > MAX_TOKEN_LEN) {
        LOG_E("[KmcDecryptor] Password is Invalid.");
        errno_t ret = memset_s(mPassword.first, mPassword.second, 0, mPassword.second);
        if (ret != 0) {
            LOG_E("[KmcDecryptor] Fail to erase password buffer, Len=%d, ErrCode=%d",
                mPassword.second, ret);
        }
        delete [] mPassword.first;
        mPassword.first = nullptr;
        return false;
    }
    passwordData = std::make_unique<KmcSecureString>(mPassword.first, mPassword.second);
    errno_t ret = memset_s(mPassword.first, mPassword.second, 0, mPassword.second);
    if (ret != 0) {
        LOG_E("[KmcDecryptor] Fail to erase password buffer, Len=%d, ErrCode=%d",
            mPassword.second, ret);
        delete [] mPassword.first;
        mPassword.first = nullptr;
        return false;
    }
    delete [] mPassword.first;
    mPassword.first = nullptr;
    return true;
}

bool KmcDecryptor::LoadEncryptedKey(std::unique_ptr<KmcSecureString>& keyData)
{
    std::string keyPath = tlsCfg.tlsKey;
    bool isFileExist = false;
    uint32_t mode = tlsCfg.checkFiles ? 0400 : 0750;
    if (!PathCheck(keyPath, isFileExist, mode, tlsCfg.checkFiles)) {
        LOG_E("[KmcDecryptor] Key file path check failed.");
        return false;
    }
    if (!isFileExist) {
        LOG_E("[KmcDecryptor] Key file is not exist.");
        return false;
    }

    std::ifstream in(keyPath);
    if (!in.is_open()) {
        LOG_E("[KmcDecryptor] Failed to open the Key file.");
        return false;
    }

    std::ostringstream fileContent;
    fileContent << in.rdbuf();
    std::string encryptedText = fileContent.str();
    int dataLength = static_cast<int>(encryptedText.length());
    keyData = std::make_unique<KmcSecureString>(encryptedText.data(), dataLength);
    return true;
}

KmcSecureString KmcDecryptor::ParsePemPrivateKey(
    const std::unique_ptr<KmcSecureString>& keyData,
    const std::unique_ptr<KmcSecureString>& passwordData)
{
    if (!keyData || !passwordData || !keyData->IsValid() || !passwordData->IsValid()) {
        return KmcSecureString(nullptr, 0);
    }

    std::unique_ptr<BIO, decltype(&BIO_free)> bio(
        BIO_new_mem_buf(keyData->GetSensitiveInfoContent(), keyData->GetSensitiveInfoSize()),
        BIO_free
    );

    if (!bio) {
        LOG_E("[KmcDecryptor]Failed to create BIO for encrypted key");
        return KmcSecureString(nullptr, 0);
    }

    EVP_PKEY* pkey = PEM_read_bio_PrivateKey(
        bio.get(), nullptr,
        PasswordCallback, passwordData.get()
    );

    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey_guard(pkey, EVP_PKEY_free);

    if (!pkey) {
        char buf[256];
        ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
        LOG_E("[KmcDecryptor]Private key decryption failed: %s", std::string(buf));
        return KmcSecureString(nullptr, 0);
    }

    return SerializeDecryptedKey(pkey);
}

KmcSecureString KmcDecryptor::SerializeDecryptedKey(EVP_PKEY* pkey)
{
    std::unique_ptr<BIO, decltype(&BIO_free)> bio(BIO_new(BIO_s_mem()), BIO_free);
    if (!bio) {
        LOG_E("[KmcDecryptor]Failed to create output BIO");
        return KmcSecureString(nullptr, 0);
    }

    if (!PEM_write_bio_PrivateKey(bio.get(), pkey, nullptr, nullptr, 0, nullptr, nullptr)) {
        LOG_E("[KmcDecryptor]Failed to serialize decrypted key");
        return KmcSecureString(nullptr, 0);
    }

    BUF_MEM* bufMem;
    BIO_get_mem_ptr(bio.get(), &bufMem);
    KmcSecureString decryptedKey(bufMem->data, bufMem->length);
    errno_t ret = memset_s(bufMem->data, bufMem->length, 0, bufMem->length); // 擦除内存中缓存的解密密钥
    if (ret != 0) {
        LOG_E("[KmcDecryptor] Fail to erase buffer of decrypted key, ErrCode=%d", ret);
    }
    return decryptedKey;
}

} // namespace MS
} // namespace MINDIE
