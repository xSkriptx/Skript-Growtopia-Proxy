#pragma once











#include <string>
#include <vector>
#include <utility>
#include <windows.h>


constexpr unsigned char _xk(size_t i, unsigned char base) noexcept {
    return static_cast<unsigned char>(base ^ static_cast<unsigned char>((i * 7u + 13u) & 0xFFu));
}


constexpr unsigned char _xe(char c, size_t i, unsigned char base) noexcept {
    return static_cast<unsigned char>(static_cast<unsigned char>(c) ^ _xk(i, base));
}


#define _XBASE(L) static_cast<unsigned char>((((unsigned)(L) * 31u + 17u) & 0xFFu) | 1u)




template<size_t N>
struct _XBuf {
    unsigned char d[N];   
    unsigned char b;      
    static constexpr size_t n = N;
};




template<size_t N, unsigned char BASE, size_t... Is>
constexpr _XBuf<N> _make_xbuf_impl(const char (&s)[N+1],
                                    std::index_sequence<Is...>) noexcept {
    return _XBuf<N>{ { _xe(s[Is], Is, BASE)... }, BASE };
}

template<size_t N, unsigned char BASE>
constexpr _XBuf<N-1> _make_xbuf(const char (&s)[N]) noexcept {
    return _make_xbuf_impl<N-1, BASE>(s, std::make_index_sequence<N-1>{});
}






__declspec(noinline) inline std::string _xdec(
    const unsigned char* enc, size_t n, unsigned char base)
{
    const volatile unsigned char* p = enc;
    std::string out(n, '\0');
    for (size_t i = 0; i < n; ++i)
        out[i] = static_cast<char>(static_cast<unsigned char>(p[i]) ^ _xk(i, base));
    return out;
}








#define DEC(s) ([]() -> std::string { \
    static constexpr auto _xb = _make_xbuf<sizeof(s), _XBASE(__LINE__)>(s); \
    return _xdec(_xb.d, _xb.n, _xb.b); \
}())


#define SE(s) DEC(s)





class EncStr {
    std::vector<unsigned char> data_;
    unsigned char key_[16]{};

    void rekey() noexcept {
        LARGE_INTEGER li{};
        QueryPerformanceCounter(&li);
        for (int i = 0; i < 16; ++i)
            key_[i] = static_cast<unsigned char>(
                (li.QuadPart >> (i * 3)) ^ (i * 97 + 13) ^ (li.QuadPart & 0xFF));
    }

public:
    EncStr()  { rekey(); }
    ~EncStr() { wipe(); }

    void store(const std::string& s) {
        rekey();
        data_.resize(s.size());
        for (size_t i = 0; i < s.size(); ++i)
            data_[i] = static_cast<unsigned char>(s[i]) ^ key_[i % 16];
    }

    std::string load() const {
        std::string out(data_.size(), '\0');
        for (size_t i = 0; i < data_.size(); ++i)
            out[i] = static_cast<char>(data_[i] ^ key_[i % 16]);
        return out;
    }

    bool empty() const noexcept { return data_.empty(); }

    void wipe() noexcept {
        if (!data_.empty()) SecureZeroMemory(data_.data(), data_.size());
        SecureZeroMemory(key_, sizeof(key_));
        data_.clear();
    }
};
