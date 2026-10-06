# cpg
code pages like Windows-1250 and other, conversion between codepages and Unicode

The languages.txt file is based on https://github.com/chardet/chardet/blob/main/chardet/metadata/languages.py

Detection through `Detector::detectCodepage(language, bytes)` runs in two stages:

1. ICU checks the input. If its best match is UTF-8, UTF-16LE/BE or UTF-32LE/BE,
   has confidence of at least 80/100, and passes strict decoding, detection
   returns that encoding. This stage does not require a known language.
2. Otherwise, the detector ranks codepages associated with the supplied language
   using corpus-trained character models. In automatic mode it ranks language
   and codepage pairs; with a known language it searches only that language's
   candidates. Without a model, the known-language API uses alphabet coverage.
   Score takes precedence over codepage preference.

Unicode results use the registered names `utf8`, `utf16` (LE), `utf16be`,
`utf32` (LE), and `utf32be`. Their score is ICU confidence divided by 100;
legacy scores measure geometric mean character likelihood (with a model) or
alphabet coverage (without one), not detection confidence. Empty input returns
no results. ASCII and texts with identical bytes in multiple codepages remain
ambiguous; the fallback ordering is a preference, not proof of an encoding.

```cpp
CpManager codepages;
Languages languages;
languages.readFromFile("languages.txt");
Detector detector(codepages, languages);
auto candidates = detector.detectCodepage("pl", bytes);
```

For an editor that opens unknown text files automatically:

```cpp
NgramModel model;
model.loadDirectory("data/models"); // all 32 language profiles
Detector detector(codepages, languages, &model);
auto candidates = detector.detectCodepage(bytes); // no language question
if (!candidates.empty()) {
    const auto& best = candidates.front();
    auto text = codepages.getByName(best.codepage)->toU32(bytes);
    // Display text; best.language contains the inferred language key.
}
```

Load `data/models/pl.ngram` alone when Polish is known. Unicode detection uses
ICU first, then models rank the decoded text's language. Automatic legacy
detection compares language/codepage pairs. For files larger than 6144 bytes,
legacy statistics sample 2048 bytes at the beginning, middle and end; the top
candidate is then checked against the whole file for undefined bytes. Unicode
language scoring similarly samples decoded codepoints. Small files use all
their text. Language scores are likelihood rankings, not calibrated confidence;
short, mixed-language or unsupported-language text can be misidentified.

The `convcp` demo still requires explicit encodings; `cpg_corpus detect` provides
automatic identification and optional UTF-8 output.

## Corpus training and evaluation

Build the `cpg_corpus` executable:
The build requires ICU and zlib development libraries.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
mkdir -p corpus-results
```

The tool reads a directory containing UTF-8 files named `<language>.txt`. It
supports the 32 languages in `languages.txt`, mapping `cs.txt` to the project's
`cz` identifier. The original corpus is only read. Generated models and reports
in `corpus-results/` are ignored by Git.
The [reference manifest and generation notes](data/corpus/README.md) and
[32 compressed binary models](data/models/README.md) are versioned. The complete
binary models total **721,895 bytes**, about **9.2%** of the old 7,876,964-byte
text model, without discarding any statistics. Polish alone takes **24,956 bytes**.

Ready-to-use automatic detection and conversion, without supplying a language:

```sh
build/cpg_corpus detect --model data/models --input document.txt --output decoded-utf8.txt
```

```sh
build/cpg_corpus train \
  --corpus /home/andrzej/dump/corpus/1MB \
  --output corpus-results/1MB.ngram

build/cpg_corpus benchmark \
  --corpus /home/andrzej/dump/corpus/1MB \
  --model corpus-results/1MB.ngram \
  --output corpus-results/1MB.csv --samples 100

build/cpg_corpus detect \
  --model corpus-results/1MB.ngram --lang pl --input document.txt
```

Use `--lang pl` on `train` or `benchmark` to process a single language, or on
`detect` to constrain the search. `detect` defaults to automatic language
identification; `--model` accepts a file or a directory. With a directory and a
known language, only its individual file is loaded. `train`, `benchmark` and
`detect` accept `--languages FILE` to override language metadata.

To compact an existing model without removing data, or export one file per language:

```sh
build/cpg_corpus compact --model corpus-results/1MB.ngram --output corpus-results/compact.ngram
build/cpg_corpus split --model corpus-results/1MB.ngram --output-dir corpus-results/languages
```

Both default to compressed binary. `--format text` retains a readable model.
`train --format binary` generates binary directly. Optional `--keep-percent 10`
retains the most frequent 10% of each language's trigrams; all unigram and bigram
counts remain. Frequency ties are broken by numeric key. Pruned models back off
to lower orders for omitted entries. This reduces size further but can worsen
detection, so the shipped models keep **100%**. Prune a complete model once;
incremental training of a pruned model is refused.

The model counts lowercase Unicode characters, bigrams and trigrams. It
preserves accents and combining marks, and collapses punctuation, whitespace
and digits to word boundaries. Conditional probabilities use interpolated
backoff to handle unseen sequences. The statistical detector gives a candidate
score of zero if its single-byte decoder drops any input bytes.

Splitting uses a deterministic FNV-1a hash of each source line: buckets 0–7
train, bucket 8 is reserved for validation, bucket 9 is used for testing. Identical
lines stay in the same partition across corpus sizes. Percentages apply to hash
buckets, so actual byte totals vary. The supplied plain text has no article IDs;
different lines from one article can occur in different partitions. This is a
line-level benchmark, not an independent article-level or real-world accuracy
estimate. For article-level splitting, retain article IDs in a future corpus.

Training writes a `.ngram.corpus` sidecar with source hashes. Benchmarking
verifies that its training corpus matches those hashes before evaluating the
held-out partition. Keep the sidecar with the model.

To compare corpus sizes fairly, use the same test directory for all models:

```sh
for size in 1MB 5MB 10MB 20MB; do
  build/cpg_corpus train \
    --corpus /home/andrzej/dump/corpus/$size \
    --output corpus-results/$size.ngram
  build/cpg_corpus benchmark \
    --corpus /home/andrzej/dump/corpus/$size \
    --test-corpus /home/andrzej/dump/corpus/1MB \
    --model corpus-results/$size.ngram \
    --output corpus-results/$size.csv --samples 100
done
```

The benchmark samples 32, 128, 512 and 2048 Unicode codepoints and converts them
to the registered single-byte codepages for that language. Successful samples
therefore have those byte lengths. A strict roundtrip must reproduce the original
text exactly; unrepresentable fragments are counted as `strict_skipped`, rather
than replacing letters with `?` or silently transliterating them. Standard and
custom encodings use this library's conversion tables: this measures detection
against those tables, not independent verification of the mappings themselves.
Unregistered metadata entries are reported and skipped (currently CP720 and
TIS-620). Modern Romanian and Vietnamese text can have very low coverage in the
available legacy tables; consult `accepted` before interpreting accuracy.
By default the language is supplied to the detector. Add `--mode auto` to measure
unknown-language detection; the baseline remains the known-language heuristic.
`model_language` counts correct language keys among accepted samples. Use
`--manifest data/corpus/1MB.ngram.corpus` to benchmark the shipped `data/models`
directory against its original corpus. Automatic results can select an equivalent
encoding from another language; `model_text` measures editor-visible correctness.

CSV columns are counts, not percentages:

| Columns | Meaning |
|---|---|
| `attempted`, `strict_skipped`, `accepted` | Conversion coverage |
| `ambiguous`, `identifiable` | Whether another candidate decodes the same bytes to exactly the same text |
| `baseline_top1`, `model_top1` | Exact codepage-name matches among all accepted samples |
| `baseline_text`, `model_text` | Correct decoded text, including equivalent encodings |
| `baseline_identifiable`, `model_identifiable` | Exact matches among identifiable samples |

For each algorithm, divide its `*_identifiable` count by `identifiable` to
measure accuracy on resolvable cases. The accompanying `.csv.confusion.csv`
lists the model's expected/predicted encoding pairs. More corpus data does not
guarantee improvement; examine individual languages and sample lengths.

Applications can load the same model directly:

```cpp
NgramModel model;
model.load("data/models/pl.ngram");
Detector detector(codepages, languages, &model); // model must outlive detector
auto candidates = detector.detectCodepage("pl", bytes);
```
