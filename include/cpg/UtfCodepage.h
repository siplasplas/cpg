#ifndef CPG_UTFCODEPAGE_H
#define CPG_UTFCODEPAGE_H

#include <cpg/Codepage.h>

class UtfCodepage: public Codepage {
    int width;
    bool binEndian;
public:
    UtfCodepage(std::string name, int width, bool binEndian);
    int minCharLen() override;
    int maxCharLen() override;
    std::u32string toU32(std::string_view str) override;
    std::string fromU32(u32string_view sv32) override;
};


#endif //CPG_UTFCODEPAGE_H
