/*

espurna-base: the text types of ESPurna and of the libraries of its modules (one string for both)

- reading: espurna::StringView, a pointer and a length
- keeping: espurna::string, a pointer and a length on the heap, terminated for C calls; it grows by the exact size of
  what is appended, nothing reserved ahead
- formatDecimal / parseDecimal: numbers with a decimal point without printf("%f") / strtod()

Moved from ESPurna's types.h / text_types.h (2026-11-19) without changes of the code.

*/

#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <type_traits>
#include <utility>

namespace espurna {

class string;

struct StringView {
    constexpr StringView() noexcept :
        _ptr(nullptr),
        _len(0)
    {}

    ~StringView() = default;

    StringView(std::nullptr_t) = delete;

    constexpr StringView(const StringView&) noexcept = default;
    constexpr StringView(StringView&&) noexcept = default;

#if __cplusplus > 201103L
    constexpr StringView& operator=(const StringView&) noexcept = default;
    constexpr StringView& operator=(StringView&&) noexcept = default;
#else
    StringView& operator=(const StringView&) noexcept = default;
    StringView& operator=(StringView&&) noexcept = default;
#endif

    constexpr StringView(const char* ptr, size_t len) noexcept :
        _ptr(ptr),
        _len(len)
    {}

    template <typename T, typename = typename std::enable_if<std::is_pointer<T>::value>::type>
    constexpr StringView(T ptr) noexcept :
        StringView(ptr, __builtin_strlen(ptr))
    {}

    template <size_t Size>
    constexpr StringView(const char (&string)[Size]) noexcept :
        StringView(&string[0], Size - 1)
    {}

    constexpr StringView(const char* begin, const char* end) noexcept :
        StringView(begin, end - begin)
    {}

    // espurna::string, and the espurna::string of a library
    template <typename T, typename = decltype(std::declval<const T&>().c_str(), std::declval<const T&>().length())>
    StringView(const T& string) noexcept :
        StringView(string.c_str(), string.length())
    {}

    template <size_t Size>
    constexpr StringView& operator=(const char (&string)[Size]) noexcept {
        _ptr = &string[0];
        _len = Size - 1;
        return *this;
    }

    constexpr const char* begin() const noexcept {
        return _ptr;
    }

    constexpr const char* end() const noexcept {
        return _ptr + _len;
    }

    constexpr const char* c_str() const {
        return _ptr;
    }

    constexpr const char* data() const {
        return _ptr;
    }

    constexpr const char& operator[](size_t offset) const {
        return *(_ptr + offset);
    }

    constexpr size_t length() const {
        return _len;
    }

    // a copy (text_types.h)
    string toString() const;

    bool equals(StringView) const;
    bool equalsIgnoreCase(StringView) const;

    bool startsWith(StringView) const;
    bool endsWith(StringView) const;

    StringView slice(size_t index, size_t end) const;
    StringView slice(size_t index) const;

private:
#if defined(HOST_MOCK)
    constexpr static bool inFlash(const char*) {
        return false;
    }
#else
    static bool inFlash(const char* ptr) {
        // common comparison would use >=0x40000000
        // instead, slightly reduce the footprint by
        // checking *only* for numbers below it
        static constexpr uintptr_t Mask { 1 << 30 };
        return (reinterpret_cast<uintptr_t>(ptr) & Mask) > 0;
    }
#endif

    const char* _ptr;
    size_t _len;
};

inline bool operator==(StringView lhs, StringView rhs) {
    return lhs.equals(rhs);
}

inline bool operator!=(StringView lhs, StringView rhs) {
    return !lhs.equals(rhs);
}


class string {
public:
    string() noexcept = default;

    string(StringView value) {
        assign(value.data(), value.length());
    }

    string(const char* value) {
        if (value) {
            assign(value, std::strlen(value));
        }
    }

    string(const char* value, size_t size) {
        assign(value, size);
    }

    explicit string(char value) {
        assign(&value, 1);
    }

    // a number in `base` (2 .. 36)
    explicit string(int value, unsigned char base = 10);
    explicit string(unsigned value, unsigned char base = 10);
    explicit string(long value, unsigned char base = 10);
    explicit string(unsigned long value, unsigned char base = 10);
    explicit string(long long value, unsigned char base = 10);
    explicit string(unsigned long long value, unsigned char base = 10);
    // `decimals` digits after the point (formatDecimal(), no printf("%f"))
    explicit string(double value, unsigned char decimals = 2);
    explicit string(float value, unsigned char decimals = 2) :
        string(static_cast<double>(value), decimals)
    {}

    explicit string(unsigned char value, unsigned char base = 10) :
        string(static_cast<unsigned>(value), base)
    {}

    string(const string& other) {
        assign(other._data, other._size);
    }

    string(string&& other) noexcept :
        _data(other._data),
        _size(other._size),
        _capacity(other._capacity)
    {
        other._data = nullptr;
        other._size = 0;
        other._capacity = 0;
    }

    ~string() {
        std::free(_data);
    }

    string& operator=(const string& other) {
        if (this != &other) {
            assign(other._data, other._size);
        }

        return *this;
    }

    string& operator=(string&& other) noexcept {
        if (this != &other) {
            std::free(_data);
            _data = other._data;
            _size = other._size;
            _capacity = other._capacity;
            other._data = nullptr;
            other._size = 0;
            other._capacity = 0;
        }

        return *this;
    }

    string& operator=(StringView value) {
        assign(value.data(), value.length());
        return *this;
    }

    string& operator=(const char* value) {
        if (value) {
            assign(value, std::strlen(value));
        } else {
            clear();
        }
        return *this;
    }

    const char* c_str() const noexcept {
        return _data ? _data : "";
    }

    const char* data() const noexcept {
        return c_str();
    }

    char* begin() noexcept {
        return _data;
    }

    char* end() noexcept {
        return _data + _size;
    }

    const char* begin() const noexcept {
        return c_str();
    }

    const char* end() const noexcept {
        return c_str() + _size;
    }

    size_t size() const noexcept {
        return _size;
    }

    size_t length() const noexcept {
        return _size;
    }

    bool empty() const noexcept {
        return _size == 0;
    }

    bool isEmpty() const noexcept {
        return _size == 0;
    }

    // `if (text)`: something in it
    explicit operator bool() const noexcept {
        return _size != 0;
    }

    StringView view() const noexcept {
        return StringView(c_str(), _size);
    }

    operator StringView() const noexcept {
        return view();
    }

    char operator[](size_t index) const {
        return (index < _size) ? _data[index] : '\0';
    }

    char& operator[](size_t index) {
        return _data[index];
    }

    char charAt(size_t index) const {
        return (*this)[index];
    }

    void setCharAt(size_t index, char value) {
        if (index < _size) {
            _data[index] = value;
        }
    }

    // empty, the memory stays (reserve()d texts are reused)
    void clear() noexcept {
        _size = 0;
        if (_data) {
            _data[0] = '\0';
        }
    }

    // room for `size` characters without another allocation. false: no memory
    bool reserve(size_t size);

    // false: no memory, the text stays as it was
    bool append(const char* value, size_t size);

    bool append(StringView value) {
        return append(value.data(), value.length());
    }

    bool append(char value) {
        return append(&value, 1);
    }

    bool append(const char* value) {
        return value ? append(value, std::strlen(value)) : true;
    }

    bool concat(StringView value) {
        return append(value);
    }

    bool concat(const char* value) {
        return value ? append(value, std::strlen(value)) : true;
    }

    bool concat(const char* value, size_t size) {
        return append(value, size);
    }

    bool concat(char value) {
        return append(value);
    }

    bool concat(const string& value) {
        return append(value.view());
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    bool concat(T value) {
        return append(string(value).view());
    }

    string& operator+=(StringView value) {
        append(value);
        return *this;
    }

    string& operator+=(const char* value) {
        if (value) {
            append(value, std::strlen(value));
        }
        return *this;
    }

    string& operator+=(const string& value) {
        append(value.view());
        return *this;
    }

    string& operator+=(char value) {
        append(value);
        return *this;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, char>::value>::type>
    string& operator+=(T value) {
        append(string(value).view());
        return *this;
    }

    bool equals(StringView other) const {
        return view() == other;
    }

    bool equalsIgnoreCase(StringView other) const;

    int compareTo(StringView other) const;

    bool startsWith(StringView prefix) const {
        return view().startsWith(prefix);
    }

    bool startsWith(StringView prefix, size_t offset) const;

    bool endsWith(StringView suffix) const {
        return view().endsWith(suffix);
    }

    // -1: not found
    int indexOf(char value, size_t from = 0) const;
    int indexOf(StringView value, size_t from = 0) const;
    // the last `value` at or before `from` (the whole string by default)
    int lastIndexOf(char value) const;
    int lastIndexOf(char value, int from) const;

    string substring(size_t from) const {
        return substring(from, _size);
    }

    string substring(size_t from, size_t to) const;

    // the number at the start (strtol of the text), 0 when none
    long toInt() const;
    float toFloat() const;

    // without the white space around
    void trim();

    void replace(char from, char to);
    void replace(StringView from, StringView to);

    // from `index` to the end, or `count` of them
    void remove(size_t index);
    void remove(size_t index, size_t count);

    void toLowerCase();
    void toUpperCase();

    // a formatted text (vsnprintf)
    static string format(const char* format, ...) __attribute__((format(printf, 1, 2)));
    static string vformat(const char* format, va_list args);

private:
    void assign(const char* value, size_t size);
    bool grow(size_t size);

    char* _data { nullptr };
    size_t _size { 0 };
    size_t _capacity { 0 };
};

// comparisons with exactly a string on one side (templates: a StringView or a "text" does not convert to a string
// to get here, StringView == "text" stays the StringView one)
template <typename T>
using IfString = typename std::enable_if<std::is_same<T, string>::value, bool>::type;

template <typename T>
inline IfString<T> operator==(const T& lhs, StringView rhs) {
    return lhs.view() == rhs;
}

template <typename T>
inline IfString<T> operator!=(const T& lhs, StringView rhs) {
    return !(lhs.view() == rhs);
}

template <typename T>
inline IfString<T> operator==(StringView lhs, const T& rhs) {
    return lhs == rhs.view();
}

template <typename T>
inline IfString<T> operator!=(StringView lhs, const T& rhs) {
    return !(lhs == rhs.view());
}

template <typename T>
inline IfString<T> operator==(const T& lhs, const T& rhs) {
    return lhs.view() == rhs.view();
}

template <typename T>
inline IfString<T> operator!=(const T& lhs, const T& rhs) {
    return !(lhs.view() == rhs.view());
}

template <typename T>
inline IfString<T> operator==(const T& lhs, const char* rhs) {
    return lhs.view() == StringView(rhs ? rhs : "");
}

template <typename T>
inline IfString<T> operator!=(const T& lhs, const char* rhs) {
    return !(lhs == rhs);
}

template <typename T>
inline IfString<T> operator<(const T& lhs, const T& rhs) {
    return lhs.compareTo(rhs) < 0;
}

inline string operator+(string lhs, StringView rhs) {
    lhs += rhs;
    return lhs;
}

inline string operator+(string lhs, const string& rhs) {
    lhs += rhs;
    return lhs;
}

inline string operator+(string lhs, const char* rhs) {
    lhs += rhs;
    return lhs;
}

inline string operator+(string lhs, char rhs) {
    lhs += rhs;
    return lhs;
}

inline string operator+(char lhs, const string& rhs) {
    string out(lhs);
    out += rhs;
    return out;
}

inline string operator+(const char* lhs, const string& rhs) {
    string out(lhs);
    out += rhs;
    return out;
}

inline string operator+(StringView lhs, const string& rhs) {
    string out(lhs);
    out += rhs;
    return out;
}

// an empty text, for references
extern const string empty_string;

inline string StringView::toString() const {
    return string(*this);
}


// "-12.50" of `value` with `decimals` digits after the point (no printf("%f"), the head has none): into `out`,
// terminated; the length. "nan", "inf", "-inf" for those
size_t formatDecimal(double value, unsigned char decimals, char* out, size_t size);

// "-12.5", "3e2": a decimal number (instead of strtod(), whose hex / inf / nan / locale parts are 7 KB of the head).
// `end` - past the last character taken, `text` when there was no number
double parseDecimal(const char* text, const char** end = nullptr);

} // namespace espurna
