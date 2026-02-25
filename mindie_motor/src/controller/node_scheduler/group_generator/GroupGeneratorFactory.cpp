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
#include "GroupGeneratorFactory.h"
#include "Logger.h"
namespace MINDIE::MS {
GroupGeneratorFactory &GroupGeneratorFactory::GetInstance()
{
    static GroupGeneratorFactory instance;
    return instance;
}

bool GroupGeneratorFactory::Register(const std::string &type, GroupGeneratorCreator func)
{
    LOG_I("[GroupGeneratorFactory] Adding creator, add type %s.", type.c_str());
    if (func == nullptr) {
        LOG_E("[%s] [GroupGeneratorFactory] Adding creator, func is invalid.",
            GetErrorCode(ErrorType::INVALID_PARAMETER, ControllerFeature::GROUP_GENERATOR).c_str());
        return false;
    }
    if (mMap.find(type) != mMap.end()) {
        LOG_E("[%s] [GroupGeneratorFactory] Adding creator, failed to add existed type %s.",
            GetErrorCode(ErrorType::OPERATION_REPEAT, ControllerFeature::GROUP_GENERATOR).c_str(),
            type.c_str());
        return false;
    }
    mMap[type] = func;
    LOG_I("[GroupGeneratorFactory] Adding creator, add type %s success.", type.c_str());
    return true;
}

std::unique_ptr<GroupGenerator> GroupGeneratorFactory::CreateGroupGenerator(const std::string &type)
{
    auto iter = mMap.find(type);
    if (iter == mMap.end()) {
        LOG_E("[%s] [GroupGeneratorFactory] Creating group generator, type %s is not found.",
            GetErrorCode(ErrorType::NOT_FOUND, ControllerFeature::GROUP_GENERATOR).c_str(),
            type.c_str());
        return nullptr;
    }
    std::unique_ptr<GroupGenerator> obj;
    try {
        obj = iter->second();
        return obj;
    } catch (const std::exception& e) {
        LOG_E("[%s] [GroupGeneratorFactory] Creating group generator pointer failed.",
            GetErrorCode(ErrorType::RESOURCE_EXHAUSTED, ControllerFeature::GROUP_GENERATOR).c_str());
        return nullptr;
    }
}
}