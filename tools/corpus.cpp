#include <cpg/Detector.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <unicode/ustring.h>

namespace fs = std::filesystem;
namespace {
using Options = std::map<std::string, std::string>;
constexpr uint64_t hashStart = 14695981039346656037ULL;
uint64_t hashBytes(std::string_view bytes, uint64_t hash = hashStart) {
    for (unsigned char c : bytes) { hash ^= c; hash *= 1099511628211ULL; }
    return hash;
}

std::string required(const Options& options, const std::string& key) {
    auto it = options.find(key);
    if (it == options.end()) throw std::runtime_error("Missing " + key);
    return it->second;
}

size_t positive(const Options& options, const std::string& key, size_t fallback) {
    auto it = options.find(key);
    if (it == options.end()) return fallback;
    const std::string& value = it->second;
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error("Invalid " + key);
    size_t used = 0;
    auto n = std::stoull(value, &used);
    if (used != value.size() || n == 0 || n > 1000000) throw std::runtime_error("Invalid " + key);
    return static_cast<size_t>(n);
}

std::string isoName(std::string iso) { return iso == "cs" ? "cz" : iso; }

unsigned keepPercent(const Options& options) {
    auto percent = positive(options, "--keep-percent", 100);
    if (percent > 100) throw std::runtime_error("--keep-percent must be between 1 and 100");
    return static_cast<unsigned>(percent);
}

bool binaryFormat(const Options& options, bool fallback) {
    if (!options.count("--format")) return fallback;
    if (options.at("--format") == "binary") return true;
    if (options.at("--format") == "text") return false;
    throw std::runtime_error("--format must be binary or text");
}

void loadModel(NgramModel& model, const fs::path& path) {
    if (fs::is_directory(path)) model.loadDirectory(path);
    else model.load(path);
}

std::u32string decodeUtf8(std::string_view bytes) {
    if (bytes.empty()) return {};
    if (bytes.size() > INT32_MAX) throw std::runtime_error("UTF-8 line too long");
    UErrorCode status = U_ZERO_ERROR;
    int32_t len = 0;
    u_strFromUTF8(nullptr, 0, &len, bytes.data(), static_cast<int32_t>(bytes.size()), &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status))
        throw std::runtime_error("Invalid UTF-8 corpus");
    status = U_ZERO_ERROR;
    std::u16string u16(len, 0);
    u_strFromUTF8(reinterpret_cast<UChar*>(u16.data()), len, nullptr, bytes.data(), bytes.size(), &status);
    if (U_FAILURE(status)) throw std::runtime_error("Invalid UTF-8 corpus");
    std::u32string result;
    for (size_t i = 0; i < u16.size(); ++i) {
        char32_t c = u16[i];
        if (c >= 0xD800 && c <= 0xDBFF) {
            c = 0x10000 + ((c - 0xD800) << 10) + (u16[++i] - 0xDC00);
        }
        if (c != 0xFEFF) result.push_back(c);
    }
    return result;
}

struct CorpusInfo {
    uint64_t hash = hashStart;
    size_t bytes = 0, trainBytes = 0, validationBytes = 0, testBytes = 0;
    std::u32string test;
};

CorpusInfo readCorpus(const fs::path& path, const std::string& iso, NgramModel* model, bool collectTest) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open corpus: " + path.string());
    CorpusInfo info;
    std::string line;
    while (std::getline(in, line)) {
        // Hash exact source bytes, including whether the last line has a newline.
        info.hash = hashBytes(line, info.hash);
        const bool newline = !in.eof();
        if (newline) info.hash = hashBytes("\n", info.hash);
        size_t bytes = line.size() + newline;
        info.bytes += bytes;
        // Hashing content keeps duplicate lines in the same partition across
        // all corpus sizes. CRLF and LF versions have the same partition.
        std::string_view content(line);
        if (!content.empty() && content.back() == '\r') content.remove_suffix(1);
        unsigned bucket = hashBytes(content) % 10;
        auto text = decodeUtf8(content);
        if (bucket < 8) {
            info.trainBytes += bytes;
            if (model) model->add(iso, text);
        } else if (bucket == 8) {
            info.validationBytes += bytes;
        } else {
            info.testBytes += bytes;
            if (collectTest && !text.empty()) {
                info.test.append(text);
                info.test.push_back(U'\n');
            }
        }
    }
    if (in.bad()) throw std::runtime_error("Failed reading corpus: " + path.string());
    return info;
}

std::vector<std::pair<std::string, fs::path>> corpusFiles(const Options& options, Languages& languages) {
    fs::path directory(required(options, "--corpus"));
    std::vector<std::pair<std::string, fs::path>> files;
    std::set<std::string> seen;
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".txt") continue;
        auto iso = isoName(entry.path().stem().string());
        if (options.count("--lang") && iso != isoName(options.at("--lang"))) continue;
        if (!languages.getByIsoCode(iso)) throw std::runtime_error("Unsupported language: " + iso);
        if (!seen.insert(iso).second) throw std::runtime_error("Duplicate language: " + iso);
        files.emplace_back(iso, entry.path());
    }
    std::sort(files.begin(), files.end());
    if (files.empty()) throw std::runtime_error("No matching <language>.txt files in " + directory.string());
    return files;
}

std::vector<std::string> candidates(const std::string& iso, Languages& languages, CpManager& manager) {
    std::vector<std::string> names;
    std::set<std::string> seen;
    for (auto raw : languages.getCharsetsForLanguage(iso)) {
        auto name = Detector::canonicalCodepageName(raw);
        if (!manager.getByName(name)) {
            std::cerr << "Unsupported codepage skipped: " << iso << ' ' << raw << '\n';
        } else if (seen.insert(name).second) names.push_back(name);
    }
    return names;
}

void train(const Options& options, Languages& languages) {
    auto percent = keepPercent(options);
    auto files = corpusFiles(options, languages);
    fs::path output(required(options, "--output"));
    NgramModel model;
    std::ostringstream manifest;
    manifest << "CPG_CORPUS 1 line-fnv1a-80-10-10\n";
    for (const auto& [iso, path] : files) {
        auto info = readCorpus(path, iso, &model, false);
        if (!model.contains(iso)) throw std::runtime_error("No training text for " + iso);
        manifest << iso << ' ' << info.bytes << ' ' << info.hash << '\n';
        std::cout << iso << " total=" << info.bytes << " train=" << info.trainBytes
                  << " validation=" << info.validationBytes << " test=" << info.testBytes << std::endl;
    }
    model.prune(percent);
    model.save(output, binaryFormat(options, false));
    std::ofstream meta(output.string() + ".corpus");
    meta << manifest.str();
    meta.close();
    if (!meta) throw std::runtime_error("Cannot write model provenance");
    std::cout << "Model saved: " << output << std::endl;
}

void compact(const Options& options) {
    const fs::path input(required(options, "--model")), output(required(options, "--output"));
    if (fs::weakly_canonical(input) == fs::weakly_canonical(output) ||
        (fs::exists(output) && fs::equivalent(input, output)))
        throw std::runtime_error("Compaction output must differ from the source model");
    auto percent = keepPercent(options);
    const bool binary = binaryFormat(options, true);
    NgramModel model;
    model.load(input);
    if (options.count("--lang")) model.retainLanguage(isoName(options.at("--lang")));
    model.prune(percent);
    const auto sourceMeta = fs::path(input.string() + ".corpus");
    const auto outputMeta = fs::path(output.string() + ".corpus");
    if (!fs::exists(sourceMeta) && fs::exists(outputMeta))
        throw std::runtime_error("Source has no .corpus provenance; choose a fresh output path");
    model.save(output, binary);
    if (fs::exists(sourceMeta)) {
        fs::copy_file(sourceMeta, outputMeta, fs::copy_options::overwrite_existing);
    }
    auto before = fs::file_size(input), after = fs::file_size(output);
    std::cout << "Model saved: " << output << " keep-percent=" << percent
              << " bytes-before=" << before << " bytes-after=" << after
              << " size-percent=" << (100.0 * after / before) << '\n';
}

void split(const Options& options) {
    const fs::path input(required(options, "--model"));
    const fs::path directory(required(options, "--output-dir"));
    // Refuse existing outputs so a source model in that directory cannot be
    // overwritten halfway through extraction.
    if (fs::exists(directory) && !fs::is_empty(directory))
        throw std::runtime_error("Split output directory must be new or empty");
    NgramModel model;
    model.load(input);
    model.prune(keepPercent(options));
    bool binary = binaryFormat(options, true);
    fs::create_directories(directory);
    uintmax_t total = 0;
    for (const auto& iso : model.languageCodes()) {
        // Filenames are ISO identifiers, never arbitrary paths from a model.
        if (iso.empty() || iso.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-") != std::string::npos)
            throw std::runtime_error("Invalid language identifier: " + iso);
        fs::path output = directory / (iso + ".ngram");
        model.forLanguage(iso).save(output, binary);
        auto size = fs::file_size(output);
        total += size;
        std::cout << iso << " bytes=" << size << '\n';
    }
    if (fs::exists(input.string() + ".corpus"))
        fs::copy_file(input.string() + ".corpus", directory / "source.corpus");
    std::cout << "Split models: " << directory << " total-bytes=" << total << '\n';
}

struct Metrics {
    size_t attempted = 0, skipped = 0, accepted = 0, ambiguous = 0;
    size_t baselineTop = 0, modelTop = 0, baselineText = 0, modelText = 0;
    size_t identifiable = 0, baselineIdentifiable = 0, modelIdentifiable = 0;
};

void benchmark(const Options& options, Languages& languages, CpManager& manager) {
    const auto modelPath = required(options, "--model");
    NgramModel model;
    loadModel(model, modelPath);
    const auto manifestPath = options.count("--manifest") ? fs::path(options.at("--manifest"))
        : fs::is_directory(modelPath) ? fs::path(modelPath) / "source.corpus"
                                     : fs::path(modelPath + ".corpus");
    std::ifstream meta(manifestPath);
    std::string header;
    std::getline(meta, header);
    if (header != "CPG_CORPUS 1 line-fnv1a-80-10-10")
        throw std::runtime_error("Missing or incompatible model provenance (.corpus)");
    std::map<std::string, std::pair<size_t, uint64_t>> source;
    std::string language;
    size_t bytes;
    uint64_t hash;
    while (meta >> language >> bytes >> hash) source[language] = {bytes, hash};
    auto files = corpusFiles(options, languages);
    std::map<std::string, fs::path> testFiles;
    if (options.count("--test-corpus")) {
        auto testOptions = options;
        testOptions["--corpus"] = options.at("--test-corpus");
        for (const auto& [iso, path] : corpusFiles(testOptions, languages)) testFiles[iso] = path;
    }
    size_t samples = positive(options, "--samples", 100);
    const auto mode = options.count("--mode") ? options.at("--mode") : "known";
    if (mode != "known" && mode != "auto") throw std::runtime_error("--mode must be known or auto");
    fs::path output(required(options, "--output"));
    std::ofstream report(output);
    if (!report) throw std::runtime_error("Cannot write benchmark: " + output.string());
    report << "language,codepage,bytes,attempted,strict_skipped,accepted,ambiguous,"
              "baseline_top1,model_top1,baseline_text,model_text,identifiable,"
              "baseline_identifiable,model_identifiable,model_language\n";
    std::map<std::tuple<std::string, std::string, std::string, size_t>, size_t> confusion;
    Detector baseline(manager, languages), trained(manager, languages, &model);
    Metrics total;
    for (const auto& [iso, path] : files) {
        auto info = readCorpus(path, iso, nullptr, testFiles.empty());
        if (!model.contains(iso) || !source.count(iso) ||
            source.at(iso) != std::make_pair(info.bytes, info.hash))
            throw std::runtime_error("Corpus/model mismatch for " + iso + "; train this corpus first");
        if (!testFiles.empty()) {
            if (!testFiles.count(iso)) throw std::runtime_error("Missing test corpus for " + iso);
            info = readCorpus(testFiles.at(iso), iso, nullptr, true);
        }
        auto names = candidates(iso, languages, manager);
        std::map<std::pair<std::string, size_t>, Metrics> rows;
        std::map<std::pair<std::string, size_t>, size_t> languageHits;
        std::mt19937_64 random(hashBytes(iso));
        for (size_t length : {32u, 128u, 512u, 2048u}) {
            if (info.test.size() < length) {
                std::cerr << "Too little held-out text: " << iso << ' ' << length << '\n';
                continue;
            }
            for (size_t sample = 0; sample < samples; ++sample) {
                size_t start = random() % (info.test.size() - length + 1);
                std::u32string_view text(info.test.data() + start, length);
                for (const auto& name : names) {
                    auto& row = rows[{name, length}];
                    ++row.attempted;
                    Codepage* cp = manager.getByName(name);
                    auto encoded = cp->fromU32(text);
                    // Strict roundtrip: never evaluate '?' replacement or a
                    // lossy custom mapping as a correctly encoded sample.
                    if (encoded.size() != length || cp->toU32(encoded) != text) {
                        ++row.skipped;
                        continue;
                    }
                    ++row.accepted;
                    size_t equivalent = 0;
                    for (const auto& alternative : names)
                        if (manager.getByName(alternative)->toU32(encoded) == text) ++equivalent;
                    bool ambiguous = equivalent > 1;
                    row.ambiguous += ambiguous;
                    row.identifiable += !ambiguous;
                    auto oldResults = baseline.detectCodepage(iso, encoded);
                    auto newResults = mode == "auto" ? trained.detectCodepage(encoded)
                                                     : trained.detectCodepage(iso, encoded);
                    if (!newResults.empty() && newResults.front().language == iso)
                        ++languageHits[{name, length}];
                    auto oldName = oldResults.empty() ? "none" : oldResults.front().codepage;
                    auto newName = newResults.empty() ? "none" : newResults.front().codepage;
                    bool oldCorrect = oldName == name, newCorrect = newName == name;
                    row.baselineTop += oldCorrect;
                    row.modelTop += newCorrect;
                    row.baselineIdentifiable += !ambiguous && oldCorrect;
                    row.modelIdentifiable += !ambiguous && newCorrect;
                    auto* oldCp = manager.getByName(oldName);
                    auto* newCp = manager.getByName(newName);
                    row.baselineText += oldCp && oldCp->toU32(encoded) == text;
                    row.modelText += newCp && newCp->toU32(encoded) == text;
                    ++confusion[{iso, name, newName, length}];
                }
            }
        }
        for (const auto& [key, row] : rows) {
            report << iso << ',' << key.first << ',' << key.second << ',' << row.attempted << ','
                   << row.skipped << ',' << row.accepted << ',' << row.ambiguous << ','
                   << row.baselineTop << ',' << row.modelTop << ',' << row.baselineText << ','
                   << row.modelText << ',' << row.identifiable << ',' << row.baselineIdentifiable << ','
                   << row.modelIdentifiable << ',' << languageHits[key] << '\n';
            total.accepted += row.accepted;
            total.identifiable += row.identifiable;
            total.baselineIdentifiable += row.baselineIdentifiable;
            total.modelIdentifiable += row.modelIdentifiable;
            total.ambiguous += row.ambiguous;
            total.skipped += row.skipped;
        }
        report.flush();
        std::cout << "Benchmarked " << iso << std::endl;
    }
    report.close();
    if (!report) throw std::runtime_error("Failed writing benchmark");
    std::ofstream matrix(output.string() + ".confusion.csv");
    matrix << "language,expected,predicted,bytes,count\n";
    for (const auto& [key, n] : confusion)
        matrix << std::get<0>(key) << ',' << std::get<1>(key) << ',' << std::get<2>(key)
               << ',' << std::get<3>(key) << ',' << n << '\n';
    matrix.close();
    if (!matrix) throw std::runtime_error("Failed writing confusion matrix");
    std::cout << "accepted=" << total.accepted << " strict_skipped=" << total.skipped
              << " ambiguous=" << total.ambiguous << " identifiable=" << total.identifiable
              << " baseline_correct=" << total.baselineIdentifiable
              << " model_correct=" << total.modelIdentifiable << '\n';
}

void detect(const Options& options, Languages& languages, CpManager& manager) {
    NgramModel model;
    const fs::path modelPath(required(options, "--model"));
    const auto iso = options.count("--lang") ? isoName(options.at("--lang")) : "auto";
    if (fs::is_directory(modelPath) && iso != "auto") model.load(modelPath / (iso + ".ngram"));
    else loadModel(model, modelPath);
    if (iso != "auto" && !model.contains(iso)) throw std::runtime_error("Model has no language: " + iso);
    std::ifstream in(required(options, "--input"), std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read input");
    std::string bytes((std::istreambuf_iterator<char>(in)), {});
    if (in.bad()) throw std::runtime_error("Failed reading input");
    Detector detector(manager, languages, &model);
    auto results = detector.detectCodepage(iso, bytes);
    std::cout << "codepage,language,score,rank,language_score\n";
    for (size_t i = 0; i < results.size() && i < 10; ++i)
        std::cout << results[i].codepage << ',' << results[i].language << ',' << results[i].score
                  << ',' << results[i].rank << ',' << results[i].languageScore << '\n';
    if (options.count("--output")) {
        if (fs::weakly_canonical(options.at("--output")) == fs::weakly_canonical(options.at("--input")) ||
            (fs::exists(options.at("--output")) && fs::equivalent(options.at("--output"), options.at("--input"))))
            throw std::runtime_error("UTF-8 output must differ from input");
        if (!bytes.empty() && results.empty()) throw std::runtime_error("No usable encoding candidate");
        std::string utf8;
        if (!results.empty())
            utf8 = manager.getByName("utf8")->fromU32(manager.getByName(results.front().codepage)->toU32(bytes));
        std::ofstream out(options.at("--output"), std::ios::binary);
        out.write(utf8.data(), utf8.size());
        out.close();
        if (!out) throw std::runtime_error("Failed writing UTF-8 output");
    }
}

void help() {
    std::cout << "Usage:\n"
        "  cpg_corpus train --corpus DIR --output MODEL [--lang ISO]\n"
        "                   [--keep-percent 1..100] (default: complete model)\n"
        "                   [--format text|binary] (default: text)\n"
        "  cpg_corpus compact --model MODEL --output SMALL_MODEL [--keep-percent 1..100] [--lang ISO]\n"
        "  cpg_corpus split --model MODEL --output-dir DIR [--keep-percent 1..100]\n"
        "compact/split default to compressed binary; --format text is available.\n"
        "  cpg_corpus benchmark --corpus DIR --model MODEL --output CSV [--samples 100] [--lang ISO]\n"
        "                       [--test-corpus DIR] (fixed test set for comparing corpus sizes)\n"
        "                       [--mode known|auto] (default: known)\n"
        "  cpg_corpus detect --model MODEL_OR_DIR --input FILE [--lang ISO|auto] [--output UTF8_FILE]\n"
        "Detection defaults to automatic language + codepage selection.\n"
        "train, benchmark and detect accept --languages FILE (default: project languages.txt).\n"
        "Input: UTF-8 <ISO>.txt files; cs is mapped to the project's cz.\n"
        "Split: line-content FNV-1a buckets 0..7 train, 8 reserved, 9 test.\n"
        "Benchmark sizes: 32, 128, 512, 2048 bytes; lossy conversions skipped.\n";
}
}

int main(int argc, char** argv) {
    try {
        if (argc < 2 || std::string(argv[1]) == "--help") { help(); return argc < 2 ? 1 : 0; }
        const std::string command(argv[1]);
        std::set<std::string> allowed{"--languages", "--lang"};
        if (command == "train") allowed.insert({"--corpus", "--output", "--keep-percent", "--format"});
        else if (command == "compact") allowed.insert({"--model", "--output", "--keep-percent", "--format"});
        else if (command == "split") allowed.insert({"--model", "--output-dir", "--keep-percent", "--format"});
        else if (command == "benchmark") allowed.insert({"--corpus", "--model", "--output", "--samples", "--test-corpus", "--mode", "--manifest"});
        else if (command == "detect") allowed.insert({"--model", "--input", "--output"});
        else throw std::runtime_error("Unknown command: " + command);
        Options options;
        for (int i = 2; i < argc; ++i) {
            std::string key(argv[i]);
            if (!allowed.count(key) || options.count(key) || i + 1 == argc)
                throw std::runtime_error("Invalid option: " + key);
            options.emplace(key, argv[++i]);
        }
        if (command == "compact" || command == "split") {
            if (options.count("--languages")) throw std::runtime_error("compact does not need --languages");
            if (command == "compact") compact(options);
            else {
                if (options.count("--lang")) throw std::runtime_error("split exports all loaded languages");
                split(options);
            }
            return 0;
        }
        Languages languages;
        auto languageFile = options.count("--languages") ? options.at("--languages") : CPG_LANGUAGES_TXT;
        if (!fs::is_regular_file(languageFile)) throw std::runtime_error("Missing languages file: " + languageFile);
        languages.readFromFile(languageFile);
        CpManager manager;
        if (command == "train") train(options, languages);
        else if (command == "benchmark") benchmark(options, languages, manager);
        else detect(options, languages, manager);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
