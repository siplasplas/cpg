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
    // ASCII-only text: all codepages score equally on alphabet coverage
    // (no Polish chars hit, only ASCII which is "common"). Rank tie-break
    // must favor cp1250/ISO over Mazovia/DHN.
    std::string ascii = "Hello world, this is pure ASCII text.";
    auto results = detector->detectCodepage("pl", ascii);
    ASSERT_FALSE(results.empty());
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
        // either score strictly greater, or (close scores AND rank <=)
        bool ok = (a.score > b.score + 0.02) ||
                  (std::abs(a.score - b.score) <= 0.02 && a.rank <= b.rank);
        EXPECT_TRUE(ok) << "pos " << i
                        << ": " << a.codepage << "(" << a.score << ",r" << a.rank
                        << ") vs " << b.codepage << "(" << b.score << ",r" << b.rank << ")";
    }
}

} // namespace
