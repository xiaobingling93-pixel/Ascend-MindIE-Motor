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
#ifndef MINDIE_DYNAMIC_CONFIG_HANDLER_H
#define MINDIE_DYNAMIC_CONFIG_HANDLER_H

#include <functional>
#include <vector>
#include <mutex>
#include <atomic>
#include <nlohmann/json.hpp>

namespace MINDIE {
namespace MS {
class DynamicConfigHandler {
public:
    static DynamicConfigHandler& GetInstance();
    static void Start(std::string inputComponent);
    static void Stop();

    template<typename T>
    static void RegisterCallBackFunction(const std::string& pathExpression, T* obj, void(T::*method)(size_t),
                                         size_t value)
    {
        std::lock_guard<std::mutex> locker(GetInstance().vectorMutex);
        GetInstance().callBackFunctions.push_back(std::make_pair(pathExpression, [obj, method, value] {
            (obj->*method)(value);
        }));
    }

private:
    DynamicConfigHandler() {}
    ~DynamicConfigHandler();
    static std::vector<std::string> SplitString(const std::string& s, const char delimiter = '.');
    std::string GetConfigPath() const;
    bool IsTriggered(const std::string pathExpression) const;

    std::vector<std::pair<std::string, std::function<void()>>> callBackFunctions;
    std::atomic<bool> isRunning{true};
    std::mutex vectorMutex;
    std::string component;
};

}
}

#endif // MINDIE_DYNAMIC_CONFIG_HANDLER_H