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
#include <vector>
#include "Logger.h"
#include "LRUCache.h"
namespace MINDIE::MS {
bool LRUCache::Get(size_t key, uint64_t &value)
{
    std::shared_lock<std::shared_mutex> lock(cacheMutex);
    if (sessionNodeMap.find(key) != sessionNodeMap.end()) {
        value = sessionNodeMap[key];
        return true;
    }
    value = 0;
    return false;
}

void LRUCache::Put(size_t key, uint64_t value)
{
    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    if (sessionNodeMap.find(key) != sessionNodeMap.end()) {
        sessionNodeMap[key] = value;
    } else {
        if (cache.size() == capacity) {
            size_t removeKey = cache.front();
            cache.pop_front();
            cacheMap.erase(removeKey);
            sessionNodeMap.erase(removeKey);
        }
        cache.push_back(key);
        std::list<size_t>::const_iterator backIter = std::prev(cache.end());
        cacheMap.insert(std::make_pair(key, backIter));
    
        sessionNodeMap.insert(std::make_pair(key, value));
    }
}

void LRUCache::Erase(size_t key)
{
    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    if (cacheMap.find(key) != cacheMap.end()) {
        std::list<size_t>::const_iterator iter = cacheMap[key];
        cache.erase(iter);
        cacheMap.erase(key);
        sessionNodeMap.erase(key);
    }
}

void LRUCache::UpdateKey(size_t oldKey, size_t newKey)
{
    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    if (cacheMap.find(oldKey) != cacheMap.end()) {
        // update cache.
        std::list<size_t>::const_iterator oldIter = cacheMap[oldKey];
        cache.erase(oldIter);
        cache.push_back(newKey);
 
        // update cacheMap
        std::list<size_t>::const_iterator backIter = std::prev(cache.end());
        cacheMap.insert(std::make_pair(newKey, backIter));
        cacheMap.erase(oldKey);

        // update sessionNodeMap
        sessionNodeMap.insert(std::make_pair(newKey, sessionNodeMap[oldKey]));
        sessionNodeMap.erase(oldKey);
    }
}
}