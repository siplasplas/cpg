#include <gtest/gtest.h>
#include <cpg/UtfCodepage.h>

namespace {

std::u32string kPolish = U"Zażółć gęślą jaźń";
std::u32string kEmoji = U"Hello \U0001F600 world";

std::string u32ToBytes(const std::u32string& s, bool bigEndian) {
    std::string out;
    out.resize(s.size() * 4);
    for (size_t i = 0; i < s.size(); i++) {
        char32_t c = s[i];
        if (bigEndian) {
            out[4*i+0] = (char)((c >> 24) & 0xFF);
            out[4*i+1] = (char)((c >> 16) & 0xFF);
            out[4*i+2] = (char)((c >>  8) & 0xFF);
            out[4*i+3] = (char)( c        & 0xFF);
        } else {
            out[4*i+0] = (char)( c        & 0xFF);
            out[4*i+1] = (char)((c >>  8) & 0xFF);
            out[4*i+2] = (char)((c >> 16) & 0xFF);
            out[4*i+3] = (char)((c >> 24) & 0xFF);
        }
    }
    return out;
}

TEST(UtfCodepage, Utf8Roundtrip) {
    UtfCodepage cp("utf8", 1, false);
    std::string utf8 = "Zażółć gęślą jaźń";
    std::u32string u32 = cp.toU32(utf8);
    EXPECT_EQ(u32, kPolish);
    EXPECT_EQ(cp.fromU32(u32), utf8);
}

TEST(UtfCodepage, Utf8EmojiRoundtrip) {
    UtfCodepage cp("utf8", 1, false);
    std::string utf8 = u8"Hello \U0001F600 world";
    std::u32string u32 = cp.toU32(utf8);
    EXPECT_EQ(u32, kEmoji);
    EXPECT_EQ(cp.fromU32(u32), utf8);
}

TEST(UtfCodepage, Utf16LeRoundtrip) {
    UtfCodepage cp("utf16", 2, false);
    std::string bytes = cp.fromU32(kPolish);
    EXPECT_EQ(cp.toU32(bytes), kPolish);
}

TEST(UtfCodepage, Utf16BeRoundtrip) {
    UtfCodepage cp("utf16be", 2, true);
    std::string bytes = cp.fromU32(kPolish);
    EXPECT_EQ(cp.toU32(bytes), kPolish);
}

TEST(UtfCodepage, Utf16SurrogatePair) {
    UtfCodepage cp("utf16", 2, false);
    std::string bytes = cp.fromU32(kEmoji);
    // emoji requires a surrogate pair → 4 bytes for U+1F600
    EXPECT_EQ(bytes.size(), kEmoji.size() * 2 + 2);
    EXPECT_EQ(cp.toU32(bytes), kEmoji);
}

TEST(UtfCodepage, Utf32LeRoundtrip) {
    UtfCodepage cp("utf32", 4, false);
    std::string bytes = cp.fromU32(kPolish);
    EXPECT_EQ(bytes, u32ToBytes(kPolish, false));
    EXPECT_EQ(cp.toU32(bytes), kPolish);
}

TEST(UtfCodepage, Utf32BeRoundtrip) {
    UtfCodepage cp("utf32be", 4, true);
    std::string bytes = cp.fromU32(kPolish);
    EXPECT_EQ(bytes, u32ToBytes(kPolish, true));
    EXPECT_EQ(cp.toU32(bytes), kPolish);
}

TEST(UtfCodepage, EmptyInput) {
    UtfCodepage cp("utf8", 1, false);
    EXPECT_EQ(cp.toU32(""), U"");
    EXPECT_EQ(cp.fromU32(U""), "");
}

TEST(UtfCodepage, CharLengths) {
    UtfCodepage cp8("utf8", 1, false);
    EXPECT_EQ(cp8.minCharLen(), 1);
    EXPECT_EQ(cp8.maxCharLen(), 4);
    UtfCodepage cp16("utf16", 2, false);
    EXPECT_EQ(cp16.minCharLen(), 2);
    UtfCodepage cp32("utf32", 4, false);
    EXPECT_EQ(cp32.minCharLen(), 4);
}

} // namespace
