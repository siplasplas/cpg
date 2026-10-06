#ifndef CPG_DETECTOR_H
#define CPG_DETECTOR_H

#include <string>
#include <string_view>
#include <vector>
#include <cpg/CpManager.h>
#include <cpg/Language.h>
#include <cpg/NgramModel.h>

struct DetectionResult {
    std::string codepage;
    double score;    // 0..1: ICU confidence, model likelihood, or alphabet coverage
    int rank;        // lower = more "normal" (Windows/ISO/UTF)
    std::string language; // languages.txt key (Czech: cz); empty without language evidence
    double languageScore = 0; // model likelihood, not calibrated confidence
};

class Detector {
    CpManager& cpManager;
    Languages& languages;
    const NgramModel* model;
public:
    Detector(CpManager& cpm, Languages& langs, const NgramModel* model = nullptr);

    // First accept ICU's best match if it is valid UTF-8/16/32 with confidence
    // >= 80 (language-independent). Otherwise rank the language's codepages
    // by model likelihood when available, otherwise alphabet coverage.
    // The optional model must outlive the detector. Empty input returns no candidates.
    // Fallback results are sorted by score desc; ties broken by rank asc.
    std::vector<DetectionResult> detectCodepage(const std::string& iso,
                                                std::string_view bytes);
    // Automatic language + encoding detection using all loaded profiles.
    // Unicode still goes through ICU first. Legacy auto-detection needs a model.
    std::vector<DetectionResult> detectCodepage(std::string_view bytes);

    // Exposed for testing / external use
    static int codepageRank(const std::string& name);
    static std::string canonicalCodepageName(const std::string& name);
};

#endif //CPG_DETECTOR_H
