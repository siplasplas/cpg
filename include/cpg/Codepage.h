#ifndef CPG_CODEPAGE_H
#define CPG_CODEPAGE_H

#include <string>
#include <utf/UTF.hpp>

class Codepage {
protected:
    std::string name;
public:
    explicit Codepage(std::string name);
    virtual int minCharLen() = 0;
    virtual int maxCharLen() = 0;
    virtual std::u32string toU32(std::string_view str) = 0;
    virtual std::string fromU32(u32string_view dstr) = 0;
    const std::string& getName() const { return name; }
};

#endif //CPG_CODEPAGE_H
