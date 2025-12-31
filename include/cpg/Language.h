#ifndef CPG_LANGUAGE_H
#define CPG_LANGUAGE_H

#include <string>
#include <vector>
#include <filesystem>
#include <unordered_set>

using namespace std;

struct Language {
    string name;
    string iso_code;
    bool use_ascii;
    vector<string> charsets;
    u16string alphabet;
    void read(ifstream &inStream);
    string key(string line);
    static string subQuotes(string line);
    unordered_set<uint16_t> alphabetToSet();
};

class Languages {
    vector<Language> languages;
public:
    int size();
    void readFromFile(const filesystem::path &path);
    unordered_set<uint16_t> alphabetToSet(int n);

    // New methods for language-codepage mapping
    vector<pair<string, string>> listAll();  // Returns pairs of (iso_code, name)
    const Language* getByIsoCode(const string& iso);
    vector<string> getCharsetsForLanguage(const string& iso);
};

#endif //CPG_LANGUAGE_H
