# Compressed models: 32 languages

Each `.ngram` is a single-language binary model derived from the reference
1 MB-per-language Wikipedia corpus. All unigram, bigram and trigram counts are
preserved: this is **lossless serialization and compression**, not frequency
pruning. The files total **721,895 bytes**; `pl.ngram` takes **24,956 bytes**.
The original aggregate text model takes 7,876,964 bytes. Splitting does not
change any language score, and automatic identification needs no extra model.

`NgramModel::loadDirectory("data/models")` loads every profile for automatic
language and codepage detection. `load("data/models/pl.ngram")` loads only Polish
for a constrained search. The project uses `cz` for Czech, so its file is
`cz.ngram`; the CLI accepts the alias `cs`.

## Reproduce

See [the reference corpus manifest](../corpus/1MB.ngram.corpus) and
[training methodology](../corpus/README.md). After reproducing the full model:

```sh
build/cpg_corpus split \
  --model corpus-results/1MB.ngram \
  --output-dir corpus-results/reproduced-models
cd corpus-results/reproduced-models
sha256sum -c ../../data/models/SHA256SUMS
```

The output directory must be new or empty. Binary files were generated with ICU
78.2 and zlib 1.3.1. Different compressor versions may change bytes while retaining
the same counts. `SHA256SUMS` records the shipped files. The source manifest is
kept once in `data/corpus`; generated split directories additionally receive a
`source.corpus` copy for benchmarking convenience.

## Binary format, version 3

- ASCII magic `CPG_NGRAM 3\n`.
- Unsigned base-128 varints containing decompressed and compressed payload sizes.
- A zlib stream compressed at level 9.
- The decompressed payload starts with retained trigram percentage (100) and
  language count (1), followed by UTF-8 language-name length/name and total count.
- Each of orders 1, 2 and 3 stores its entry count, then sorted numeric keys as
  unsigned varint deltas and counts as unsigned varints. Unicode codepoints use
  21-bit fields in the keys.

The loader also accepts legacy text formats 1 and 2. It checks payload bounds,
compression integrity, duplicate keys and original prefix counts. `.gitattributes`
marks these models as binary so Git does not translate newlines or merge bytes.

## Validation

The compact aggregate reproduced the complete model's benchmark exactly:
15,584/15,830 identifiable samples (**98.446%**) with the language provided.
A separate automatic-language run of 20 windows per size/language produced
5,913/6,005 correct decoded texts (**98.468%**) and 5,854/6,005 correct language
keys (**97.485%**). These are accepted, losslessly converted Wikipedia samples;
they do not establish universal accuracy. Very short, mixed-language and
unsupported-language text remain ambiguous; Romanian and Vietnamese legacy
tables have low conversion coverage. The API returns ranked candidates so an
editor can decode the best match without asking for a language.
