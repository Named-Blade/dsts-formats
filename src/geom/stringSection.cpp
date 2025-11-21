#pragma once

#include <string>
#include <unordered_map>

class StringSection {
public:
    StringSection() {
        section.push_back('\0');
        offsetMap[""] = 0;
    }

    size_t add(const std::string& s) {
        auto it = offsetMap.find(s);
        if (it != offsetMap.end()) {
            return it->second;
        }

        size_t offset = section.size();
        section += s;
        section.push_back('\0');
        offsetMap[s] = offset;

        return offset;
    }

    const std::string& data() const { return section; }

private:
    std::string section;
    std::unordered_map<std::string, size_t> offsetMap;
};