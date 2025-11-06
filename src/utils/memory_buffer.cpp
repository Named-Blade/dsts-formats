#pragma once

#include <streambuf>
#include <vector>
#include <ostream>
#include <algorithm>
#include <cstring>

class MemoryBuffer : public std::streambuf {
public:
    MemoryBuffer() {
        // No preallocation, starts empty
        setp(nullptr, nullptr);
    }

    std::vector<char> &data() { return buffer_; }
    const std::vector<char> &data() const { return buffer_; }

protected:
    // Write one character
    int_type overflow(int_type ch) override {
        if (ch != traits_type::eof()) {
            auto pos = pptr() - pbase();
            if (pos >= (std::ptrdiff_t)buffer_.size())
                buffer_.resize(pos + 1);
            buffer_[pos] = static_cast<char>(ch);
            setp(buffer_.data(), buffer_.data() + buffer_.size());
            pbump(static_cast<int>(pos + 1));
            return ch;
        }
        return traits_type::eof();
    }

    // Write multiple characters
    std::streamsize xsputn(const char* s, std::streamsize n) override {
        auto pos = pptr() - pbase();
        if (pos + n > (std::streamsize)buffer_.size())
            buffer_.resize(pos + n);
        std::memcpy(buffer_.data() + pos, s, (size_t)n);
        setp(buffer_.data(), buffer_.data() + buffer_.size());
        pbump(static_cast<int>(pos + n));
        return n;
    }

    // Seek support
    pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which) override {
        std::streamoff newpos;
        if (dir == std::ios_base::beg)
            newpos = off;
        else if (dir == std::ios_base::cur)
            newpos = (pptr() - pbase()) + off;
        else if (dir == std::ios_base::end)
            newpos = buffer_.size() + off;
        else
            return pos_type(off_type(-1));

        if (newpos < 0) return pos_type(off_type(-1));

        if ((size_t)newpos > buffer_.size())
            buffer_.resize((size_t)newpos);

        setp(buffer_.data(), buffer_.data() + buffer_.size());
        pbump(static_cast<int>(newpos));
        return pos_type(newpos);
    }

    pos_type seekpos(pos_type pos, std::ios_base::openmode which) override {
        return seekoff(pos, std::ios_base::beg, which);
    }

private:
    std::vector<char> buffer_;
};

class MemoryStream : public std::ostream {
public:
    MemoryStream() : std::ostream(&buffer_) {}
    std::vector<char> &data() { return buffer_.data(); }
    const std::vector<char> &data() const { return buffer_.data(); }
    std::string str() const { return std::string(buffer_.data().data(), buffer_.data().size()); }
private:
    MemoryBuffer buffer_;
};