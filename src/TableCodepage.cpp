#include <cpg/TableCodepage.h>

#include <utility>

int TableCodepage::minCharLen() {
    return 1;
}

int TableCodepage::maxCharLen() {
    return 1;
}

TableCodepage::TableCodepage(std::string name, uint16_t *convTable) : Codepage(std::move(name)), convTable(convTable) {
}

TableCodepage::TableCodepage(std::string name, uint16_t *convTable, std::pair<uint16_t, uint16_t> *bestTable)
        : TableCodepage(std::move(name), convTable) {
    this->bestTable = bestTable;
}

std::u32string TableCodepage::toU32(std::string_view str) {
    std::u32string result;
    result.reserve(str.size());
    for (unsigned char c : str) {
        if (c < 128) {
            result.push_back(static_cast<char32_t>(c));
        } else {
            uint16_t unicode = convTable[c - 128];
            if (unicode != 0 && unicode != 65533) {
                result.push_back(static_cast<char32_t>(unicode));
            }
            // Skip unmapped characters (0 or replacement char 65533)
        }
    }
    return result;
}

std::string TableCodepage::fromU32(u32string_view dstr) {
    std::string result;
    result.reserve(dstr.size());
    for (char32_t c : dstr) {
        if (c < 128) {
            result.push_back(static_cast<char>(c));
        } else {
            // Search in convTable for matching codepoint
            bool found = false;
            for (int i = 0; i < 128; i++) {
                if (convTable[i] == static_cast<uint16_t>(c)) {
                    result.push_back(static_cast<char>(i + 128));
                    found = true;
                    break;
                }
            }
            if (!found) {
                // Character not in codepage - use '?'
                result.push_back('?');
            }
        }
    }
    return result;
}

