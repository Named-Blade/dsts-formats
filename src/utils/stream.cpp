#pragma once

#include <iostream>
#include <fstream>

// Aligns the stream position to the next multiple of 'alignment' bytes
void alignStream(std::ostream& stream, std::streamoff alignment) {
    if (alignment <= 0) return;  // invalid alignment

    std::streampos currentPos = stream.tellp();
    if (currentPos == -1) {
        std::cerr << "Failed to get current position\n";
        return;
    }

    std::streamoff offset = currentPos % alignment;
    if (offset != 0) {
        std::streamoff skip = alignment - offset;
        stream.seekp(skip, std::ios::cur);
        if (!stream) {
            std::cerr << "Failed to seek to aligned position\n";
        }
    }
}

void alignStream(std::istream& stream, std::streamoff alignment) {
    if (alignment <= 0) return;  // invalid alignment

    std::streampos currentPos = stream.tellg();
    if (currentPos == -1) {
        std::cerr << "Failed to get current position\n";
        return;
    }

    std::streamoff offset = currentPos % alignment;
    if (offset != 0) {
        std::streamoff skip = alignment - offset;
        stream.seekg(skip, std::ios::cur);
        if (!stream) {
            std::cerr << "Failed to seek to aligned position\n";
        }
    }
}