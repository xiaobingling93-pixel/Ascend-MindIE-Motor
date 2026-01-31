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
#ifndef KMCDECRYPTOR_H
#define KMCDECRYPTOR_H

#include <memory>            // 智能指针
#include <openssl/bio.h>     // OpenSSL BIO接口
#include <openssl/evp.h>     // OpenSSL EVP接口
#include "KmcSecureString.h"  // 自定义安全字符串封装类
#include "ConfigParams.h"

namespace MINDIE::MS {

class KmcDecryptor {
public:
    /**
     * @brief 构造函数，初始化TLS配置项
     * @param config TLS配置项（如私钥路径、文件检查标志等）
     */
    explicit KmcDecryptor(const TlsItems& config);

    /**
     * @brief 析构函数（RAII清理资源）
     */
    ~KmcDecryptor();

    /**
     * @brief 解密私钥主入口
     * @return 解密后的私钥内容（封装在安全字符串中）
     */
    KmcSecureString DecryptPrivateKey();

private:
    /**
     * @brief 获取解密密码（从外部安全存储）
     * @param passwordData 输出参数，接收密码的安全字符串指针
     * @return 是否成功获取密码
     */
    bool GetDecryptionPassword(std::unique_ptr<KmcSecureString>& passwordData);

    /**
     * @brief 加载加密的私钥文件内容
     * @param keyData 输出参数，接收私钥文件内容的安全字符串指针
     * @return 是否成功加载文件
     */
    bool LoadEncryptedKey(std::unique_ptr<KmcSecureString>& keyData);

    /**
     * @brief 解析并解密PEM格式的私钥
     * @param keyData 加密的私钥内容
     * @param passwordData 解密密码
     * @return 解密后的私钥内容
     */
    KmcSecureString ParsePemPrivateKey(
        const std::unique_ptr<KmcSecureString>& keyData,
        const std::unique_ptr<KmcSecureString>& passwordData);

    /**
     * @brief 序列化解密后的私钥为PEM格式
     * @param pkey 已解密的EVP_PKEY对象
     * @return 序列化后的私钥内容
     */
    KmcSecureString SerializeDecryptedKey(EVP_PKEY* pkey);

    TlsItems tlsCfg;  // TLS配置项（如私钥路径、文件检查标志等）
};

} // namespace MINDIE::MS

#endif // KMCDECRYPTOR_H
