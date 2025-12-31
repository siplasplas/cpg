#ifndef CPG_TABLECODEPAGE_H
#define CPG_TABLECODEPAGE_H

#include <cstdint>
#include <utility>
#include <cpg/Codepage.h>

class TableCodepage: public Codepage {
    uint16_t *convTable;
    std::pair<uint16_t,uint16_t> *bestTable = nullptr;
public:
    TableCodepage(std::string name, uint16_t *convTable);
    TableCodepage(std::string name, uint16_t *convTable, std::pair<uint16_t,uint16_t> *bestTable);
    int minCharLen() override;
    int maxCharLen() override;
    std::u32string toU32(std::string_view str) override;
    std::string fromU32(u32string_view dstr) override;
};

#endif //CPG_TABLECODEPAGE_H
