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
#include "resource_load.h"

namespace MINDIE::MS {

ResourceLoad::ResourceLoad(MINDIE::MS::DIGSInstanceDynamicInfo& insDynaInfo)
    : instanceDynamicInfo_(insDynaInfo)
{
}

ResourceLoad::ResourceLoad()
{
    instanceDynamicInfo_ = { .id = common::ILLEGAL_INSTANCE_ID, };
}

bool ResourceLoad::IsResAvailable() const
{
    // 当前仅考虑block和slots资源，多机场景下需考虑单个实例最大可用block资源
    return instanceDynamicInfo_.availSlotsNum > 0 &&
           instanceDynamicInfo_.availBlockNum > 0 &&
           instanceDynamicInfo_.maxAvailBlockNum > 0;
}

}