#include <gtest/gtest.h>
#include <algorithm>
#include <cpg/Language.h>

#ifndef CPG_LANGUAGES_TXT
#define CPG_LANGUAGES_TXT "languages.txt"
#endif

namespace {

class LanguagesFixture : public ::testing::Test {
protected:
    Languages languages;
    void SetUp() override {
        languages.readFromFile(CPG_LANGUAGES_TXT);
    }
};

TEST_F(LanguagesFixture, LoadsSomeLanguages) {
    EXPECT_GT(languages.size(), 10);
}

TEST_F(LanguagesFixture, FindByIsoCode) {
    const Language* pl = languages.getByIsoCode("cz");
    ASSERT_NE(pl, nullptr);
    EXPECT_EQ(pl->name, "Czech");
}

TEST_F(LanguagesFixture, CharsetsForLanguage) {
    auto charsets = languages.getCharsetsForLanguage("cz");
    ASSERT_FALSE(charsets.empty());
    EXPECT_NE(std::find(charsets.begin(), charsets.end(), "WINDOWS-1250"),
              charsets.end());
}

TEST_F(LanguagesFixture, AlphabetContainsExpectedChars) {
    const Language* cz = languages.getByIsoCode("cz");
    ASSERT_NE(cz, nullptr);
    // Czech alphabet contains č (U+010D) and ř (U+0159)
    EXPECT_NE(cz->alphabet.find(u'č'), std::u16string::npos);
    EXPECT_NE(cz->alphabet.find(u'ř'), std::u16string::npos);
}

TEST_F(LanguagesFixture, UnknownIsoReturnsNull) {
    EXPECT_EQ(languages.getByIsoCode("xx"), nullptr);
    EXPECT_TRUE(languages.getCharsetsForLanguage("xx").empty());
}

} // namespace
