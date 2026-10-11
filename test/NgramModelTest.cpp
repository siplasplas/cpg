#include <cpg/Detector.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <set>

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

TEST_F(NgramFixture, PruningKeepsFrequentContextsAndBacksOffForMissingOnes) {
    NgramModel small;
    for (int i = 0; i < 100; ++i) small.add("test", U"abc");
    small.add("test", U"axy");
    small.prune(1);
    EXPECT_EQ(small.keepPercent(), 1u);
    // The common abc trigram needs ab's original count, despite a tiny budget.
    EXPECT_GT(small.score("test", U"abc"), small.score("test", U"axy"));
    EXPECT_GT(small.score("test", U"axy"), small.score("test", U"aΩΨ"));
    EXPECT_LE(small.score("test", U"abc"), 1);
    small.save(path);
    NgramModel loaded;
    loaded.load(path);
    EXPECT_EQ(loaded.keepPercent(), 1u);
    EXPECT_DOUBLE_EQ(small.score("test", U"axy"), loaded.score("test", U"axy"));
    EXPECT_THROW(loaded.prune(5), std::runtime_error);
    EXPECT_THROW(loaded.add("test", U"abc"), std::runtime_error);
}

TEST_F(NgramFixture, PruningIsDeterministicAndCompleteRetentionChangesNothing) {
    const double before = model.score("pl", U"Dzień dobry");
    model.prune(100);
    EXPECT_DOUBLE_EQ(before, model.score("pl", U"Dzień dobry"));
    EXPECT_THROW(model.prune(0), std::invalid_argument);
    EXPECT_THROW(model.prune(101), std::invalid_argument);
    NgramModel copy = model;
    model.prune(5);
    copy.prune(5);
    model.save(path);
    std::ifstream first(path);
    const std::string bytes((std::istreambuf_iterator<char>(first)), {});
    copy.save(path);
    std::ifstream second(path);
    EXPECT_EQ(bytes, std::string((std::istreambuf_iterator<char>(second)), {}));
}

TEST_F(NgramFixture, SingleLanguageModelPreservesItsScores) {
    model.add("en", U"This is an English text.");
    const double before = model.score("pl", U"Dzień dobry");
    EXPECT_THROW(model.retainLanguage("xx"), std::invalid_argument);
    model.retainLanguage("pl");
    EXPECT_TRUE(model.contains("pl"));
    EXPECT_FALSE(model.contains("en"));
    EXPECT_DOUBLE_EQ(before, model.score("pl", U"Dzień dobry"));
}

TEST_F(NgramFixture, BinaryModelPreservesEveryScoreAndRejectsCorruption) {
    model.add("en", U"This is an English text about language detection.");
    model.save(path, true);
    NgramModel loaded;
    loaded.load(path);
    EXPECT_EQ(model.languageCodes(), loaded.languageCodes());
    for (const auto& iso : model.languageCodes())
        for (const auto* text : {U"Dzień dobry", U"This is a text", U"äöüß", U"ΩΨ"})
            EXPECT_DOUBLE_EQ(model.score(iso, text), loaded.score(iso, text));
    EXPECT_EQ(loaded.keepPercent(), 100u);
    std::ifstream file(path, std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(file)), {});
    model.save(path, true);
    std::ifstream repeated(path, std::ios::binary);
    EXPECT_EQ(bytes, std::string((std::istreambuf_iterator<char>(repeated)), {}));
    bytes.back() ^= 1;
    std::ofstream damaged(path, std::ios::binary);
    damaged.write(bytes.data(), bytes.size());
    damaged.close();
    EXPECT_THROW(loaded.load(path), std::runtime_error);
    EXPECT_TRUE(loaded.contains("pl"));
}

TEST_F(NgramFixture, DirectoryLoadingMatchesCombinedModel) {
    model.add("en", U"This is an English text about language detection.");
    std::filesystem::create_directory(path);
    for (const auto& iso : model.languageCodes())
        model.forLanguage(iso).save(path / (iso + ".ngram"), true);
    NgramModel loaded;
    loaded.loadDirectory(path);
    EXPECT_EQ(model.languageCodes(), loaded.languageCodes());
    EXPECT_DOUBLE_EQ(model.score("pl", U"Dzień dobry"), loaded.score("pl", U"Dzień dobry"));
    for (const auto& iso : model.languageCodes()) std::filesystem::remove(path / (iso + ".ngram"));
}

TEST_F(NgramFixture, AutoDetectsLanguageAndEncodingForLegacyAndUnicode) {
    for (int i = 0; i < 50; ++i)
        model.add("en", U"This is an English text about language detection. Please send the document.");
    CpManager manager;
    Languages languages;
    languages.readFromFile(CPG_LANGUAGES_TXT);
    Detector detector(manager, languages, &model);
    for (const auto& cp : {"cp1250", "utf8", "utf16be", "utf32"}) {
        SCOPED_TRACE(cp);
        auto bytes = manager.getByName(cp)->fromU32(U"Dzień dobry. Proszę o przesłanie dokumentów do piątku.");
        auto results = detector.detectCodepage(bytes);
        ASSERT_FALSE(results.empty());
        EXPECT_EQ(results.front().language, "pl");
        EXPECT_EQ(results.front().codepage, cp);
        EXPECT_GT(results.front().languageScore, 0);
    }
    auto english = detector.detectCodepage("auto", "This is an English text about language detection.");
    ASSERT_FALSE(english.empty());
    EXPECT_EQ(english.front().language, "en");
}

TEST_F(NgramFixture, AutoRejectsUndefinedBytesOutsideStatisticalSample) {
    CpManager manager;
    Languages languages;
    languages.readFromFile(CPG_LANGUAGES_TXT);
    Detector detector(manager, languages, &model);
    auto paragraph = manager.getByName("cp1250")->fromU32(U"Dzień dobry. Proszę o przesłanie dokumentów do piątku. ");
    std::string bytes;
    for (int i = 0; i < 200; ++i) bytes += paragraph;
    bytes[3000] = char(0x81);
    auto results = detector.detectCodepage(bytes);
    ASSERT_FALSE(results.empty());
    EXPECT_NE(results.front().codepage, "cp1250");
    EXPECT_EQ(manager.getByName(results.front().codepage)->toU32(bytes).size(), bytes.size());
}

TEST_F(NgramFixture, ShippedModelsDetectPolishAndEnglishWithoutLanguageHint) {
    NgramModel shipped;
    shipped.loadDirectory(CPG_MODELS_DIR);
    ASSERT_EQ(shipped.languageCodes().size(), 32u);
    EXPECT_EQ(shipped.keepPercent(), 100u);
    CpManager manager;
    Languages languages;
    languages.readFromFile(CPG_LANGUAGES_TXT);
    Detector detector(manager, languages, &shipped);
    auto polish = U"Dzień dobry. Przesyłam zestawienie kosztów za wrzesień. Proszę o sprawdzenie "
                   "dokumentów i przesłanie odpowiedzi do piątku. Dziękuję za pomoc.";
    auto bytes = manager.getByName("cp1250")->fromU32(polish);
    auto results = detector.detectCodepage(bytes);
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results.front().language, "pl");
    EXPECT_EQ(results.front().codepage, "cp1250");
    auto english = detector.detectCodepage("The document describes a computer program and its architecture. "
                                           "Please read the instructions before installing the software.");
    ASSERT_FALSE(english.empty());
    EXPECT_EQ(english.front().language, "en");
    auto part = shipped.forLanguage("pl");
    EXPECT_DOUBLE_EQ(shipped.score("pl", polish), part.score("pl", polish));
}

TEST_F(NgramFixture, ConfidenceNeedsEnoughDistinguishingText) {
    NgramModel shipped;
    shipped.loadDirectory(CPG_MODELS_DIR);
    CpManager manager;
    Languages languages;
    languages.readFromFile(CPG_LANGUAGES_TXT);
    Detector detector(manager, languages, &shipped);
    auto cp1250 = manager.getByName("cp1250");
    auto clear = detector.detectCodepage("pl", cp1250->fromU32(
        U"Dzień dobry. Przesyłam zestawienie kosztów za wrzesień. Proszę o sprawdzenie "
         "dokumentów i przesłanie odpowiedzi do piątku. Dziękuję za pomoc."));
    ASSERT_FALSE(clear.empty());
    EXPECT_EQ(clear.front().codepage, "cp1250");
    EXPECT_GE(clear.front().confidence, 0.95);
    EXPECT_GE(clear.front().languageConfidence, 0.95);
    EXPECT_EQ(Detector::plausible(clear).size(), 1u);
    // Two Polish letters in source code: amiga-fat decodes them nearly as well.
    auto source = cp1250->fromU32(U"  InfoTabSheet.Caption := 'Programy zewnętrzne';\n"
                                   "  OpenDial.Caption:='Wskaż program do symulacji';\n");
    auto weak = detector.detectCodepage("pl", source);
    ASSERT_FALSE(weak.empty());
    EXPECT_LT(weak.front().confidence, 0.95);
    auto alternatives = Detector::plausible(weak);
    EXPECT_GT(alternatives.size(), 1u);
    double sum = 0;
    std::set<int> decodings;
    for (const auto& result : alternatives) {
        EXPECT_TRUE(decodings.insert(result.decoding).second);
        sum += result.confidence;
    }
    EXPECT_GE(sum, 0.95);
    // cp1250 and iso-8859-2 decode these bytes identically: one alternative.
    auto same = [&](const std::string& name) {
        return std::find_if(weak.begin(), weak.end(), [&](const auto& r) { return r.codepage == name; });
    };
    ASSERT_NE(same("cp1250"), weak.end());
    ASSERT_NE(same("iso-8859-2"), weak.end());
    EXPECT_EQ(same("cp1250")->decoding, same("iso-8859-2")->decoding);
    EXPECT_DOUBLE_EQ(same("cp1250")->confidence, same("iso-8859-2")->confidence);
    // UTF-8 source code: the language comes from the Polish words, not the code.
    std::string code;
    for (int i = 0; i < 40; ++i)
        code += "  if Assigned(FList) then FList.Clear; Inc(Counter, Length(Items));\n";
    code += "  // Usuwamy wszystkie zaznaczone obiekty z bieżącej warstwy\n";
    for (int i = 0; i < 40; ++i) code += "  Result := FindComponent(Name) as TControl;\n";
    auto commented = detector.detectCodepage(code + "  // Zażółć gęślą jaźń\n"); // UTF-8 literals
    ASSERT_FALSE(commented.empty());
    EXPECT_EQ(commented.front().codepage, "utf8");
    EXPECT_EQ(commented.front().language, "pl");
    // ASCII decodes the same everywhere: certain encoding, whatever the language.
    auto ascii = detector.detectCodepage("pl", "Plain ASCII text.");
    ASSERT_FALSE(ascii.empty());
    EXPECT_DOUBLE_EQ(ascii.front().confidence, 1.0);
}

TEST_F(NgramFixture, OddLetterCaseAndRarityBreakTiesForCommonCodepages) {
    NgramModel shipped;
    shipped.loadDirectory(CPG_MODELS_DIR);
    CpManager manager;
    Languages languages;
    languages.readFromFile(CPG_LANGUAGES_TXT);
    // SMC differs from cp1250 here only in letter case: "przenieŚc".
    auto bytes = manager.getByName("cp1250")->fromU32(
        U"{przenieśc to do wspólnego unitu z okienkami dialogowymi i komunikatami}\n");
    auto confidenceOf = [&](double casePenalty, double rarityPenalty, const std::string& codepage) {
        Detector detector(manager, languages, &shipped);
        detector.setCasePenalty(casePenalty);
        detector.setRarityPenalty(rarityPenalty);
        for (const auto& result : detector.detectCodepage("pl", bytes))
            if (result.codepage == codepage) return result.confidence;
        return 0.0;
    };
    EXPECT_NEAR(confidenceOf(0, 0, "cp1250"), confidenceOf(0, 0, "smc"), 1e-9); // the model alone ties
    EXPECT_GT(confidenceOf(2, 0, "cp1250"), 0.8);
    EXPECT_GT(confidenceOf(2, 0.5, "cp1250"), confidenceOf(2, 0, "cp1250"));
    Detector detector(manager, languages, &shipped); // defaults: 2 and 0.5
    auto results = detector.detectCodepage("pl", bytes);
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results.front().codepage, "cp1250");
}

TEST_F(NgramFixture, PreviewShowsWhereOptionsDiffer) {
    CpManager manager;
    Languages languages;
    languages.readFromFile(CPG_LANGUAGES_TXT);
    Detector detector(manager, languages, &model);
    auto option = [](const std::string& codepage) {
        DetectionResult result{codepage, 0.5, 1, "pl"};
        return result;
    };
    // cp1250 and iso-8859-2 differ in ś (0x9C / 0xB6), not in ó, ł, ż and ę.
    auto bytes = manager.getByName("cp1250")->fromU32(
        U"Mój kot je łyżkę.\n    Napis := 'Świeży śnieg';\nMój kot je łyżkę.\nNapis := 'Świeży śnieg';\n"
        "To jest bardzo długa linia, w której różnica pojawia się dopiero daleko: ś na końcu tej linii.\n");
    auto lines = detector.preview(bytes, {option("cp1250"), option("iso-8859-2")});
    ASSERT_EQ(lines.size(), 2u); // the repeated line once, the line without ś not at all
    EXPECT_EQ(lines[0].variants[0], "Napis := 'Świeży śnieg';"); // leading spaces trimmed
    EXPECT_NE(lines[0].variants[1], lines[0].variants[0]);
    EXPECT_EQ(lines[0].byteOffset, bytes.find("Napis"));
    EXPECT_EQ(lines[1].variants[0].rfind("…", 0), 0u); // a cut line
    EXPECT_NE(lines[1].variants[0].find("ś na końcu"), std::string::npos);
    // UTF-8 against cp1250: every non-ASCII byte differs; undefined bytes are visible.
    auto utf8 = manager.getByName("utf8")->fromU32(U"Zatwierdź\n");
    auto mixed = detector.preview(utf8, {option("utf8"), option("cp1250")});
    ASSERT_EQ(mixed.size(), 1u);
    EXPECT_EQ(mixed[0].variants[0], "Zatwierdź");
    EXPECT_EQ(mixed[0].variants[1], "ZatwierdĹş");
    auto undefined = detector.preview(std::string("A\x81"), {option("cp1250"), option("iso-8859-2")});
    ASSERT_EQ(undefined.size(), 1u);
    EXPECT_EQ(undefined[0].variants[0], "A�");
    EXPECT_TRUE(detector.preview(bytes, {option("cp1250")}).empty());
}

TEST_F(NgramFixture, DetectionAmongLanguagesConsidersOnlyThoseLanguages) {
    NgramModel shipped;
    shipped.loadDirectory(CPG_MODELS_DIR);
    CpManager manager;
    Languages languages;
    languages.readFromFile(CPG_LANGUAGES_TXT);
    Detector detector(manager, languages, &shipped);
    auto bytes = manager.getByName("cp1250")->fromU32(
        U"Dzień dobry. Przesyłam zestawienie kosztów za wrzesień. Dziękuję za pomoc.");
    auto results = detector.detectCodepageAmong({"pl", "cs"}, bytes);
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results.front().language, "pl");
    EXPECT_EQ(results.front().codepage, "cp1250");
    for (const auto& result : results)
        EXPECT_TRUE(result.language == "pl" || result.language == "cz") << result.language;
    auto unicode = detector.detectCodepageAmong({"pl", "cz"}, manager.getByName("utf8")->fromU32(
        U"Dzień dobry. Przesyłam zestawienie kosztów za wrzesień."));
    ASSERT_FALSE(unicode.empty());
    EXPECT_EQ(unicode.front().codepage, "utf8");
    EXPECT_EQ(unicode.front().language, "pl");
    for (const auto& result : unicode)
        EXPECT_TRUE(result.language == "pl" || result.language == "cz") << result.language;
    // One language behaves like the known-language overload; auto means all.
    auto single = detector.detectCodepageAmong({"pl"}, bytes);
    auto known = detector.detectCodepage("pl", bytes);
    ASSERT_EQ(single.size(), known.size());
    EXPECT_EQ(single.front().codepage, known.front().codepage);
    EXPECT_EQ(detector.detectCodepageAmong({"auto"}, bytes).size(), detector.detectCodepage(bytes).size());
}

TEST_F(NgramFixture, PrunedModelWithMissingPrefixIsRejected) {
    std::ofstream out(path);
    out << "CPG_NGRAM 2\npruning 5\nlanguage bad 10\norder 1 1\n97 10\n"
           "order 2 0\norder 3 1\n426610712870912 1\nend\n";
    out.close();
    EXPECT_THROW(model.load(path), std::runtime_error);
    EXPECT_TRUE(model.contains("pl"));
    EXPECT_EQ(model.keepPercent(), 100u);
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
    // UTF-8 competes with the legacy decodings and wins by far.
    ASSERT_FALSE(unicode.empty());
    EXPECT_EQ(unicode.front().codepage, "utf8");
    EXPECT_GE(unicode.front().confidence, 0.999);
    EXPECT_EQ(Detector::plausible(unicode).size(), 1u);
    // With a BOM it is UTF-8 without competition.
    auto bom = detector.detectCodepage("pl", "\xEF\xBB\xBF" + manager.getByName("utf8")->fromU32(text));
    ASSERT_EQ(bom.size(), 1u);
    EXPECT_EQ(bom.front().codepage, "utf8");
    auto bytes = cp1250->fromU32(text) + char(0x81);
    auto damaged = detector.detectCodepage("pl", bytes);
    auto it = std::find_if(damaged.begin(), damaged.end(), [](const auto& r) { return r.codepage == "cp1250"; });
    EXPECT_EQ(it, damaged.end());
}
}
