#pragma once

#include <iostream>
#include <vector>
#include <memory>
#include <algorithm> // for std::find

template <typename T>
int getIndex(const std::vector<std::shared_ptr<T>>& vec, const std::shared_ptr<T>& ptr) {
    auto it = std::find(vec.begin(), vec.end(), ptr);
    if (it != vec.end()) {
        return std::distance(vec.begin(), it);
    }
    return -1; // not found
}

template <typename T>
std::vector<T> flatten(const std::vector<std::vector<T>>& nested)
{
    std::vector<T> result;

    // Pre-compute total size for one allocation
    size_t total_size = 0;
    for (const auto& v : nested) {
        total_size += v.size();
    }
    result.reserve(total_size);

    // Append all inner vectors
    for (const auto& v : nested) {
        result.insert(result.end(), v.begin(), v.end());
    }

    return result;
}