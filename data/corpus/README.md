# Reference corpus: 1 MB per language

`1MB.ngram.corpus` is the small, versioned provenance manifest for the reference
model. It contains 32 source-file sizes and FNV-1a 64-bit hashes, not character
statistics. Detection uses the versioned compressed models in `../models/`,
generated from `corpus-results/1MB.ngram`. Full text models, benchmark CSVs and
source texts are not included in Git.

One manifest is retained: benchmarks on the same held-out samples produced
98.446%, 98.490%, 98.433% and 98.484% accuracy for the 1/5/10/20 MB corpora,
respectively (15,830 identifiable samples, 100 windows per size and language).
These small differences do not establish a benefit from larger corpora. The
reference uses the smallest corpus. This is a line-level Wikipedia benchmark;
it includes per-language regressions and is not a real-world accuracy estimate.

## Sources and calculation

The user supplied Wikipedia text extracted to UTF-8 `<language>.txt` files in
`/home/andrzej/dump/corpus/1MB`. Each language contributes approximately 1 MB;
exact byte lengths and hashes are recorded in the manifest. Original dump dates,
article IDs and extraction commands were not supplied. `cs.txt` maps to the
project's `cz` language identifier.

The generator uses:

- FNV-1a of each source line (without newline or trailing CR), modulo 10:
  buckets 0–7 train, 8 reserved for validation, 9 held out for testing.
- ICU lowercase letters; accents and combining marks are preserved.
- Punctuation, whitespace and digits collapsed into word boundaries.
- Counts of Unicode characters, pairs and triples, reset between source lines.
- Sorted language names and numeric n-gram keys for deterministic serialization.

FNV-1a starts at `14695981039346656037`, XORs each byte and multiplies by
`1099511628211` modulo 2^64. Manifest hashes cover each whole source file,
including its exact newline bytes. The model was generated using ICU **78.2**.
Source lines from the same article may appear in different partitions because
the files do not retain article boundaries.

## Reproduce

Run from the repository root with the exact source files identified by the
manifest:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
mkdir -p corpus-results
build/cpg_corpus train \
  --corpus /home/andrzej/dump/corpus/1MB \
  --output corpus-results/1MB.ngram
cmp data/corpus/1MB.ngram.corpus corpus-results/1MB.ngram.corpus
sha256sum corpus-results/1MB.ngram
```

Expected model: **7,876,964 bytes**, SHA-256
`b60dc23ffe57c4d068b4e3c34503952d319ae8202dc726bafc49e9f3cf0a1f69`.
Fresh Wikipedia dumps generally have different contents and will produce a
different manifest and model.

To reproduce the evaluation:

```sh
build/cpg_corpus benchmark \
  --corpus /home/andrzej/dump/corpus/1MB \
  --model corpus-results/1MB.ngram \
  --output corpus-results/1MB.csv --samples 100
```

The statistical score uses additive unigram smoothing of 0.1 and conditional
bigram/trigram backoff strength 4, then takes the geometric mean of character
probabilities. It is a ranking score, not detection confidence. See the main
[README](../../README.md) for report columns and conversion coverage limits.
