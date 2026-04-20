//
// Created by andrzej on 9/30/23.
//

#include <cpg/UtfCodepage.h>

#include <utility>
#include <vector>
#include <cstdint>

#include <unicode/utypes.h>
#include <unicode/ustring.h>

namespace {

char16_t swap16(char16_t c) {
    return (char16_t) (((c & 0xFF) << 8) | ((c & 0xFF00) >> 8));
}

char32_t reverse32(char32_t c) {
    return ((c & 0xFF) << 24) | ((c & 0xFF00) << 8)
         | ((c & 0xFF0000) >> 8) | ((c & 0xFF000000) >> 24);
}

void swapIt(std::u16string &u16) {
    for (auto &c : u16) c = swap16(c);
}

void reverseIt(std::u32string &u32) {
    for (auto &c : u32) c = reverse32(c);
}

std::u32string u8ToU32(std::string_view str) {
    UErrorCode status = U_ZERO_ERROR;
    int32_t destLen = 0;
    u_strFromUTF8(nullptr, 0, &destLen, str.data(), (int32_t) str.size(), &status);
    status = U_ZERO_ERROR;
    std::vector<UChar> u16((size_t) destLen);
    u_strFromUTF8(u16.data(), destLen, nullptr, str.data(), (int32_t) str.size(), &status);
    if (U_FAILURE(status) && status != U_STRING_NOT_TERMINATED_WARNING)
        return {};

    status = U_ZERO_ERROR;
    int32_t u32Len = 0;
    u_strToUTF32(nullptr, 0, &u32Len, u16.data(), destLen, &status);
    status = U_ZERO_ERROR;
    std::u32string result((size_t) u32Len, 0);
    u_strToUTF32(reinterpret_cast<UChar32*>(result.data()), u32Len, nullptr,
                 u16.data(), destLen, &status);
    return result;
}

std::u32string u16ToU32(std::u16string_view sv16) {
    UErrorCode status = U_ZERO_ERROR;
    int32_t u32Len = 0;
    u_strToUTF32(nullptr, 0, &u32Len, reinterpret_cast<const UChar*>(sv16.data()),
                 (int32_t) sv16.size(), &status);
    status = U_ZERO_ERROR;
    std::u32string result((size_t) u32Len, 0);
    u_strToUTF32(reinterpret_cast<UChar32*>(result.data()), u32Len, nullptr,
                 reinterpret_cast<const UChar*>(sv16.data()),
                 (int32_t) sv16.size(), &status);
    return result;
}

std::string u32ToU8(std::u32string_view sv32) {
    UErrorCode status = U_ZERO_ERROR;
    int32_t u16Len = 0;
    u_strFromUTF32(nullptr, 0, &u16Len,
                   reinterpret_cast<const UChar32*>(sv32.data()),
                   (int32_t) sv32.size(), &status);
    status = U_ZERO_ERROR;
    std::vector<UChar> u16((size_t) u16Len);
    u_strFromUTF32(u16.data(), u16Len, nullptr,
                   reinterpret_cast<const UChar32*>(sv32.data()),
                   (int32_t) sv32.size(), &status);

    status = U_ZERO_ERROR;
    int32_t u8Len = 0;
    u_strToUTF8(nullptr, 0, &u8Len, u16.data(), u16Len, &status);
    status = U_ZERO_ERROR;
    std::string result((size_t) u8Len, 0);
    u_strToUTF8(result.data(), u8Len, nullptr, u16.data(), u16Len, &status);
    return result;
}

std::u16string u32ToU16(std::u32string_view sv32) {
    UErrorCode status = U_ZERO_ERROR;
    int32_t u16Len = 0;
    u_strFromUTF32(nullptr, 0, &u16Len,
                   reinterpret_cast<const UChar32*>(sv32.data()),
                   (int32_t) sv32.size(), &status);
    status = U_ZERO_ERROR;
    std::u16string result((size_t) u16Len, 0);
    u_strFromUTF32(reinterpret_cast<UChar*>(result.data()), u16Len, nullptr,
                   reinterpret_cast<const UChar32*>(sv32.data()),
                   (int32_t) sv32.size(), &status);
    return result;
}

} // namespace

int UtfCodepage::minCharLen() {
    return width;
}

int UtfCodepage::maxCharLen() {
    return 4;
}

UtfCodepage::UtfCodepage(std::string name, int width, bool binEndian) : Codepage(std::move(name)),
                                                                        width(width), binEndian(binEndian) {
}

std::u32string UtfCodepage::toU32(std::string_view str) {
    switch (width) {
        case 1: return u8ToU32(str);
        case 2: {
            std::u16string_view sv16((char16_t*)str.data(), str.size()/2);
            if (!binEndian)
                return u16ToU32(sv16);
            std::u16string str16(sv16);
            swapIt(str16);
            return u16ToU32(str16);
        }
        case 4: {
            std::u32string_view sv32((char32_t*)str.data(), str.size()/4);
            std::u32string str32(sv32);
            if (binEndian)
                reverseIt(str32);
            return str32;
        }
        default: return {};
    }
}

std::string UtfCodepage::fromU32(std::u32string_view sv32) {
    switch (width) {
        case 1:
            return u32ToU8(sv32);
        case 2: {
            std::u16string str16 = u32ToU16(sv32);
            if (binEndian)
                swapIt(str16);
            return std::string((const char*) str16.data(), str16.size() * 2);
        }
        case 4: {
            std::u32string str32(sv32);
            if (binEndian)
                reverseIt(str32);
            return std::string((const char*) str32.data(), str32.size() * 4);
        }
        default:
            return {};
    }
}
