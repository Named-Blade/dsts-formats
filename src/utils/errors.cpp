#pragma once

#include <vector>
#include <string>
#include <stdexcept>
#include <cassert>

bool throwErrors = true;
std::vector<std::string> errorList{};

#ifndef NDEBUG
    #define ASSERT_OR_THROW(cond, msg) \
        do { \
            if (!(cond)) { \
                if (throwErrors) { \
                    assert(cond); \
                } else { \
                    errorList.push_back(msg); \
                } \
            } \
        } while(0)
#else
    #define ASSERT_OR_THROW(cond, msg) \
        do { \
            if (!(cond)) { \
                if (throwErrors) { \
                    throw std::runtime_error(msg); \
                } else { \
                    errorList.push_back(msg); \
                } \
            } \
        } while(0)
#endif