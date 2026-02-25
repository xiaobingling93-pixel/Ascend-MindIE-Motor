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
#ifndef MINDIE_DIGS_RESOURCE_LOAD_H
#define MINDIE_DIGS_RESOURCE_LOAD_H

#include "common.h"

namespace MINDIE::MS {

class ResourceLoad {
public:
    explicit ResourceLoad(DIGSInstanceDynamicInfo& insDynaInfo);

    ResourceLoad();

    ~ResourceLoad() = default;

    const DIGSInstanceDynamicInfo& DynamicInfo() { return instanceDynamicInfo_; }

    bool IsResAvailable() const;

private:
    DIGSInstanceDynamicInfo instanceDynamicInfo_;
};

}
#endif