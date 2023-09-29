#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;
using namespace std;

string inFile = "../www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1250.TXT";
string inFileBest = "../www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WindowsBestFit/bestfit1250.txt";
string CP936 = "../www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP936.TXT";
string CP936Best = "../www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WindowsBestFit/bestfit936.txt";
string CP1361Best = "../www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WindowsBestFit/bestfit1361.txt";

string trimLeft(const string& str)
{
    const auto strBegin = str.find_first_not_of(" \t");
    if (strBegin == string::npos)
        return ""; // no content
    return str.substr(strBegin, str.length() - strBegin);
}


string trimRight(const string& str)
{
    const auto strEnd = str.find_last_not_of(" \t\r");
    if (strEnd == string::npos)
        return ""; // no content
    return str.substr(0, strEnd + 1);
}

string trim(const string& str) {
    return trimLeft(trimRight(str));
}

template<typename I>
std::string n2hexstr(I w, size_t hex_len = sizeof(I) << 1) {
    static const char *digits = "0123456789ABCDEF";
    std::string rc(hex_len, '0');
    for (size_t i = 0, j = (hex_len - 1) * 4; i < hex_len; ++i, j -= 4)
        rc[i] = digits[(w >> j) & 0x0f];
    return rc;
}

std::string str_tolower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); }
    );
    return s;
}

int counter = 0;
bool readTwo(const string &line, int &a, int &b) {
    counter++;
    int pos1 = line.find('\t');
    if (pos1 < 0)
        pos1 = line.find("  ");
    if (pos1 < 0)
        throw runtime_error("can't read two hex integers");
    string s1 = line.substr(0, pos1);
    size_t idx;
    a = stoi(s1, &idx, 16);
    int pos2start = pos1 + 1;
    int pos2 = line.find('\t', pos2start);
    string s2;
    if (pos2 < 0)
        pos2 = line.find("  ", pos2start);
    if (pos2 < 0)
        pos2 = line.length();
    s2 = line.substr(pos2start, pos2 - pos2start);
    s2 = trim(s2);
    if (s2.empty()) {
        int pos3 = line.find('\t', pos2 + 1);
        string s3 = line.substr(pos2 + 1, pos3 - pos2 - 1);
        if (s3 == "#DBCS LEAD BYTE") {
            b = a;
            return true;
        } else {
            b = 0xfffd;
            return false;
        }
    } else
        b = stoi(s2, &idx, 16);
    return false;
}

void readMBtable(ifstream &infile, int mbSize, uint16_t tab[], uint16_t *dbcsroot[]) {
    for (int i = 0; i < 128; i++)
        tab[i] = 0xfffd;
    if (dbcsroot)
        for (int i = 0; i < 128; i++)
            dbcsroot[i] = nullptr;
    int i = 0;
    for (string line; getline(infile, line);) {
        line = trimRight(line);
        if (line.empty()) continue;
        if (line[0] == '#') continue;
        int a, b;
        bool dbcs = readTwo(line, a, b);
        if (a >= 128)
            tab[a - 128] = dbcs ? 0 : b;
        else if (a != b)
            throw runtime_error("7bit ascii code mismatch");
        if (dbcs) {
            cout << "dbcs" << n2hexstr(b, 2) << endl;
            if (!dbcsroot[a - 128]) {
                dbcsroot[a - 128] = new uint16_t[256];
                memset(dbcsroot[a - 128], 0, 256 * sizeof(uint16_t));
            }
        }
        i++;
        if (i==mbSize)
            break;
    }
}

void readDBCStable(ifstream &infile, uint16_t *dbcsroot[], int n) {
    string line;
    for (line; getline(infile, line);) {
        line = trimRight(line);
        if (!line.empty()) break;
    }
    if (line.find("DBCSTABLE") != 0)
        throw runtime_error("can't find DBCSTABLE");
    int dbcsSize = stoi(line.substr(10));
    getline(infile, line);
    line = trimRight(line);
    if (!line.empty()) throw runtime_error("must be empty line");
    dbcsroot[n - 128] = new uint16_t[256];
    memset(dbcsroot[n - 128], 0, 256 * sizeof(uint16_t));
    for (int i = 0; i < dbcsSize; i++) {
        getline(infile, line);
        int a, b;
        readTwo(line, a, b);
        dbcsroot[n - 128][a] = b;
    }
}

void readDBCStables(ifstream &infile, uint16_t *dbcsroot[]) {
    for (string line; getline(infile, line);) {
        line = trimRight(line);
        if (line.empty()) break;
        int a, b;
        readTwo(line, a, b);
        dbcsroot[(a>>8)-128][a & 0xff] = b;
    }
}

void printTab(uint16_t tab[], ofstream &outStream, string name) {
    outStream << "uint16_t " + name + "[128] = {" << endl;
    for (int i = 0; i < 128; i += 16) {
        outStream << "    ";
        for (int j = i; j < i + 16; j++) {
            outStream << tab[j];
            if (j < 127)
                outStream << ",";
        }
        if (i == 128 - 16)
            outStream << "};";
        outStream << endl;
    }
}

void printBestTab(vector<pair<uint16_t, uint8_t>> &bestv, ofstream &outStream, string name) {
    outStream << "uint16_t " + name + "[" << bestv.size() << "][2] = {" << endl;
    bool isEnd = false;
    for (int i = 0; i < bestv.size(); i += 16) {
        outStream << "    ";
        for (int j = i; j < min(i + 16, (int)bestv.size()); j++) {
            outStream << bestv[j].first;
            if (j < bestv.size() - 1)
                outStream << ",";
            else
                isEnd = true;
        }
        if (isEnd)
            outStream << "};";
        outStream << endl;
    }
}

void processFile(const string &filename) {
    uint16_t tab[128];
    ifstream infile(filename);
    readMBtable(infile, 256, tab, nullptr);
}

//1 or 2 bytes for code
void processFile12(const string &filename) {
    uint16_t tab[128];
    uint16_t* dbcsroot[128];
    ifstream infile(filename);
    readMBtable(infile, 256, tab, dbcsroot);
    readDBCStables(infile, dbcsroot);
}

void processBest(const filesystem::path &path, ofstream &outStream) {
    string name = "cp" + path.stem().string().substr(7);
    uint16_t tab[128];
    ifstream infile(path);
    int mbSize = 0;
    for (string line; getline(infile, line);) {
        line = trimRight(line);
        if (line.empty()) continue;
        if (line.substr(0, 7) == "MBTABLE") {
            mbSize = stoi(line.substr(8));
            break;
        }
    }
    string line1;
    getline(infile, line1);
    line1 = trimRight(line1);
    if (!line1.empty())
        throw runtime_error("no empty line");
    readMBtable(infile, mbSize, tab, nullptr);
    int wcSize = 0;
    for (string line; getline(infile, line);) {
        line = trimRight(line);
        if (line.empty()) continue;
        if (line.substr(0, 7) == "WCTABLE") {
            wcSize = stoi(line.substr(8));
            break;
        }
    }
    vector<pair<uint16_t, uint8_t>> bestv;
    getline(infile, line1);
    line1 = trimRight(line1);
    if (!line1.empty())
        throw runtime_error("no empty line");
    for (int i = 0; i < wcSize; i++) {
        string line;
        getline(infile, line);
        line = trimRight(line);
        int a, b;
        readTwo(line, b, a);
        if (a < 128) {
            if (a != b)
                bestv.emplace_back(make_pair(b, a));
        } else {
            if (a > 255)
                throw runtime_error("ascii code mismatch");
            if (tab[a - 128] != b)
                bestv.emplace_back(make_pair(b, a));
        }
    }
    printTab(tab, outStream, name);
    printBestTab(bestv, outStream, "b" + name);
}

void printDBCStables(uint16_t *dbcsroot[], ofstream &outStream, string name) {
    for (int i = 0; i < 128; i++)
        if (dbcsroot[i])
            printTab(dbcsroot[i], outStream, name + "_" + n2hexstr(i, 2));
}

void processBest12(const filesystem::path &path, ofstream &outStream) {
    string name = "cp" + path.stem().string().substr(7);
    uint16_t tab[128];
    uint16_t *dbcsroot[128];
    ifstream infile(path);
    int mbSize = 0;
    for (string line; getline(infile, line);) {
        line = trimRight(line);
        if (line.empty()) continue;
        if (line.substr(0, 7) == "MBTABLE") {
            mbSize = stoi(line.substr(8));
            break;
        }
    }
    string line1;
    getline(infile, line1);
    line1 = trimRight(line1);
    if (!line1.empty())
        throw runtime_error("no empty line");
    readMBtable(infile, mbSize, tab, dbcsroot);
    string line;
    for (; getline(infile, line);) {
        line = trimRight(line);
        if (!line.empty()) break;
    }
    int rangeCount;
    if (line.find("DBCSRANGE  ") == 0)
        rangeCount = stoi(line.substr(11));
    else if (line.find("DBCSRANGE\t") == 0)
        rangeCount = stoi(line.substr(10));
    else
        throw runtime_error("no DBCSRANGE");

    for (int range = 0; range < rangeCount; range++) {
        getline(infile, line);
        getline(infile, line);
        int rangeFrom, rangeTo;
        readTwo(line, rangeFrom, rangeTo);
        for (int n = rangeFrom; n <= rangeTo; n++)
            readDBCStable(infile, dbcsroot, n);
        getline(infile, line);
    }
    int wcSize = 0;
    for (string line; getline(infile, line);) {
        line = trimRight(line);
        if (line.empty()) continue;
        if (line.substr(0, 7) == "WCTABLE") {
            wcSize = stoi(line.substr(8));
            break;
        }
    }
    vector<pair<uint16_t, uint8_t>> bestv;
    getline(infile, line1);
    line1 = trimRight(line1);
    if (!line1.empty())
        throw runtime_error("no empty line");
    for (int i = 0; i < wcSize; i++) {
        string line;
        getline(infile, line);
        line = trimRight(line);
        int a, b;
        readTwo(line, b, a);
        if (a < 128) {
            if (a != b)
                bestv.emplace_back(make_pair(b, a));
        } else {
            if (a > 255) {
                bestv.emplace_back(make_pair(b, a));
            }
            else if (tab[a - 128] != b)
                bestv.emplace_back(make_pair(b, a));
        }
    }
    printTab(tab, outStream, name);
    printDBCStables(dbcsroot, outStream, name);
    printBestTab(bestv, outStream, "b" + name);
}

void searchDirectories(const fs::path &directory) {
    for (const auto &entry: fs::recursive_directory_iterator(directory)) {
        if (entry.is_directory()) {
            std::cout << entry.path() << std::endl;
        }
    }
}

void ebcdic(bool hi, const filesystem::path &path, ofstream &outStream) {
    uint16_t tab[128];
    ifstream infile(path);
    for (string line; getline(infile, line);) {
        line = trimRight(line);
        if (line.empty()) continue;
        if (line[0] == 26) break;
        if (line[0] == '#') continue;
        int a, b;
        readTwo(line, a, b);
        if (hi) {
            if (a >= 128)
                tab[a - 128] = b;
        } else {
            if (a < 128)
                tab[a] = b;
        }
    }
    string name;
    if (hi)
        name = path.stem();
    else
        name = "ecbdic";
    printTab(tab, outStream, name);
}

void ebcdicDir() {
    fs::path dir = "../www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/EBCDIC";
    ofstream outfile("ecbdic.h");
    ebcdic(false, dir / "CP500.TXT", outfile);
    for (const auto &entry: fs::directory_iterator(dir)) {
        if (!entry.is_directory()) {
            string ext = str_tolower(entry.path().extension().string());
            if (ext != ".txt") continue;
            ebcdic(true, entry.path(), outfile);
        }
    }
}

void bestFitDir() {
    fs::path dir = "../www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WindowsBestFit";
    ofstream outfile("bestFit.h");
    for (const auto &entry: fs::directory_iterator(dir)) {
        if (!entry.is_directory()) {
            string ext = str_tolower(entry.path().extension().string());
            if (ext != ".txt") continue;
            if (entry.path().stem().string().find("bestfit")!=0)
                continue;
            if (entry.file_size() < 50000)
                processBest(entry.path(), outfile);
            else
                processBest12(entry.path(), outfile);
        }
    }
}

int main() {
    bestFitDir();
    return 0;
}
