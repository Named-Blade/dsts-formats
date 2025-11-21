#pragma once

size_t align(size_t val, size_t alignment) {
    size_t offset = val % alignment;
    size_t skip = alignment - offset;
    return val + skip;
}