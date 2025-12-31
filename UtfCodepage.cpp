//
// Created by andrzej on 9/30/23.
//

#include "UtfCodepage.h"

#include <utility>

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
    UTF utf;
    switch (width) {
        case 1: return utf.toUTF32(str);
        case 2: {
            u16string_view sv16((char16_t*)str.data(), str.size()/2);
            if (!binEndian)
                return UTF::toUTF32(sv16);
            else {
                std::u16string str16;
                str16 = sv16;
                UTF::swapIt(str16);
                return UTF::toUTF32(str16);
            }
        }
        case 4: {
            u32string_view sv32((char32_t*)str.data(), str.size()/4);
            std::u32string str32;
            str32 = sv32;
            if (binEndian)
                UTF::reverseIt(str32);
            return str32;
        }
        default: return {};
    }
}

std::string UtfCodepage::fromU32(u32string_view sv32) {
    UTF utf;
    switch (width) {
        case 1: {
            return utf.fromUTF32(sv32);
        }
        case 2: {
            std::u16string str16 = utf.fromUTF32to16(sv32);
            if (binEndian)
                UTF::swapIt(str16);
            std::string_view sv8((char *) str16.c_str(), str16.size() * 2);
            std::string str;
            str = sv8;
            return str;
        }
        case 4: {
            std::u32string str32;
            str32 = sv32;
            if (binEndian)
                UTF::reverseIt(str32);
            std::string_view sv8((char *) str32.c_str(), str32.size() * 4);
            std::string str;
            str = sv8;
            return str;
        }
        default:
            return {};
    }
}
