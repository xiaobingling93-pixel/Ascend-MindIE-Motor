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
#ifndef MINDIE_LRUCACHE__H
#define MINDIE_LRUCACHE__H

#include <iostream>
#include <list>
#include <unordered_map>
#include <string>
#include <mutex>
#include <shared_mutex>

namespace MINDIE::MS {

class LRUCache {
public:
    explicit LRUCache(uint32_t capacity) : capacity(capacity) {};

    bool Get(size_t key, uint64_t &value);

    void Put(size_t key, uint64_t value);

    void Erase(size_t key);

    void UpdateKey(size_t oldKey, size_t newKey);

private:
    uint32_t capacity;
    std::list<size_t> cache;
    std::unordered_map<size_t, std::list<size_t>::const_iterator> cacheMap;
    std::unordered_map<size_t, uint64_t> sessionNodeMap;
    std::shared_mutex cacheMutex;
};
}

#endif