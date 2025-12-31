#include <iostream>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <cpg/CpManager.h>
#include <cpg/Language.h>

using namespace std;

void printHelp() {
    cout << "Usage: convcp [options] [input_file output_file]\n\n";
    cout << "Convert files between codepages:\n";
    cout << "  convcp -f <from> -t <to> input.txt output.txt\n\n";
    cout << "Options:\n";
    cout << "  -f <codepage>   Source codepage (e.g., utf8, cp1250)\n";
    cout << "  -t <codepage>   Target codepage (e.g., cp852, iso-8859-2)\n";
    cout << "  --list          List all available codepages\n";
    cout << "  --languages     List all supported languages\n";
    cout << "  --lang <iso>    Show codepages for a language (e.g., pl, ru)\n";
    cout << "  --help          Show this help\n";
}

void listCodepages(CpManager& cpManager) {
    auto codepages = cpManager.listAll();
    cout << "Available codepages (" << codepages.size() << "):\n";
    int col = 0;
    for (const auto& name : codepages) {
        cout << "  " << name;
        if (name.size() < 16) cout << string(16 - name.size(), ' ');
        if (++col % 4 == 0) cout << "\n";
    }
    if (col % 4 != 0) cout << "\n";
}

void listLanguages(Languages& languages) {
    auto langs = languages.listAll();
    cout << "Supported languages (" << langs.size() << "):\n";
    for (const auto& [iso, name] : langs) {
        cout << "  " << iso;
        if (iso.size() < 4) cout << string(4 - iso.size(), ' ');
        cout << " - " << name << "\n";
    }
}

void showLanguageCodepages(Languages& languages, const string& iso) {
    auto charsets = languages.getCharsetsForLanguage(iso);
    if (charsets.empty()) {
        cerr << "Unknown language: " << iso << endl;
        return;
    }
    const Language* lang = languages.getByIsoCode(iso);
    cout << "Codepages for " << lang->name << " (" << iso << "):\n";
    for (const auto& charset : charsets) {
        cout << "  " << charset << "\n";
    }
}

bool convert(const string& fromCp, const string& toCp,
             const string& inFile, const string& outFile,
             CpManager& cpManager) {
    // Find codepages
    Codepage* from = cpManager.getByName(fromCp);
    Codepage* to = cpManager.getByName(toCp);

    if (!from) {
        cerr << "Error: Unknown source codepage: " << fromCp << endl;
        return false;
    }
    if (!to) {
        cerr << "Error: Unknown target codepage: " << toCp << endl;
        return false;
    }

    // Read input file
    ifstream inStream(inFile, ios::binary);
    if (!inStream) {
        cerr << "Error: Cannot open input file: " << inFile << endl;
        return false;
    }
    string content((istreambuf_iterator<char>(inStream)),
                   istreambuf_iterator<char>());
    inStream.close();

    // Convert: source -> UTF-32 -> target
    u32string u32 = from->toU32(content);
    string result = to->fromU32(u32);

    // Write output file
    ofstream outStream(outFile, ios::binary);
    if (!outStream) {
        cerr << "Error: Cannot open output file: " << outFile << endl;
        return false;
    }
    outStream.write(result.data(), result.size());
    outStream.close();

    cout << "Converted " << content.size() << " bytes from " << fromCp
         << " to " << toCp << " (" << result.size() << " bytes)" << endl;
    return true;
}

int main(int argc, char *argv[]) {
    CpManager cpManager;
    Languages languages;

    // Try to load languages from common locations
    const char* langPaths[] = {
        "languages.txt",
        "../languages.txt",
        "/home/andrzej/wazne/gitmy/cpg/languages.txt"
    };
    for (const char* path : langPaths) {
        if (filesystem::exists(path)) {
            languages.readFromFile(path);
            break;
        }
    }

    if (argc < 2) {
        printHelp();
        return 1;
    }

    string fromCp, toCp, inFile, outFile, langIso;

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            printHelp();
            return 0;
        }
        else if (arg == "--list") {
            listCodepages(cpManager);
            return 0;
        }
        else if (arg == "--languages") {
            listLanguages(languages);
            return 0;
        }
        else if (arg == "--lang" && i + 1 < argc) {
            langIso = argv[++i];
            showLanguageCodepages(languages, langIso);
            return 0;
        }
        else if (arg == "-f" && i + 1 < argc) {
            fromCp = argv[++i];
        }
        else if (arg == "-t" && i + 1 < argc) {
            toCp = argv[++i];
        }
        else if (arg[0] != '-') {
            if (inFile.empty()) inFile = arg;
            else if (outFile.empty()) outFile = arg;
        }
    }

    // Validate for conversion
    if (fromCp.empty() || toCp.empty() || inFile.empty() || outFile.empty()) {
        cerr << "Error: Missing required arguments\n";
        cerr << "Usage: convcp -f <from> -t <to> input.txt output.txt\n";
        return 1;
    }

    if (!convert(fromCp, toCp, inFile, outFile, cpManager)) {
        return 1;
    }

    return 0;
}
