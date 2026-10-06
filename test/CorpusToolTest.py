"""Exercise training, provenance, strict conversion, and detection end to end."""
import csv
import pathlib
import subprocess
import sys
import tempfile

tool = sys.argv[1]


def run(*args, success=True):
    result = subprocess.run([tool, *map(str, args)], capture_output=True, text=True)
    assert (result.returncode == 0) == success, result.stdout + result.stderr
    return result


with tempfile.TemporaryDirectory(prefix="cpg-corpus-test-") as temporary:
    root = pathlib.Path(temporary)
    corpus = root / "corpus"
    corpus.mkdir()
    for iso, sentence in [
        ("pl", "Dzień dobry. Proszę o przesłanie dokumentów. Zażółć gęślą jaźń. "),
        ("cs", "Příliš žluťoučký kůň úpěl ďábelské ódy. "),
    ]:
        (corpus / f"{iso}.txt").write_text(
            "".join(f"{i}: {sentence * 8}\n" for i in range(100)), encoding="utf-8"
        )
    model = root / "model.ngram"
    run("train", "--corpus", corpus, "--output", model)
    assert model.is_file() and pathlib.Path(str(model) + ".corpus").is_file()
    sample = root / "sample.bin"
    text = "Zażółć gęślą jaźń. Dzień dobry. Proszę o przesłanie dokumentów."
    sample.write_bytes(text.encode("cp1250"))
    result = run("detect", "--model", model, "--lang", "pl", "--input", sample)
    assert result.stdout.splitlines()[1].startswith("cp1250,"), result.stdout
    sample.write_bytes(text.encode("utf-8"))
    result = run("detect", "--model", model, "--lang", "pl", "--input", sample)
    assert result.stdout.splitlines()[1].startswith("utf8,"), result.stdout
    report = root / "benchmark.csv"
    run("benchmark", "--corpus", corpus, "--test-corpus", corpus, "--model", model,
        "--output", report, "--samples", "3")
    with report.open() as stream:
        rows = list(csv.DictReader(stream))
    assert {row["language"] for row in rows} == {"pl", "cz"}
    assert sum(int(row["accepted"]) for row in rows) > 0
    assert sum(int(row["strict_skipped"]) for row in rows) > 0
    for row in rows:
        assert int(row["accepted"]) + int(row["strict_skipped"]) == int(row["attempted"])
        assert int(row["ambiguous"]) + int(row["identifiable"]) == int(row["accepted"])
        assert int(row["model_identifiable"]) <= int(row["identifiable"])
    assert pathlib.Path(str(report) + ".confusion.csv").is_file()
    compressed = root / "compact.ngram"
    run("compact", "--model", model, "--output", compressed)
    assert compressed.stat().st_size < model.stat().st_size
    split = root / "languages"
    run("split", "--model", model, "--output-dir", split)
    assert {p.name for p in split.glob("*.ngram")} == {"pl.ngram", "cz.ngram"}
    sample.write_bytes(text.encode("cp1250"))
    decoded = root / "decoded.txt"
    result = run("detect", "--model", split, "--input", sample, "--output", decoded)
    assert result.stdout.splitlines()[1].startswith("cp1250,pl,"), result.stdout
    assert decoded.read_bytes() == text.encode("utf-8")
    run("benchmark", "--corpus", corpus, "--model", split, "--output", report,
        "--samples", "3", "--mode", "auto")
    run("detect", "--model", compressed, "--input", sample, "--output", sample, success=False)
    pruned = root / "pl-pruned.ngram"
    run("compact", "--model", model, "--output", pruned, "--keep-percent", "10", "--lang", "pl")
    trained_small = root / "pl-trained.ngram"
    run("train", "--corpus", corpus, "--output", trained_small, "--keep-percent", "10",
        "--lang", "pl", "--format", "binary")
    assert pruned.read_bytes() == trained_small.read_bytes()
    with (corpus / "pl.txt").open("a", encoding="utf-8") as stream:
        stream.write("Zmieniony korpus.\n")
    result = run("benchmark", "--corpus", corpus, "--model", model,
                 "--output", report, "--samples", "1", success=False)
    assert "mismatch" in result.stderr
    (corpus / "pl.txt").write_bytes(b"invalid UTF-8 \xff\n")
    result = run("train", "--corpus", corpus, "--output", model, success=False)
    assert "Invalid UTF-8" in result.stderr
    run("detect", "--model", model, "--lang", "xx", "--input", sample, success=False)
    run("benchmark", "--samples", "-1", success=False)
