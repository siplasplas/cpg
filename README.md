# cpg
code pages like Windows-1250 and other, conversion between codepages and Unicode

The languages.txt file is based on https://github.com/chardet/chardet/blob/main/chardet/metadata/languages.py

Detection through `Detector::detectCodepage(language, bytes)` runs in two stages:

1. ICU checks the input. If its best match is UTF-8, UTF-16LE/BE or UTF-32LE/BE,
   has confidence of at least 80/100, and passes strict decoding, detection
   returns that encoding. This stage does not require a known language.
2. Otherwise, the detector ranks codepages associated with the supplied language
   by alphabet coverage, with score taking precedence over codepage preference.

Unicode results use the registered names `utf8`, `utf16` (LE), `utf16be`,
`utf32` (LE), and `utf32be`. Their score is ICU confidence divided by 100;
legacy scores measure alphabet coverage, not probability. Empty input returns
no results. ASCII and texts with identical bytes in multiple codepages remain
ambiguous; the fallback ordering is a preference, not proof of an encoding.

```cpp
CpManager codepages;
Languages languages;
languages.readFromFile("languages.txt");
Detector detector(codepages, languages);
auto candidates = detector.detectCodepage("pl", bytes);
```

The `convcp` demo currently requires explicit source and target encodings.
