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
#ifndef MINDIE_MS_COORDINATOR_WRITE_DEQUE_H
#define MINDIE_MS_COORDINATOR_WRITE_DEQUE_H

#include <mutex>
#include <deque>
#include <map>
#include <shared_mutex>
#include <stdexcept>

namespace MINDIE::MS {

template <typename T> struct WriteDeque {
public:
    void PushBack(const T &element)
    {
        std::unique_lock<std::mutex> lock(mtx);
        mQueue.push_back(element);
    }
    bool Empty()
    {
        std::unique_lock<std::mutex> lock(mtx);
        return mQueue.empty();
    }
    T &Front()
    {
        std::unique_lock<std::mutex> lock(mtx);
        if (mQueue.empty()) {
            throw std::out_of_range("[WriteDeque] front() called on empty queue.");
        }
        return mQueue.front();
    }
    void PopFront()
    {
        std::unique_lock<std::mutex> lock(mtx);
        mQueue.pop_front();
    }
    void Clear()
    {
        std::unique_lock<std::mutex> lock(mtx);
        mQueue.clear();
    }
    size_t Size()
    {
        std::unique_lock<std::mutex> lock(mtx);
        return mQueue.size();
    }

private:
    std::deque<T> mQueue;
    std::mutex mtx;
};

template <typename K, typename V> class ThreadSafeMap {
public:
    void Emplace(const K &key, const V &value)
    {
        std::unique_lock<std::shared_mutex> lock(mtx);
        mapInner.emplace(std::make_pair(key, value));
    }
    V Get(const K &key)
    {
        std::shared_lock<std::shared_mutex> lock(mtx);
        return mapInner[key];
    }

private:
    std::shared_mutex mtx;
    std::map<K, V> mapInner;
};

}
#endif