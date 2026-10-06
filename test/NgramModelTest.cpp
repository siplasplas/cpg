#include <cpg/Detector.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <fstream>

namespace {
class NgramFixture : public ::testing::Test {
protected:
    NgramModel model;
    std::filesystem::path path;
    void SetUp() override {
        path = std::filesystem::temp_directory_path() /
            ("cpg-model-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        for (int i = 0; i < 50; ++i)
            model.add("pl", U"Dzień dobry. Proszę o przesłanie dokumentów do piątku. Dziękuję za pomoc.");
    }
    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
};

TEST_F(NgramFixture, ContextDistinguishesWordsFromShuffledLetters) {
    EXPECT_GT(model.score("pl", U"Proszę o przesłanie dokumentów"),
              model.score("pl", U"ęszorP o einałsezrp wótnemukod"));
    EXPECT_EQ(model.score("unknown", U"tekst"), 0);
    EXPECT_EQ(model.score("pl", U""), 0);
}

TEST_F(NgramFixture, CaseAndPunctuationDoNotChangeModelScore) {
    EXPECT_DOUBLE_EQ(model.score("pl", U"Dzień dobry"), model.score("pl", U"DZIEŃ DOBRY"));
    EXPECT_DOUBLE_EQ(model.score("pl", U"Dzień, dobry"), model.score("pl", U"Dzień dobry"));
    EXPECT_GT(model.score("pl", U"Dzień dobry"), model.score("pl", U"Dzien dobry"));
}

TEST_F(NgramFixture, SaveLoadPreservesScoresAndIsDeterministic) {
    model.save(path);
    NgramModel loaded;
    loaded.load(path);
    EXPECT_TRUE(loaded.contains("pl"));
    EXPECT_DOUBLE_EQ(model.score("pl", U"Dzień dobry"), loaded.score("pl", U"Dzień dobry"));
    std::ifstream first(path);
    std::string original((std::istreambuf_iterator<char>(first)), {});
    loaded.save(path);
    std::ifstream second(path);
    EXPECT_EQ(original, std::string((std::istreambuf_iterator<char>(second)), {}));
}

TEST_F(NgramFixture, InvalidModelDoesNotReplaceExistingProfiles) {
    std::ofstream out(path);
    out << "CPG_NGRAM 1\nlanguage bad 10\norder 1 1\n97 9\n";
    out.close();
    EXPECT_THROW(model.load(path), std::runtime_error);
    EXPECT_TRUE(model.contains("pl"));
    EXPECT_FALSE(model.contains("bad"));
    EXPECT_THROW(model.load(path.string() + ".missing"), std::runtime_error);
}

TEST_F(NgramFixture, DetectorUsesModelAfterUnicodeAndRejectsDroppedBytes) {
    CpManager manager;
    Languages languages;
    languages.readFromFile(CPG_LANGUAGES_TXT);
    Detector detector(manager, languages, &model);
    auto text = U"Dzień dobry. Proszę o przesłanie dokumentów do piątku.";
    auto cp1250 = manager.getByName("cp1250");
    auto results = detector.detectCodepage("pl", cp1250->fromU32(text));
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results.front().codepage, "cp1250");
    EXPECT_DOUBLE_EQ(results.front().score, model.score("pl", text));
    auto unicode = detector.detectCodepage("pl", manager.getByName("utf8")->fromU32(text));
    ASSERT_EQ(unicode.size(), 1u);
    EXPECT_EQ(unicode.front().codepage, "utf8");
    auto bytes = cp1250->fromU32(text) + char(0x81);
    auto damaged = detector.detectCodepage("pl", bytes);
    auto it = std::find_if(damaged.begin(), damaged.end(), [](const auto& r) { return r.codepage == "cp1250"; });
    ASSERT_NE(it, damaged.end());
    EXPECT_EQ(it->score, 0);
}
}
