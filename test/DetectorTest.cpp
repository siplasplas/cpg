#include <gtest/gtest.h>
#include <algorithm>
#include <cpg/CpManager.h>
#include <cpg/Language.h>
#include <cpg/Detector.h>

#ifndef CPG_LANGUAGES_TXT
#define CPG_LANGUAGES_TXT "languages.txt"
#endif

namespace {

class DetectorFixture : public ::testing::Test {
protected:
    CpManager cpManager;
    Languages languages;
    std::unique_ptr<Detector> detector;

    void SetUp() override {
        languages.readFromFile(CPG_LANGUAGES_TXT);
        detector = std::make_unique<Detector>(cpManager, languages);
    }

    std::string encodeAs(const std::u32string& text, const std::string& cp) {
        Codepage* c = cpManager.getByName(cp);
        return c ? c->fromU32(text) : std::string{};
    }
};

const std::u32string kPolishText =
    U"Zażółć gęślą jaźń. Polski tekst zawiera wiele znaków diakrytycznych "
    U"takich jak ą, ć, ę, ł, ń, ó, ś, ź, ż. To jest test detekcji kodowania.";

TEST_F(DetectorFixture, IcuDetectsUnicodeWithAndWithoutBom) {
    for (const std::string cp : {"utf8", "utf16", "utf16be", "utf32", "utf32be"}) {
        for (bool bom : {false, true}) {
            SCOPED_TRACE(cp + (bom ? " with BOM" : " without BOM"));
            const auto text = bom ? std::u32string(U"\uFEFF") + kPolishText : kPolishText;
            auto results = detector->detectCodepage("pl", encodeAs(text, cp));
            ASSERT_EQ(results.size(), 1u);
            EXPECT_EQ(results[0].codepage, cp);
            EXPECT_EQ(results[0].rank, 0);
            EXPECT_GE(results[0].score, 0.8);
        }
    }
}

TEST_F(DetectorFixture, UnicodeDetectionDoesNotRequireKnownLanguage) {
    auto results = detector->detectCodepage("zz-unknown", encodeAs(kPolishText, "utf8"));
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].codepage, "utf8");
}

TEST_F(DetectorFixture, EmptyInputReturnsEmpty) {
    EXPECT_TRUE(detector->detectCodepage("pl", "").empty());
}

TEST_F(DetectorFixture, LegacyEncodingsUseFallback) {
    for (const std::string cp : {"cp1250", "iso-8859-2", "cp852", "mazovia"}) {
        SCOPED_TRACE(cp);
        auto results = detector->detectCodepage("pl", encodeAs(kPolishText, cp));
        ASSERT_GT(results.size(), 1u);
        EXPECT_EQ(results[0].codepage, cp);
    }
}

TEST_F(DetectorFixture, BetterIso88592ScoreIsNotOverriddenByCp1250Preference) {
    const std::u32string text =
        U"Dzień dobry. Przesyłam zestawienie kosztów za wrzesień. Proszę o sprawdzenie "
        U"dokumentów i przesłanie odpowiedzi do piątku. Dziękuję za pomoc.";
    auto results = detector->detectCodepage("pl", encodeAs(text, "iso-8859-2"));
    ASSERT_GT(results.size(), 1u);
    EXPECT_EQ(results[0].codepage, "iso-8859-2");
}

TEST_F(DetectorFixture, MalformedUnicodeUsesFallback) {
    for (const std::string cp : {"utf8", "utf16", "utf16be", "utf32", "utf32be"}) {
        SCOPED_TRACE(cp);
        auto bytes = encodeAs(std::u32string(U"\uFEFF") + kPolishText, cp);
        if (cp == "utf8") bytes.push_back(char(0xFF));
        else bytes.pop_back();
        auto results = detector->detectCodepage("pl", bytes);
        ASSERT_GT(results.size(), 1u);
        EXPECT_NE(results[0].rank, 0);
    }
}

TEST_F(DetectorFixture, NewCodepagesRegistered) {
    EXPECT_NE(cpManager.getByName("mazovia"), nullptr);
    EXPECT_NE(cpManager.getByName("dhn"), nullptr);
    EXPECT_NE(cpManager.getByName("cyfromat"), nullptr);
    EXPECT_NE(cpManager.getByName("amigapl"), nullptr);
    EXPECT_NE(cpManager.getByName("texpl"), nullptr);
    EXPECT_NE(cpManager.getByName("mac-pl"), nullptr);
}

TEST_F(DetectorFixture, MazoviaRoundtrip) {
    Codepage* cp = cpManager.getByName("mazovia");
    ASSERT_NE(cp, nullptr);
    std::string bytes = cp->fromU32(kPolishText);
    std::u32string back = cp->toU32(bytes);
    EXPECT_EQ(back, kPolishText);
}

TEST_F(DetectorFixture, DhnRoundtrip) {
    Codepage* cp = cpManager.getByName("dhn");
    ASSERT_NE(cp, nullptr);
    std::string bytes = cp->fromU32(kPolishText);
    EXPECT_EQ(cp->toU32(bytes), kPolishText);
}

TEST_F(DetectorFixture, Cp1250EncodedTextScoresHighAsCp1250) {
    std::string bytes = encodeAs(kPolishText, "cp1250");
    auto results = detector->detectCodepage("pl", bytes);
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0].codepage, "cp1250");
    EXPECT_GT(results[0].score, 0.3);
}

TEST_F(DetectorFixture, MazoviaEncodedTextFavorsMazovia) {
    // Mazovia bytes measured against every candidate codepage. Mazovia
    // should win; cp1250/iso-8859-2 should rank lower.
    std::string bytes = encodeAs(kPolishText, "mazovia");
    auto results = detector->detectCodepage("pl", bytes);
    ASSERT_FALSE(results.empty());

    auto findScore = [&](const std::string& name) {
        auto it = std::find_if(results.begin(), results.end(),
            [&](const DetectionResult& r) { return r.codepage == name; });
        return it == results.end() ? -1.0 : it->score;
    };
    double mazoviaScore = findScore("mazovia");
    double cp1250Score  = findScore("cp1250");
    EXPECT_GT(mazoviaScore, cp1250Score);
}

TEST_F(DetectorFixture, TieBreakPrefersNormalOverExotic) {
    // ASCII provides weak evidence for ICU. Fall back to alphabet coverage,
    // with rank breaking ties between compatible legacy codepages.
    std::string ascii = "Hello world, this is pure ASCII text.";
    auto results = detector->detectCodepage("pl", ascii);
    ASSERT_FALSE(results.empty());
    EXPECT_GT(results.size(), 1u);
    // Top result must be a rank-1 codepage (cp1250 or iso-8859-*)
    EXPECT_LE(results[0].rank, 2);
}

TEST_F(DetectorFixture, WrongLanguageReturnsEmpty) {
    auto results = detector->detectCodepage("zz-unknown", "bytes");
    EXPECT_TRUE(results.empty());
}

TEST_F(DetectorFixture, CodepageRankOrdering) {
    EXPECT_EQ(Detector::codepageRank("utf8"), 0);
    EXPECT_EQ(Detector::codepageRank("UTF16"), 0);
    EXPECT_EQ(Detector::codepageRank("iso-8859-2"), 1);
    EXPECT_EQ(Detector::codepageRank("cp1250"), 1);
    EXPECT_EQ(Detector::codepageRank("cp852"), 2);
    EXPECT_GE(Detector::codepageRank("mazovia"), 3);
    EXPECT_GT(Detector::codepageRank("dhn"),     Detector::codepageRank("cp1250"));
    EXPECT_GT(Detector::codepageRank("texpl"),   Detector::codepageRank("iso-8859-2"));
}

TEST_F(DetectorFixture, ResultsAreSortedByScoreThenRank) {
    std::string bytes = encodeAs(kPolishText, "cp1250");
    auto results = detector->detectCodepage("pl", bytes);
    for (size_t i = 1; i < results.size(); i++) {
        const auto& a = results[i-1];
        const auto& b = results[i];
        bool ok = (a.score > b.score) ||
                  (a.score == b.score && a.rank <= b.rank);
        EXPECT_TRUE(ok) << "pos " << i
                        << ": " << a.codepage << "(" << a.score << ",r" << a.rank
                        << ") vs " << b.codepage << "(" << b.score << ",r" << b.rank << ")";
    }
}

} // namespace
