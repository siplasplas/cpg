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
    // Probability that this candidate gives the right text: shared by all
    // candidates that decode the input identically (e.g. cp1250 and
    // iso-8859-2 without bytes that differ). 0 without a model.
    double confidence = 0;
    double languageConfidence = 0; // probability of `language`; 0 without a model
    int decoding = -1; // results with the same value decode the input identically
};

// A piece of the input shown in every proposed encoding, for a person to choose.
struct PreviewLine {
    size_t byteOffset = 0;             // start of the piece in the input
    std::vector<std::string> variants; // the piece in UTF-8, one per option, in option order
};

class Detector {
    CpManager& cpManager;
    Languages& languages;
    const NgramModel* model;
    double evidenceDivisor = 8;
    double utf8Evidence = 2;
    double casePenalty = 2;
    double rarityPenalty = 0.5;
    // Language + encoding detection among model languages; empty = all.
    std::vector<DetectionResult> detectAuto(const std::vector<std::string>& isos,
                                            std::string_view bytes);
public:
    Detector(CpManager& cpm, Languages& langs, const NgramModel* model = nullptr);

    // First accept ICU's best match if it is valid UTF-8/16/32 with confidence
    // >= 80 (language-independent); with a model, UTF-8 without a BOM
    // competes with the legacy candidates instead. Otherwise rank the
    // language's codepages by model likelihood when available, otherwise
    // alphabet coverage. The optional model must outlive the detector.
    // Empty input returns no candidates. Results are sorted by confidence,
    // then score desc; ties broken by rank asc.
    std::vector<DetectionResult> detectCodepage(const std::string& iso,
                                                std::string_view bytes);
    // Automatic language + encoding detection using all loaded profiles.
    // Unicode still goes through ICU first. Legacy auto-detection needs a model.
    std::vector<DetectionResult> detectCodepage(std::string_view bytes);
    // Automatic detection restricted to the given languages (e.g. {"pl", "cz"}).
    // Empty or {"auto"} = all languages; one language = detectCodepage(iso, bytes).
    // Several languages need a model; languages without a profile are ignored.
    // (A separate name: braced lists would be ambiguous with the string overload.)
    std::vector<DetectionResult> detectCodepageAmong(const std::vector<std::string>& isos,
                                                     std::string_view bytes);

    // The alternatives worth considering: the smallest set of distinct
    // decodings whose confidence adds up to `threshold`, one result for each
    // (its most likely language and preferred codepage). A single result
    // means a confident winner; several mean the caller should not decide
    // alone. Expects sorted detectCodepage() results.
    static std::vector<DetectionResult> plausible(const std::vector<DetectionResult>& results,
                                                  double threshold = 0.95);

    // Up to maxLines pieces of the input (single lines, about 80 bytes around
    // the first distinguishing byte) where the options decode differently,
    // e.g. for plausible() alternatives. Pieces with the most distinguishing
    // bytes come first, then in file order; repeated pieces appear once.
    // Bytes a codepage leaves undefined and control characters show as
    // U+FFFD; "…" marks a cut line. Only single-byte codepages and UTF-8;
    // empty for fewer than two options.
    std::vector<PreviewLine> preview(std::string_view bytes,
                                     const std::vector<DetectionResult>& options,
                                     size_t maxLines = 5) const;

    // Confidence treats the model's log likelihood ratio, divided by this
    // value, as the evidence. Neighbouring characters are not independent and
    // rare characters (foreign names, symbols) mislead the model, so the raw
    // ratio is overconfident. Default 8, from held-out corpus samples.
    void setEvidenceDivisor(double divisor) { evidenceDivisor = divisor; }
    // Valid UTF-8 without a BOM competes with the legacy decodings; each of
    // its multi-byte sequences adds this many nats (log odds) for UTF-8,
    // since such sequences rarely arise by chance in legacy text.
    void setUtf8Evidence(double nats) { utf8Evidence = nats; }
    // Nats against a decoding for every lowercase letter directly followed by
    // an uppercase one ("przenieŚc"), which the lowercase model cannot see.
    // Default 2: more right and confident winners on held-out text at no cost.
    void setCasePenalty(double nats) { casePenalty = nats; }
    // Nats against a codepage for every codepageRank level above 1
    // (Windows/ISO): a prior that exotic codepages are rarer. Default 0.5;
    // higher values make more common-codepage files confident, but also make
    // exotic ones (Amiga, SMC, ...) confidently wrong more often.
    void setRarityPenalty(double nats) { rarityPenalty = nats; }

    // Exposed for testing / external use
    static int codepageRank(const std::string& name);
    static std::string canonicalCodepageName(const std::string& name);
};

#endif //CPG_DETECTOR_H
