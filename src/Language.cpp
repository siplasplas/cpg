//
// Created by andrzej on 9/30/23.
//

#include <fstream>
#include <iostream>
#include <cpg/Language.h>
#include <utf/UTF.hpp>

using namespace std;

void Languages::readFromFile(const filesystem::path &path) {
    ifstream infile(path);
    while (!infile.eof()) {
        Language lang;
        lang.read(infile);
        languages.push_back(lang);
        string line;
        getline(infile, line);
    }
}

int Languages::size() {
    return languages.size();
}

unordered_set<uint16_t> Languages::alphabetToSet(int n) {
    return languages[n].alphabetToSet();
}

string Language::key(string line) {
    int pos1 = line.find('=');
    if (pos1 < 0) throw runtime_error("not found =");
    return line.substr(0, pos1);
}

string Language::subQuotes(string line) {
    int pos1 = line.find('\"');
    if (pos1 < 0) throw runtime_error("not found begin quote");
    int pos2 = line.find('\"', pos1 + 1);
    if (pos2 < 0) throw runtime_error("not found end quote");
    return line.substr(pos1 + 1, pos2 - pos1 - 1);
}

// Parse charsets=["ISO-8859-6", "WINDOWS-1256", ...]
static vector<string> parseCharsets(const string& line) {
    vector<string> result;
    size_t start = line.find('[');
    size_t end = line.find(']');
    if (start == string::npos || end == string::npos) return result;

    string content = line.substr(start + 1, end - start - 1);
    size_t pos = 0;
    while ((pos = content.find('"')) != string::npos) {
        size_t endQuote = content.find('"', pos + 1);
        if (endQuote == string::npos) break;
        result.push_back(content.substr(pos + 1, endQuote - pos - 1));
        content = content.substr(endQuote + 1);
    }
    return result;
}

void Language::read(ifstream &inStream) {
    string line;
    getline(inStream, line);
    name = subQuotes(line);
    assert(key(line) == "name");
    getline(inStream, line);
    iso_code = subQuotes(line);
    assert(key(line) == "iso_code");
    getline(inStream, line);  // use_ascii - skip
    getline(inStream, line);  // charsets
    charsets = parseCharsets(line);
    getline(inStream, line);  // alphabet
    UTF utf;
    alphabet = utf.toUTF16(subQuotes(line));
    assert(key(line) == "alphabet");
}

unordered_set<uint16_t> Language::alphabetToSet() {
    unordered_set<uint16_t> set;
    for (auto c: alphabet)
        set.insert(c);
    return set;
}

vector<pair<string, string>> Languages::listAll() {
    vector<pair<string, string>> result;
    result.reserve(languages.size());
    for (const auto& lang : languages) {
        result.emplace_back(lang.iso_code, lang.name);
    }
    return result;
}

const Language* Languages::getByIsoCode(const string& iso) {
    for (const auto& lang : languages) {
        if (lang.iso_code == iso) {
            return &lang;
        }
    }
    return nullptr;
}

vector<string> Languages::getCharsetsForLanguage(const string& iso) {
    const Language* lang = getByIsoCode(iso);
    if (lang) {
        return lang->charsets;
    }
    return {};
}

