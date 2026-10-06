#include <cpg/Converter.h>
#include <gtest/gtest.h>

TEST(Converter, PolishCp1250IndependentBytesRoundtrip) {
    CpManager manager;
    Converter converter(manager);
    const std::string bytes("Za\xbf\xf3\xb3\xe6 g\xea\x9cl\xb9 ja\x9f\xf1", 17);
    auto decoded = converter.toUtf8("CP1250", bytes);
    ASSERT_TRUE(decoded.success);
    EXPECT_EQ(decoded.output, u8"Zażółć gęślą jaźń");
    auto encoded = converter.fromUtf8("cp1250", decoded.output);
    ASSERT_TRUE(encoded.success);
    EXPECT_EQ(encoded.output, bytes);
}

TEST(Converter, RejectIsAtomicAndReportsUtf8ByteOffsets) {
    CpManager manager;
    Converter converter(manager);
    auto result = converter.fromUtf8("cp1250", u8"ą😀中!");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, ConversionError::Unrepresentable);
    EXPECT_TRUE(result.output.empty());
    ASSERT_EQ(result.issues.size(), 2);
    EXPECT_EQ(result.issues[0].byteOffset, 2);
    EXPECT_EQ(result.issues[0].codepoint, U'😀');
    EXPECT_EQ(result.issues[1].byteOffset, 6);
    EXPECT_EQ(result.issues[1].codepoint, U'中');
}

TEST(Converter, ReplacementIsExplicitAndReportsLoss) {
    CpManager manager;
    Converter converter(manager);
    auto result = converter.fromUtf8("cp1250", u8"ą😀中!", UnmappablePolicy::Replace);
    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.error, ConversionError::None);
    EXPECT_EQ(result.output, std::string("\xb9?" "?!", 4));
    EXPECT_EQ(result.issues.size(), 2);
    // A supplementary scalar with the same low 16 bits as 'ą' must not alias it.
    auto alias = converter.fromUtf8("cp1250", u8"\U00010105");
    EXPECT_FALSE(alias.success);
}

TEST(Converter, UndefinedSourceByteDoesNotDisappear) {
    CpManager manager;
    Converter converter(manager);
    auto result = converter.toUtf8("cp1250", std::string("A\x81" "B", 3));
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, ConversionError::InvalidInput);
    EXPECT_TRUE(result.output.empty());
    ASSERT_EQ(result.issues.size(), 1);
    EXPECT_EQ(result.issues.front().byteOffset, 1);
}

TEST(Converter, RejectMalformedUtf8EvenInReplacementMode) {
    CpManager manager;
    Converter converter(manager);
    for (const auto& bytes : {std::string("A\xc0\xaf", 3),
                              std::string("A\xed\xa0\x80", 4),
                              std::string("A\xf4\x90\x80\x80", 5),
                              std::string("A\xe2\x82", 3)}) {
        auto result = converter.fromUtf8("cp1250", bytes, UnmappablePolicy::Replace);
        EXPECT_FALSE(result.success);
        EXPECT_EQ(result.error, ConversionError::InvalidInput);
        EXPECT_TRUE(result.output.empty());
        ASSERT_EQ(result.issues.size(), 1);
        EXPECT_EQ(result.issues.front().byteOffset, 1);
        EXPECT_FALSE(converter.toUtf8("utf8", bytes).success);
    }
}

TEST(Converter, UnicodeBomNulAndSupplementaryRoundtrip) {
    CpManager manager;
    Converter converter(manager);
    std::string text = u8"\ufeffą😀";
    text.push_back('\0');
    for (const auto& name : {"utf8", "utf16", "utf16be", "utf32", "utf32be"}) {
        auto encoded = converter.fromUtf8(name, text);
        ASSERT_TRUE(encoded.success) << name;
        auto decoded = converter.toUtf8(name, encoded.output);
        ASSERT_TRUE(decoded.success) << name;
        EXPECT_EQ(decoded.output, text) << name;
    }
    EXPECT_EQ(converter.fromUtf8("utf16be", u8"😀").output,
              std::string("\xd8\x3d\xde\x00", 4));
    EXPECT_EQ(converter.fromUtf8("utf32", u8"😀").output,
              std::string("\x00\xf6\x01\x00", 4));
}

TEST(Converter, InvalidUtf16AndUtf32AreRejected) {
    CpManager manager;
    Converter converter(manager);
    EXPECT_FALSE(converter.toUtf8("utf16", std::string("\x41", 1)).success);
    EXPECT_FALSE(converter.toUtf8("utf16", std::string("\x00\xd8", 2)).success);
    EXPECT_FALSE(converter.toUtf8("utf16be", std::string("\xdc\x00", 2)).success);
    EXPECT_FALSE(converter.toUtf8("utf32", std::string("\x00\x00\x11\x00", 4)).success);
    EXPECT_FALSE(converter.toUtf8("utf32", std::string("\x00\xd8\x00\x00", 4)).success);
    EXPECT_FALSE(converter.toUtf8("utf32be", std::string("\x00\x00\x00", 3)).success);
}

TEST(Converter, EmptyInputAndUnknownCodepage) {
    CpManager manager;
    Converter converter(manager);
    EXPECT_TRUE(converter.toUtf8("cp1250", {}).success);
    EXPECT_TRUE(converter.fromUtf8("utf8", {}).success);
    auto result = converter.fromUtf8("unknown", "abc");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, ConversionError::UnknownCodepage);
    EXPECT_TRUE(result.output.empty());
}
