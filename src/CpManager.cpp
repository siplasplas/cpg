#include <algorithm>
#include <cctype>
#include <cpg/CpManager.h>
#include <cpg/UtfCodepage.h>
#include <cpg/TableCodepage.h>

// Include all codepage tables
#include "../data/ISO8859.h"
#include "../data/PC.h"
#include "../data/WINDOWS.h"
#include "../data/MAC.h"
#include "../data/Apple.h"
#include "../data/GSM.h"
#include "../data/ecbdic.h"
#include "../data/misc.h"

using namespace std;

CpManager::~CpManager() {
    for (auto cp: codepages)
        delete cp;
}

CpManager::CpManager() {
    // UTF encodings
    codepages.emplace_back(new UtfCodepage("utf8", 1, false));
    codepages.emplace_back(new UtfCodepage("utf16", 2, false));
    codepages.emplace_back(new UtfCodepage("utf16be", 2, true));
    codepages.emplace_back(new UtfCodepage("utf32", 4, false));
    codepages.emplace_back(new UtfCodepage("utf32be", 4, true));

    // ISO 8859 family
    codepages.emplace_back(new TableCodepage("iso-8859-1", ISO_8859_1));
    codepages.emplace_back(new TableCodepage("iso-8859-2", ISO_8859_2));
    codepages.emplace_back(new TableCodepage("iso-8859-3", ISO_8859_3));
    codepages.emplace_back(new TableCodepage("iso-8859-4", ISO_8859_4));
    codepages.emplace_back(new TableCodepage("iso-8859-5", ISO_8859_5));
    codepages.emplace_back(new TableCodepage("iso-8859-6", ISO_8859_6));
    codepages.emplace_back(new TableCodepage("iso-8859-7", ISO_8859_7));
    codepages.emplace_back(new TableCodepage("iso-8859-8", ISO_8859_8));
    codepages.emplace_back(new TableCodepage("iso-8859-9", ISO_8859_9));
    codepages.emplace_back(new TableCodepage("iso-8859-10", ISO_8859_10));
    codepages.emplace_back(new TableCodepage("iso-8859-11", ISO_8859_11));
    codepages.emplace_back(new TableCodepage("iso-8859-13", ISO_8859_13));
    codepages.emplace_back(new TableCodepage("iso-8859-14", ISO_8859_14));
    codepages.emplace_back(new TableCodepage("iso-8859-15", ISO_8859_15));
    codepages.emplace_back(new TableCodepage("iso-8859-16", ISO_8859_16));

    // Windows codepages
    codepages.emplace_back(new TableCodepage("cp1250", CP1250));
    codepages.emplace_back(new TableCodepage("cp1251", CP1251));
    codepages.emplace_back(new TableCodepage("cp1252", CP1252));
    codepages.emplace_back(new TableCodepage("cp1253", CP1253));
    codepages.emplace_back(new TableCodepage("cp1254", CP1254));
    codepages.emplace_back(new TableCodepage("cp1255", CP1255));
    codepages.emplace_back(new TableCodepage("cp1256", CP1256));
    codepages.emplace_back(new TableCodepage("cp1257", CP1257));
    codepages.emplace_back(new TableCodepage("cp1258", CP1258));
    codepages.emplace_back(new TableCodepage("cp874", CP874));

    // DOS/PC codepages
    codepages.emplace_back(new TableCodepage("cp437", CP437));
    codepages.emplace_back(new TableCodepage("cp737", CP737));
    codepages.emplace_back(new TableCodepage("cp775", CP775));
    codepages.emplace_back(new TableCodepage("cp850", CP850));
    codepages.emplace_back(new TableCodepage("cp852", CP852));
    codepages.emplace_back(new TableCodepage("cp855", CP855));
    codepages.emplace_back(new TableCodepage("cp856", CP856));
    codepages.emplace_back(new TableCodepage("cp857", CP857));
    codepages.emplace_back(new TableCodepage("cp860", CP860));
    codepages.emplace_back(new TableCodepage("cp861", CP861));
    codepages.emplace_back(new TableCodepage("cp862", CP862));
    codepages.emplace_back(new TableCodepage("cp863", CP863));
    codepages.emplace_back(new TableCodepage("cp864", CP864));
    codepages.emplace_back(new TableCodepage("cp865", CP865));
    codepages.emplace_back(new TableCodepage("cp866", CP866));
    codepages.emplace_back(new TableCodepage("cp869", CP869));

    // Cyrillic
    codepages.emplace_back(new TableCodepage("koi8-r", KOI8_R));
    codepages.emplace_back(new TableCodepage("koi8-u", KOI8_U));

    // Mac codepages
    codepages.emplace_back(new TableCodepage("mac-roman", ROMAN));
    codepages.emplace_back(new TableCodepage("mac-greek", GREEK));
    codepages.emplace_back(new TableCodepage("mac-cyrillic", CYRILLIC));
    codepages.emplace_back(new TableCodepage("mac-iceland", ICELAND));
    codepages.emplace_back(new TableCodepage("mac-turkish", TURKISH));
    codepages.emplace_back(new TableCodepage("mac-latin2", LATIN2));

    // Apple codepages
    codepages.emplace_back(new TableCodepage("apple-arabic", cpARABIC));
    codepages.emplace_back(new TableCodepage("apple-celtic", cpCELTIC));
    codepages.emplace_back(new TableCodepage("apple-centeur", cpCENTEURO));
    codepages.emplace_back(new TableCodepage("apple-croatian", cpCROATIAN));
    codepages.emplace_back(new TableCodepage("apple-cyrillic", cpCYRILLIC));
    codepages.emplace_back(new TableCodepage("apple-devanaga", cpDEVANAGA));
    codepages.emplace_back(new TableCodepage("apple-farsi", cpFARSI));
    codepages.emplace_back(new TableCodepage("apple-gaelic", cpGAELIC));
    codepages.emplace_back(new TableCodepage("apple-greek", cpGREEK));
    codepages.emplace_back(new TableCodepage("apple-gujarati", cpGUJARATI));
    codepages.emplace_back(new TableCodepage("apple-gurmukhi", cpGURMUKHI));
    codepages.emplace_back(new TableCodepage("apple-hebrew", cpHEBREW));
    codepages.emplace_back(new TableCodepage("apple-iceland", cpICELAND));
    codepages.emplace_back(new TableCodepage("apple-inuit", cpINUIT));
    codepages.emplace_back(new TableCodepage("apple-roman", cpROMAN));
    codepages.emplace_back(new TableCodepage("apple-romanian", cpROMANIAN));
    codepages.emplace_back(new TableCodepage("apple-thai", cpTHAI));
    codepages.emplace_back(new TableCodepage("apple-turkish", cpTURKISH));
    codepages.emplace_back(new TableCodepage("apple-ukraine", cpUKRAINE));

    // GSM 7-bit
    codepages.emplace_back(new TableCodepage("gsm", GSM));

    // EBCDIC
    codepages.emplace_back(new TableCodepage("ebcdic", ecbdic));

    // Misc
    codepages.emplace_back(new TableCodepage("atarist", ATARIST));
    codepages.emplace_back(new TableCodepage("kz1048", KZ1048));
}

Codepage* CpManager::getByName(const std::string& name) {
    // Case-insensitive search
    string lowerName = name;
    transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
              [](unsigned char c) { return tolower(c); });

    for (auto* cp : codepages) {
        string cpName = cp->getName();
        transform(cpName.begin(), cpName.end(), cpName.begin(),
                  [](unsigned char c) { return tolower(c); });
        if (cpName == lowerName) {
            return cp;
        }
    }
    return nullptr;
}

vector<string> CpManager::listAll() {
    vector<string> names;
    names.reserve(codepages.size());
    for (auto* cp : codepages) {
        names.push_back(cp->getName());
    }
    return names;
}
