#include <cpg/PolishDosCodepages.h>
#include <cpg/TableCodepage.h>
#include <cstring>
#include <cstdint>
#include <initializer_list>

// Base tables are defined in CpManager.cpp (via data/*.h includes). We only
// reference them here to avoid duplicating ~20 KB of static data across TUs.
extern uint16_t CP437[128];
extern uint16_t ISO_8859_1[128];
extern uint16_t ROMAN[128];

namespace {

// Order: Ą Ć Ę Ł Ń Ó Ś Ź Ż  ą ć ę ł ń ó ś ź ż
constexpr char32_t kPolishChars[18] = {
    0x0104, 0x0106, 0x0118, 0x0141, 0x0143, 0x00D3, 0x015A, 0x0179, 0x017B,
    0x0105, 0x0107, 0x0119, 0x0142, 0x0144, 0x00F3, 0x015B, 0x017A, 0x017C
};

struct PolTable {
    uint16_t data[128];
    PolTable(const uint16_t* base, std::initializer_list<uint8_t> positions) {
        std::memcpy(data, base, sizeof(data));
        int i = 0;
        for (uint8_t pos : positions) {
            if (pos >= 128)
                data[pos - 128] = static_cast<uint16_t>(kPolishChars[i]);
            i++;
        }
    }
};

// DOS-era codepages (base CP437)
PolTable t_mazovia      (CP437,      {143,149,144,156,165,163,152,160,161,134,141,145,146,164,162,158,166,167});
PolTable t_dhn          (CP437,      {128,129,130,131,132,133,134,136,135,137,138,139,140,141,142,143,145,144});
PolTable t_csk          (CP437,      {128,129,130,131,132,133,134,136,135,160,161,162,163,164,165,166,168,167});
PolTable t_cyfromat     (CP437,      {128,129,130,131,132,133,134,136,135,144,145,146,147,148,149,150,152,151});
PolTable t_iinte_isis   (CP437,      {128,129,130,131,132,133,134,135,136,144,145,146,147,148,149,150,151,152});
PolTable t_iea_swierk   (CP437,      {143,128,144,156,165,153,235,157,146,160,155,130,159,164,162,135,168,145});
PolTable t_logic        (CP437,      {128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145});
PolTable t_microvex     (CP437,      {143,128,144,156,165,147,152,157,146,160,155,130,159,164,162,135,168,145});
PolTable t_ventura      (CP437,      {151,153,165,166,146,143,142,144,128,150,148,164,167,145,162,132,130,135});
PolTable t_fido_mazovia (CP437,      {143,149,144,156,165,163,152,160,161,134,135,145,146,164,162,158,166,167});
PolTable t_smc          (CP437,      {198,188,201,157,185,250,156,225,237,230, 99,197,229,165,243,215,142,158});
PolTable t_elwro_junior (CP437,      {193,195,197,204,206,207,211,218,217,225,227,229,236,238,239,243,250,249});
PolTable t_atari_calamus(CP437,      {193,194,195,196,197,198,199,200,201,209,210,211,212,213,214,215,216,217});
PolTable t_corel_draw   (CP437,      {197,242,201,163,209,211,255,225,237,229,236,230,198,241,243,165,170,186});
PolTable t_corel_2      (CP437,      {194,199,202,206,209,211,212,218,219,226,231,234,238,241,243,244,250,251});
PolTable t_atm          (CP437,      {196,199,203,208,209,211,214,218,220,228,231,235,240,241,243,246,250,252});
PolTable t_texpl        (CP437,      {129,130,134,138,139,211,145,153,155,161,162,166,170,171,243,177,185,187});

// Amiga (base ISO-8859-1)
PolTable t_amiga_pl     (ISO_8859_1, {194,202,203,206,207,211,212,218,219,226,234,235,238,239,243,244,250,251});
PolTable t_amiga_fat    (ISO_8859_1, {192,193,194,195,196,197,198,200,199,230,231,234,238,241,243,245,251,254});
PolTable t_amiga_xjp    (ISO_8859_1, {198,199,202,206,209,211,213,219,222,230,231,234,238,241,243,245,251,254});

// Mac Polish (base Mac Roman)
PolTable t_mac_pl       (ROMAN,      {132,140,162,252,193,238,229,143,251,136,141,171,184,196,151,230,144,253});

} // namespace

std::vector<Codepage*> makePolishDosCodepages() {
    return {
        // DOS
        new TableCodepage("mazovia",       t_mazovia.data),
        new TableCodepage("dhn",           t_dhn.data),
        new TableCodepage("csk",           t_csk.data),
        new TableCodepage("cyfromat",      t_cyfromat.data),
        new TableCodepage("iinte-isis",    t_iinte_isis.data),
        new TableCodepage("iea-swierk",    t_iea_swierk.data),
        new TableCodepage("logic-pl",      t_logic.data),
        new TableCodepage("microvex",      t_microvex.data),
        new TableCodepage("ventura-pl",    t_ventura.data),
        new TableCodepage("fido-mazovia",  t_fido_mazovia.data),
        new TableCodepage("smc",           t_smc.data),
        new TableCodepage("elwro-junior",  t_elwro_junior.data),
        new TableCodepage("atari-calamus", t_atari_calamus.data),
        new TableCodepage("coreldraw-pl",  t_corel_draw.data),
        new TableCodepage("corel2-pl",     t_corel_2.data),
        new TableCodepage("atm-pl",        t_atm.data),
        new TableCodepage("texpl",         t_texpl.data),
        // Amiga
        new TableCodepage("amigapl",       t_amiga_pl.data),
        new TableCodepage("amiga-fat",     t_amiga_fat.data),
        new TableCodepage("amiga-xjp",     t_amiga_xjp.data),
        // Mac PL variant
        new TableCodepage("mac-pl",        t_mac_pl.data),
    };
}
