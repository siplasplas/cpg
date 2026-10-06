#ifndef CPG_DETECTOR_H
#define CPG_DETECTOR_H

#include <string>
#include <string_view>
#include <vector>
#include <cpg/CpManager.h>
#include <cpg/Language.h>

struct DetectionResult {
    std::string codepage;
    double score;    // 0..1: ICU confidence for Unicode; alphabet coverage otherwise
    int rank;        // lower = more "normal" (Windows/ISO/UTF)
};

class Detector {
    CpManager& cpManager;
    Languages& languages;
public:
    Detector(CpManager& cpm, Languages& langs);

    // First accept ICU's best match if it is valid UTF-8/16/32 with confidence
    // >= 80 (language-independent). Otherwise rank the language's codepages
    // by alphabet coverage. Empty input returns no candidates.
    // Fallback results are sorted by score desc; ties broken by rank asc.
    std::vector<DetectionResult> detectCodepage(const std::string& iso,
                                                std::string_view bytes);

    // Exposed for testing / external use
    static int codepageRank(const std::string& name);
};

#endif //CPG_DETECTOR_H
