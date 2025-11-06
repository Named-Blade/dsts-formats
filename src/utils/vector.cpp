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