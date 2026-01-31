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
#ifndef JSONFILEMANAGER_H
#define JSONFILEMANAGER_H

#include <fstream>
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class JsonFileManager {
public:
    explicit JsonFileManager(const std::string& filename) : filename_(filename) {}

    // Load the JSON file into the internal object
    bool Load()
    {
        std::ifstream file(filename_);
        if (!file.is_open()) {
            std::cout << "Error opening file: " << filename_ << ", input empty context" << std::endl;
            data_ = json();
            return true;
        }
        try {
            file >> data_;
        } catch (std::exception& e) {
            std::cerr << "Error loading JSON from file: " << e.what() << std::endl;
            return false;
        }
        file.close();
        return true;
    }

    // Save the internal object to the JSON file
    bool Save()
    {
        std::ofstream file(filename_);
        if (!file.is_open()) {
            std::cout << "Error opening file for writing: " << filename_ << std::endl;
            file.close();
            return true;
        }
        try {
            file << std::setw(4) << data_ << std::endl; // 输出至少占用4个字符的宽度
        } catch (std::exception& e) {
            std::cerr << "Error saving JSON to file: " << e.what() << std::endl;
            return false;
        }
        file.close();
        return true;
    }

    // Get a value by key
    template<typename T>
    T Get(const std::string& key) const
    {
        if (!data_.contains(key)) {
            throw std::runtime_error("Key not found in JSON: " + key);
        }
        return data_[key].get<T>();
    }

    // Get a value by key list
    template<typename T>
    T GetList(const std::vector<std::string>& keys) const
    {
        json current = data_;
        for (size_t i = 0; i < keys.size() - 1; ++i) {
            const std::string& key = keys[i];
            if (!current.contains(key)) {
                current[key] = json();
            }
            current = current[key];
        }
        T value = current[keys[keys.size() - 1]];
        return value;
    }

    // Set a value by key
    template<typename T>
    void Set(const std::string& key, const T& value)
    {
        data_[key] = value;
    }
    // Set a value by key list
    template<typename T>
    void SetList(const std::vector<std::string>& keys, const T& value)
    {
        json* current = &data_;
        for (size_t i = 0; i < keys.size() - 1; ++i) {
            const std::string& key = keys[i];
            if (!current->contains(key)) {
                (*current)[key] = json();
            }
            current = &(*current)[key];
        }
        (*current)[keys[keys.size() - 1]] = value;
    }

    // Remove a key from the JSON object
    void Remove(const std::string& key)
    {
        if (data_.contains(key)) {
            data_.erase(key);
        }
    }

    // Add a new key-value pair to the JSON object
    template<typename T>
    void Add(const std::string& key, const T& value)
    {
        if (!data_.contains(key)) {
            data_[key] = value;
        } else {
            throw std::runtime_error("Key already exists in JSON: " + key);
        }
    }

    void Erase(const std::string& key)
    {
        if (!data_.contains(key)) {
            throw std::runtime_error("Key not exists in JSON: " + key);
        } else {
            data_.erase(key);
        }
    }

    bool RemoveKey(json& j, const std::vector<std::string>& keys)
    {
        if (keys.empty()) {
            return false;
        }

        if (keys.size() == 1) {
            // 如果只有一个键，则直接删除
            j.erase(keys[0]);
            return true;
        }

        // 递归查找并删除键
        std::vector<std::string> subKeys(keys.begin() + 1, keys.end());
        if (j.find(keys[0]) != j.end()) {
            if (j[keys[0]].is_object()) {
                // 如果找到的键是对象，则递归调用
                return RemoveKey(j[keys[0]], subKeys);
            } else {
                // 如果找到的键不是对象，则无法继续递归
                std::cerr << "Key '" << keys[0] << "' is not an object." << std::endl;
                return false;
            }
        } else {
            // 键不存在
            std::cerr << "Key '" << keys[0] << "' does not exist." << std::endl;
            return false;
        }
    }

    // 删除 JSON 结构中的指定 key
    void EraseList(const std::vector<std::string>& keys)
    {
        RemoveKey(data_, keys);
    }

    // Check if a key exists in the JSON object
    bool Contains(const std::string& key) const
    {
        return data_.contains(key);
    }

    std::string Dump()
    {
        return data_.dump();
    }

private:
    std::string filename_; // The file name of the JSON file
    json data_;            // The JSON object loaded from the file
};

#endif