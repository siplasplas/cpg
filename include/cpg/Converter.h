#ifndef CPG_CONVERTER_H
#define CPG_CONVERTER_H

#include <cpg/CpManager.h>
#include <vector>

enum class UnmappablePolicy { Reject, Replace };
enum class ConversionError {
    None, UnknownCodepage, UnsupportedCodepage, InvalidInput, Unrepresentable
};

struct ConversionIssue {
    size_t byteOffset; // Offset in the input, including any BOM.
    char32_t codepoint; // Unrepresentable scalar; 0 for invalid input bytes.
};

struct ConversionResult {
    bool success = false;
    std::string output; // Empty on failure: never save a partial conversion.
    ConversionError error = ConversionError::None;
    std::vector<ConversionIssue> issues; // Also populated for successful Replace.
};

// Strict conversion for editors. CpManager must outlive this object.
// BOMs are preserved as U+FEFF; no BOM is inserted automatically.
// Malformed input is always rejected, regardless of the output policy.
class Converter {
    CpManager& codepages;
public:
    explicit Converter(CpManager& manager) : codepages(manager) {}
    ConversionResult toUtf8(const std::string& sourceCodepage,
                            std::string_view bytes) const;
    ConversionResult fromUtf8(const std::string& targetCodepage,
                              std::string_view utf8,
                              UnmappablePolicy policy = UnmappablePolicy::Reject) const;
};

#endif
