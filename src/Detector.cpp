#include <cpg/Detector.h>

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace {

std::string toLowerAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return (char) std::tolower(c); });
    return s;
}

// Map aliases used in languages.txt to canonical names in CpManager.
std::string normalizeName(const std::string& raw) {
    std::string n = toLowerAscii(raw);
    if (n.rfind("windows-", 0) == 0) return "cp" + n.substr(8);  // WINDOWS-1250 -> cp1250
    if (n.rfind("ibm", 0) == 0) {
        std::string rest = n.substr(3);
        if (!rest.empty() && std::isdigit((unsigned char) rest[0]))
            return "cp" + rest;   // IBM866 -> cp866
    }
    if (n == "maccyrillic") return "mac-cyrillic";
    if (n == "macroman") return "mac-roman";
    if (n == "macgreek") return "mac-greek";
    if (n == "macturkish") return "mac-turkish";
    if (n == "maciceland") return "mac-iceland";
    return n;
}

// Characters that can legitimately appear in any language: ASCII printable,
// whitespace, common Unicode punctuation, digits. Do NOT count as alphabet
// hit, but also not as noise.
bool isCommon(char32_t c) {
    if (c == 0) return false;
    if (c < 0x80) {
        // ASCII printable + whitespace
        return c == '\t' || c == '\n' || c == '\r' || (c >= 0x20 && c < 0x7F);
    }
    // NBSP, soft hyphen, general punctuation block
    if (c == 0x00A0 || c == 0x00AD) return true;
    if (c >= 0x2000 && c <= 0x206F) return true;  // general punctuation
    return false;
}

bool isControlOrPrivate(char32_t c) {
    if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') return true;
    if (c >= 0x7F && c < 0xA0) return true;
    if (c >= 0xE000 && c <= 0xF8FF) return true;   // PUA
    return false;
}

std::unordered_set<char32_t> alphabetSet(const Language& lang) {
    std::unordered_set<char32_t> s;
    for (char16_t c : lang.alphabet) s.insert((char32_t) c);
    return s;
}

} // namespace

Detector::Detector(CpManager& cpm, Languages& langs)
    : cpManager(cpm), languages(langs) {}

int Detector::codepageRank(const std::string& name) {
    std::string n = toLowerAscii(name);
    if (n.rfind("utf", 0) == 0) return 0;                    // UTF = always best if valid
    if (n.rfind("iso-8859", 0) == 0) return 1;               // ISO Latin family
    if (n.rfind("cp125", 0) == 0) return 1;                  // Windows ANSI
    if (n.rfind("cp1250", 0) == 0 || n.rfind("windows-", 0) == 0) return 1;
    if (n == "cp852" || n == "cp866" || n == "cp437" ||
        n == "cp850" || n == "cp855" || n == "cp862") return 2;   // common DOS
    if (n.rfind("cp", 0) == 0) return 3;                     // other DOS
    if (n.rfind("mac-", 0) == 0 || n.rfind("apple-", 0) == 0) return 3;
    if (n == "mazovia" || n == "fido-mazovia") return 4;     // well-known PL DOS
    // Everything else (DHN, Cyfromat, IINTE-ISIS, SMC, TeXPL, Amiga*, ...)
    return 5;
}

std::vector<DetectionResult>
Detector::detectCodepage(const std::string& iso, std::string_view bytes) {
    std::vector<DetectionResult> results;

    const Language* lang = languages.getByIsoCode(iso);
    if (!lang) return results;

    auto alpha = alphabetSet(*lang);
    auto charsets = languages.getCharsetsForLanguage(iso);

    for (const auto& charsetName : charsets) {
        std::string canonical = normalizeName(charsetName);
        Codepage* cp = cpManager.getByName(canonical);
        if (!cp) continue;

        std::u32string u32 = cp->toU32(bytes);

        int64_t hits = 0, misses = 0, noise = 0;
        for (char32_t c : u32) {
            if (alpha.count(c)) hits++;
            else if (isCommon(c)) { /* neutral */ }
            else if (isControlOrPrivate(c)) noise++;
            else misses++;
        }

        double denom = (double)(hits + misses + noise);
        double score = denom > 0 ? (double) hits / denom : 0.0;
        if (denom > 0) {
            double noiseRatio = (double) noise / denom;
            score *= (1.0 - noiseRatio);
        }

        results.push_back({canonical, score, codepageRank(canonical)});
    }

    std::sort(results.begin(), results.end(),
              [](const DetectionResult& a, const DetectionResult& b) {
                  // primary: score desc (close scores are clustered)
                  constexpr double EPS = 0.02;
                  if (std::abs(a.score - b.score) > EPS)
                      return a.score > b.score;
                  // tie-break: rank asc (prefer Windows/ISO over exotics)
                  if (a.rank != b.rank) return a.rank < b.rank;
                  return a.codepage < b.codepage;
              });
    return results;
}
