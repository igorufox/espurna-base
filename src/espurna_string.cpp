/*

espurna-base: espurna::StringView and espurna::string (espurna_string.h)

*/

#include "espurna_string.h"

#include <strings.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <utility>

// ESP8266 flash strings are read as memory (the RTOS SDK maps .rodata), the plain C calls on every chip
#ifndef memcmp_P
#define memcmp_P memcmp
#endif
#ifndef strncasecmp_P
#define strncasecmp_P strncasecmp
#endif

namespace espurna {

bool StringView::equals(StringView other) const {
    if (other._len == _len) {
        // the same text, wherever it is. Two pointers of the flash range used to be compared by address only: on the
        // RTOS SDK the heap has IRAM (0x4010xxxx) too, a stored settings key and a key of a request both looked like
        // flash and never matched (11-18: every getSetting() failed, every setSetting() appended a copy)
        if (_ptr == other._ptr) {
            return true;
        } else if (inFlash(_ptr)) {
            return memcmp_P(other._ptr, _ptr, _len) == 0;
        } else if (inFlash(other._ptr)) {
            return memcmp_P(_ptr, other._ptr, _len) == 0;
        }

        return __builtin_memcmp(_ptr, other._ptr, _len) == 0;
    }

    return false;
}

bool StringView::equalsIgnoreCase(StringView other) const {
    if (other._len == _len) {
        if (inFlash(_ptr) && inFlash(other._ptr) && (_ptr == other._ptr)) {
            return true;
        } else if (inFlash(_ptr) || inFlash(other._ptr)) {
            espurna::string copy;
            const char* ptr = _ptr;
            if (inFlash(_ptr)) {
                copy = toString();
                ptr = copy.begin();
            }

            return strncasecmp_P(ptr, other._ptr, _len) == 0;
        }

        return __builtin_strncasecmp(_ptr, other._ptr, _len) == 0;
    }

    return false;
}

bool StringView::startsWith(StringView other) const {
    if (other._len <= _len) {
        return StringView(begin(), begin() + other._len).equals(other);
    }

    return false;
}

bool StringView::endsWith(StringView other) const {
    if (other._len <= _len) {
        return StringView(end() - other._len, end()).equals(other);
    }

    return false;
}

StringView StringView::slice(size_t index, size_t len) const {
    return StringView(_ptr + std::min(index, _len), std::min(len, _len - index));
}

StringView StringView::slice(size_t index) const {
    return slice(index, _len - index);
}

const string empty_string;

// the text grows by half of what it has (at least 16 bytes): appending one character at a time does not realloc
// every time
bool string::grow(size_t size) {
    if (size <= _capacity) {
        return true;
    }

    size_t capacity = _capacity + (_capacity / 2);
    if (capacity < size) {
        capacity = size;
    }
    if (capacity < 16) {
        capacity = 16;
    }

    auto* out = static_cast<char*>(std::realloc(_data, capacity + 1));
    if (!out) {
        // as much as was asked for, nothing ahead
        out = static_cast<char*>(std::realloc(_data, size + 1));
        if (!out) {
            return false;
        }
        capacity = size;
    }

    _data = out;
    _capacity = capacity;
    return true;
}

bool string::reserve(size_t size) {
    if (!grow(size)) {
        return false;
    }

    _data[_size] = '\0';
    return true;
}

void string::assign(const char* value, size_t size) {
    if (!value || !size) {
        clear();
        return;
    }

    // the value may be a part of this text
    if (_data && (value >= _data) && (value < (_data + _capacity))) {
        std::memmove(_data, value, size);
        _size = size;
        _data[_size] = '\0';
        return;
    }

    if (!grow(size)) {
        return;
    }

    std::memcpy(_data, value, size);
    _size = size;
    _data[_size] = '\0';
}

bool string::append(const char* value, size_t size) {
    if (!size || !value) {
        return true;
    }

    // the value may be a part of this text: its offset survives the realloc
    const bool inside = _data && (value >= _data) && (value < _data + _size);
    const size_t offset = inside ? static_cast<size_t>(value - _data) : 0;

    if (!grow(_size + size)) {
        return false;
    }

    std::memmove(_data + _size, inside ? _data + offset : value, size);
    _size += size;
    _data[_size] = '\0';
    return true;
}

namespace {

// `value` in `base` from the end of `out` (34 bytes and more); the first character
char* formatNumber(unsigned long long value, unsigned char base, char* end) {
    if ((base < 2) || (base > 36)) {
        base = 10;
    }

    char* ptr = end;
    *--ptr = '\0';
    do {
        const auto digit = static_cast<unsigned>(value % base);
        *--ptr = static_cast<char>((digit < 10) ? ('0' + digit) : ('a' + digit - 10));
        value /= base;
    } while (value);

    return ptr;
}

} // namespace

string::string(unsigned long long value, unsigned char base) {
    char buffer[66];
    const char* text = formatNumber(value, base, buffer + sizeof(buffer));
    assign(text, std::strlen(text));
}

string::string(long long value, unsigned char base) {
    char buffer[67];
    const bool negative = (value < 0) && (base == 10);
    const auto magnitude = negative
        ? static_cast<unsigned long long>(-(value + 1)) + 1
        : static_cast<unsigned long long>(value);
    char* text = formatNumber(magnitude, base, buffer + sizeof(buffer));
    if (negative) {
        *--text = '-';
    }
    assign(text, std::strlen(text));
}

string::string(int value, unsigned char base) :
    string(static_cast<long long>(value), base)
{}

string::string(unsigned value, unsigned char base) :
    string(static_cast<unsigned long long>(value), base)
{}

string::string(long value, unsigned char base) :
    string(static_cast<long long>(value), base)
{}

string::string(unsigned long value, unsigned char base) :
    string(static_cast<unsigned long long>(value), base)
{}

string::string(double value, unsigned char decimals) {
    char buffer[32];
    const auto length = formatDecimal(value, (decimals > 9) ? 9 : decimals, buffer, sizeof(buffer));
    assign(buffer, length);
}

bool string::equalsIgnoreCase(StringView other) const {
    if (other.length() != _size) {
        return false;
    }

    for (size_t index = 0; index < _size; ++index) {
        if (std::tolower(static_cast<unsigned char>(_data[index]))
            != std::tolower(static_cast<unsigned char>(other.data()[index])))
        {
            return false;
        }
    }

    return true;
}

int string::compareTo(StringView other) const {
    const size_t length = std::min(_size, other.length());
    const int out = length ? std::memcmp(c_str(), other.data(), length) : 0;
    if (out) {
        return out;
    }

    return (_size < other.length()) ? -1 : (_size > other.length()) ? 1 : 0;
}

bool string::startsWith(StringView prefix, size_t offset) const {
    return (offset <= _size) && StringView(c_str() + offset, _size - offset).startsWith(prefix);
}

int string::indexOf(char value, size_t from) const {
    for (size_t index = from; index < _size; ++index) {
        if (_data[index] == value) {
            return static_cast<int>(index);
        }
    }

    return -1;
}

int string::indexOf(StringView value, size_t from) const {
    if (!value.length()) {
        return (from <= _size) ? static_cast<int>(from) : -1;
    }

    for (size_t index = from; (index + value.length()) <= _size; ++index) {
        if (std::memcmp(_data + index, value.data(), value.length()) == 0) {
            return static_cast<int>(index);
        }
    }

    return -1;
}

int string::lastIndexOf(char value) const {
    return lastIndexOf(value, static_cast<int>(_size) - 1);
}

int string::lastIndexOf(char value, int from) const {
    if (from < 0) {
        return -1;
    }

    const size_t last = (static_cast<size_t>(from) < _size) ? static_cast<size_t>(from) + 1 : _size;
    for (size_t index = last; index > 0; --index) {
        if (_data[index - 1] == value) {
            return static_cast<int>(index - 1);
        }
    }

    return -1;
}

string string::substring(size_t from, size_t to) const {
    if (from > to) {
        std::swap(from, to);
    }
    if (to > _size) {
        to = _size;
    }
    if (from >= to) {
        return string();
    }

    return string(_data + from, to - from);
}

long string::toInt() const {
    return _size ? std::strtol(_data, nullptr, 10) : 0;
}

float string::toFloat() const {
    return _size ? static_cast<float>(parseDecimal(_data)) : 0.0f;
}

void string::trim() {
    if (!_size) {
        return;
    }

    size_t begin { 0 };
    while ((begin < _size) && std::isspace(static_cast<unsigned char>(_data[begin]))) {
        ++begin;
    }

    size_t end { _size };
    while ((end > begin) && std::isspace(static_cast<unsigned char>(_data[end - 1]))) {
        --end;
    }

    _size = end - begin;
    if (begin) {
        std::memmove(_data, _data + begin, _size);
    }
    _data[_size] = '\0';
}

void string::replace(char from, char to) {
    for (size_t index = 0; index < _size; ++index) {
        if (_data[index] == from) {
            _data[index] = to;
        }
    }
}

void string::replace(StringView from, StringView to) {
    if (!from.length() || !_size) {
        return;
    }

    string out;
    size_t index { 0 };
    while (index < _size) {
        const int found = indexOf(from, index);
        if (found < 0) {
            out.append(_data + index, _size - index);
            break;
        }

        out.append(_data + index, static_cast<size_t>(found) - index);
        out.append(to);
        index = static_cast<size_t>(found) + from.length();
    }

    *this = std::move(out);
}

void string::remove(size_t index) {
    remove(index, _size);
}

void string::remove(size_t index, size_t count) {
    if (index >= _size) {
        return;
    }

    if (count > (_size - index)) {
        count = _size - index;
    }

    std::memmove(_data + index, _data + index + count, _size - index - count);
    _size -= count;
    _data[_size] = '\0';
}

void string::toLowerCase() {
    for (size_t index = 0; index < _size; ++index) {
        _data[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(_data[index])));
    }
}

void string::toUpperCase() {
    for (size_t index = 0; index < _size; ++index) {
        _data[index] = static_cast<char>(std::toupper(static_cast<unsigned char>(_data[index])));
    }
}

string string::vformat(const char* format, va_list args) {
    string out;

    va_list copy;
    va_copy(copy, args);
    const int size = std::vsnprintf(nullptr, 0, format, copy);
    va_end(copy);

    if (size > 0) {
        auto* data = static_cast<char*>(std::malloc(size + 1));
        if (data) {
            std::vsnprintf(data, size + 1, format, args);
            out._data = data;
            out._size = static_cast<size_t>(size);
            out._capacity = static_cast<size_t>(size);
        }
    }

    return out;
}

string string::format(const char* format, ...) {
    va_list args;
    va_start(args, format);
    auto out = vformat(format, args);
    va_end(args);
    return out;
}

size_t formatDecimal(double value, unsigned char decimals, char* out, size_t size) {
    if (!size) {
        return 0;
    }

    if (value != value) {
        return static_cast<size_t>(std::snprintf(out, size, "%s", "nan"));
    }

    if (decimals > 9) {
        decimals = 9;
    }

    bool negative = value < 0.0;
    if (negative) {
        value = -value;
    }

    uint32_t scale { 1 };
    for (unsigned char n = 0; n < decimals; ++n) {
        scale *= 10;
    }

    // rounded at the last digit
    value += 0.5 / static_cast<double>(scale);
    if (value >= 4294967295.0) {
        return static_cast<size_t>(std::snprintf(out, size, "%s", negative ? "-inf" : "inf"));
    }

    const auto whole = static_cast<uint32_t>(value);
    const auto fraction = static_cast<uint32_t>((value - static_cast<double>(whole)) * static_cast<double>(scale));
    negative = negative && (whole || fraction);

    int length;
    if (decimals) {
        length = std::snprintf(out, size, "%s%lu.%0*lu", negative ? "-" : "",
            static_cast<unsigned long>(whole), static_cast<int>(decimals), static_cast<unsigned long>(fraction));
    } else {
        length = std::snprintf(out, size, "%s%lu", negative ? "-" : "", static_cast<unsigned long>(whole));
    }

    return (length > 0) ? static_cast<size_t>(length) : 0;
}

double parseDecimal(const char* text, const char** end) {
    const char* ptr = text;
    while ((*ptr == ' ') || (*ptr == '\t')) {
        ++ptr;
    }

    bool negative { false };
    if ((*ptr == '-') || (*ptr == '+')) {
        negative = (*ptr == '-');
        ++ptr;
    }

    double value { 0.0 };
    bool digits { false };
    for (; (*ptr >= '0') && (*ptr <= '9'); ++ptr) {
        value = (value * 10.0) + (*ptr - '0');
        digits = true;
    }

    if (*ptr == '.') {
        ++ptr;
        double scale { 0.1 };
        for (; (*ptr >= '0') && (*ptr <= '9'); ++ptr) {
            value += (*ptr - '0') * scale;
            scale *= 0.1;
            digits = true;
        }
    }

    if (!digits) {
        if (end) {
            *end = text;
        }
        return 0.0;
    }

    if ((*ptr == 'e') || (*ptr == 'E')) {
        const char* exp = ptr + 1;
        bool exp_negative { false };
        if ((*exp == '-') || (*exp == '+')) {
            exp_negative = (*exp == '-');
            ++exp;
        }

        if ((*exp >= '0') && (*exp <= '9')) {
            int power { 0 };
            for (; (*exp >= '0') && (*exp <= '9'); ++exp) {
                power = (power < 400) ? ((power * 10) + (*exp - '0')) : power;
            }

            for (; power > 0; --power) {
                value = exp_negative ? (value / 10.0) : (value * 10.0);
            }

            ptr = exp;
        }
    }

    if (end) {
        *end = ptr;
    }

    return negative ? -value : value;
}

} // namespace espurna
