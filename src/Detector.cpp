#include <cpg/Detector.h>

#include <algorithm>
#include <cctype>
#include <limits>
#include <memory>
#include <optional>
#include <unordered_set>
#include <unicode/ucnv.h>
#include <unicode/ucnv_err.h>
#include <unicode/ucsdet.h>

namespace {

std::optional<DetectionResult> detectUnicode(std::string_view bytes) {
    if (bytes.empty() || bytes.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max()))
        return std::nullopt;

    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<UCharsetDetector, decltype(&ucsdet_close)> detector(
        ucsdet_open(&status), &ucsdet_close);
    if (U_FAILURE(status) || !detector) return std::nullopt;
    ucsdet_setText(detector.get(), bytes.data(), static_cast<int32_t>(bytes.size()), &status);
    const UCharsetMatch* match = ucsdet_detect(detector.get(), &status);
    if (U_FAILURE(status) || !match) return std::nullopt;

    const char* name = ucsdet_getName(match, &status);
    const int confidence = ucsdet_getConfidence(match, &status);
    // Weak guesses (including plain ASCII) should reach the codepage fallback.
    constexpr int minimumConfidence = 80;
    if (U_FAILURE(status) || !name || confidence < minimumConfidence)
        return std::nullopt;

    std::string canonical;
    const std::string icuName(name);
    if (icuName == "UTF-8") canonical = "utf8";
    else if (icuName == "UTF-16LE") canonical = "utf16";
    else if (icuName == "UTF-16BE") canonical = "utf16be";
    else if (icuName == "UTF-32LE") canonical = "utf32";
    else if (icuName == "UTF-32BE") canonical = "utf32be";
    else return std::nullopt;

    // Detection is heuristic; reject truncated or malformed Unicode even when
    // ICU gives it a high confidence. Preflight conversion validates all bytes.
    std::unique_ptr<UConverter, decltype(&ucnv_close)> converter(
        ucnv_open(name, &status), &ucnv_close);
    if (U_FAILURE(status) || !converter) return std::nullopt;
    ucnv_setToUCallBack(converter.get(), UCNV_TO_U_CALLBACK_STOP, nullptr,
                      nullptr, nullptr, &status);
    if (U_FAILURE(status)) return std::nullopt;
    ucnv_toUChars(converter.get(), nullptr, 0, bytes.data(),
                 static_cast<int32_t>(bytes.size()), &status);
    if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR)
        return std::nullopt;

    return DetectionResult{canonical, confidence / 100.0, 0};
}

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

    if (bytes.empty()) return results;
    if (auto unicode = detectUnicode(bytes)) return {*unicode};

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
                  // Exact ordering keeps the comparator transitive.
                  if (a.score != b.score)
                      return a.score > b.score;
                  // tie-break: rank asc (prefer Windows/ISO over exotics)
                  if (a.rank != b.rank) return a.rank < b.rank;
                  return a.codepage < b.codepage;
              });
    return results;
}
