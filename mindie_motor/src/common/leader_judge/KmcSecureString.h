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
#ifndef KMSECURESTRING_H
#define KMSECURESTRING_H

#include <cstdint>             // 需要 int32_t 类型定义
#include <string>
#include "Logger.h"

namespace MINDIE::MS {

class KmcSecureString {
public:
    /**
     * @brief 构造函数 - 从原始数据创建安全字符串
     * @param data 原始数据指针 (可能为 nullptr)
     * @param size 数据长度 (必须 >= 0)
     */
    explicit KmcSecureString(const char* data = nullptr, int32_t size = 0);

    /**
     * @brief 移动构造函数
     * @param other 要移动的源对象
     */
    KmcSecureString(KmcSecureString&& other) noexcept;

    /**
     * @brief 移动赋值操作符
     * @param other 要移动的源对象
     */
    KmcSecureString& operator=(KmcSecureString&&) = delete;
    /**
     * @brief 析构函数 - 自动清理安全内存
     */
    ~KmcSecureString();

    // 禁用拷贝操作
    KmcSecureString(const KmcSecureString&) = delete;
    KmcSecureString& operator=(const KmcSecureString&) = delete;

    /**
     * @brief 安全擦除内存并重置状态
     */
    void Clear();

    /**
     * @brief 检查对象有效性
     * @return 是否持有有效数据
     */
    bool IsValid() const;

    /**
     * @brief 获取敏感数据指针
     * @return 数据指针 (可能为 nullptr)
     */
    const char* GetSensitiveInfoContent() const;

    /**
     * @brief 获取敏感数据长度
     * @return 数据长度 (可能为 0)
     */
    int32_t GetSensitiveInfoSize() const;

private:
    char* data_ = nullptr;    // 安全内存指针
    int32_t size_ = 0;         // 数据长度
};

} // namespace MINDIE::MS

#endif // KMSECURESTRING_H