#include <gtest/gtest.h>
#include <cpg/CpManager.h>

namespace {

TEST(CpManager, KnownCodepages) {
    CpManager mgr;
    EXPECT_NE(mgr.getByName("utf8"), nullptr);
    EXPECT_NE(mgr.getByName("cp1250"), nullptr);
    EXPECT_NE(mgr.getByName("iso-8859-2"), nullptr);
    EXPECT_EQ(mgr.getByName("utf8")->getName(), "utf8");
}

TEST(CpManager, LookupIsCaseInsensitive) {
    CpManager mgr;
    EXPECT_NE(mgr.getByName("UTF8"), nullptr);
    EXPECT_NE(mgr.getByName("CP1250"), nullptr);
    EXPECT_NE(mgr.getByName("Iso-8859-2"), nullptr);
}

TEST(CpManager, UnknownReturnsNull) {
    CpManager mgr;
    EXPECT_EQ(mgr.getByName("bogus-codepage"), nullptr);
    EXPECT_EQ(mgr.getByName(""), nullptr);
}

TEST(CpManager, ListAllHasContent) {
    CpManager mgr;
    auto all = mgr.listAll();
    EXPECT_EQ(all.size(), mgr.size());
    EXPECT_GT(all.size(), 50u);
}

TEST(CpManager, Cp1250PolishRoundtrip) {
    CpManager mgr;
    Codepage* cp = mgr.getByName("cp1250");
    ASSERT_NE(cp, nullptr);
    // Polish characters that must survive cp1250
    std::u32string text = U"Zażółć gęślą jaźń";
    std::string bytes = cp->fromU32(text);
    EXPECT_EQ(cp->toU32(bytes), text);
    // each char must be 1 byte in cp1250
    EXPECT_EQ(bytes.size(), text.size());
}

TEST(CpManager, Utf8ToCp1250ToUtf8) {
    CpManager mgr;
    Codepage* utf8 = mgr.getByName("utf8");
    Codepage* cp1250 = mgr.getByName("cp1250");
    ASSERT_NE(utf8, nullptr);
    ASSERT_NE(cp1250, nullptr);
    std::string src = "Zażółć gęślą jaźń";
    std::u32string u32 = utf8->toU32(src);
    std::string mid = cp1250->fromU32(u32);
    std::u32string back = cp1250->toU32(mid);
    std::string roundtrip = utf8->fromU32(back);
    EXPECT_EQ(roundtrip, src);
}

TEST(CpManager, UnmappableCharacterBecomesQuestionMark) {
    CpManager mgr;
    Codepage* cp = mgr.getByName("iso-8859-1");
    ASSERT_NE(cp, nullptr);
    // Polish ż (U+017C) is not in iso-8859-1
    std::string bytes = cp->fromU32(U"a\u017Cb");
    EXPECT_EQ(bytes, "a?b");
}

} // namespace
