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
#ifndef MINDIS_MS_GROUPGENERATORFACTORY_H
#define MINDIS_MS_GROUPGENERATORFACTORY_H

#include <functional>
#include <unordered_map>
#include <memory>
#include "GroupGenerator.h"
namespace MINDIE::MS {
using GroupGeneratorCreator = std::function<std::unique_ptr<GroupGenerator>()>;
class GroupGeneratorFactory {
public:
    GroupGeneratorFactory() = default;
    ~GroupGeneratorFactory() = default;
    static GroupGeneratorFactory &GetInstance();
    bool Register(const std::string &type, GroupGeneratorCreator func);
    std::unique_ptr<GroupGenerator> CreateGroupGenerator(const std::string &type);
private:
    std::unordered_map<std::string, GroupGeneratorCreator> mMap {};
};

template <class T>
class GroupGeneratorRegister {
public:
    explicit GroupGeneratorRegister(const std::string &type) noexcept
    {
        auto creator = []() -> std::unique_ptr<GroupGenerator> {
            auto groupGenerator = std::make_unique<T>();
            return groupGenerator;
        };
        GroupGeneratorFactory::GetInstance().Register(type, creator);
    }
    ~GroupGeneratorRegister() = default;
};

#define REGISTER_GROUP_GENERATOR_CREATOR(className, groupGeneratorType) \
    static GroupGeneratorRegister<className> groupGeneratorType##Register(#groupGeneratorType)
}
#endif // MINDIS_MS_GROUPGENERATORFACTORY_H