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
#include <securec.h>
#include "KmcSecureString.h"

namespace MINDIE::MS {

KmcSecureString::KmcSecureString(const char* data, int32_t size) : size_(size)
{
    if (!data || size < 0) {
        LOG_E("[KmcSecureString] input data or size is invalid");
    }
    data_ = static_cast<char*>(OPENSSL_secure_malloc(size));
    if (data_) {
        auto cpyret = memcpy_s(data_, size_, data, size);
        if (cpyret != 0) {
            LOG_E("[KmcSecureString] data memcpy_s failed");
        }
    } else {
        LOG_E("[KmcSecureString] OPENSSL_secure_malloc failed");
    }
}

KmcSecureString::~KmcSecureString()
{
    Clear();
}

void KmcSecureString::Clear()
{
    if (data_ != nullptr) {
        OPENSSL_secure_clear_free(data_, size_);
        data_ = nullptr;
    }
    size_ = 0;
}

KmcSecureString::KmcSecureString(KmcSecureString&& other) noexcept
    : data_(other.data_), size_(other.size_)
{
    other.data_ = nullptr;
    other.size_ = 0;
}

bool KmcSecureString::IsValid() const
{
    return data_ != nullptr && size_ > 0;
}

const char* KmcSecureString::GetSensitiveInfoContent() const
{
    if (IsValid()) {
        return data_;
    } else {
        LOG_E("[KmcSecureString] Sensitive data info is invalid");
        return nullptr;
    }
}

int32_t KmcSecureString::GetSensitiveInfoSize() const
{
    if (IsValid()) {
        return size_;
    } else {
        LOG_E("[KmcSecureString] Sensitive size info is invalid");
        return 0;
    }
}

} // namespace MINDIE::MS