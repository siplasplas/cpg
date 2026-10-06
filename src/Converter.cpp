#include <cpg/Converter.h>
#include <unicode/ucnv.h>
#include <unicode/ucnv_err.h>
#include <unicode/utf8.h>
#include <array>
#include <memory>
#include <unordered_map>

namespace {
const char* unicodeName(const std::string& name) {
    if (name == "utf8") return "UTF-8";
    if (name == "utf16") return "UTF-16LE";
    if (name == "utf16be") return "UTF-16BE";
    if (name == "utf32") return "UTF-32LE";
    if (name == "utf32be") return "UTF-32BE";
    return nullptr;
}

struct Scalar { char32_t value; size_t offset; };

bool decodeUnicode(const char* encoding, std::string_view bytes,
                   std::vector<Scalar>& scalars, ConversionResult& result) {
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<UConverter, decltype(&ucnv_close)> converter(
        ucnv_open(encoding, &status), ucnv_close);
    if (U_FAILURE(status)) {
        result.error = ConversionError::UnsupportedCodepage;
        return false;
    }
    ucnv_setToUCallBack(converter.get(), UCNV_TO_U_CALLBACK_STOP,
                      nullptr, nullptr, nullptr, &status);
    if (U_FAILURE(status)) {
        result.error = ConversionError::UnsupportedCodepage;
        return false;
    }
    if (bytes.empty()) return true;
    const char* cursor = bytes.data();
    const char* end = cursor + bytes.size();
    while (cursor < end) {
        const size_t offset = cursor - bytes.data();
        UChar32 value = ucnv_getNextUChar(converter.get(), &cursor, end, &status);
        if (U_FAILURE(status) || value < 0 || value > 0x10ffff ||
            (value >= 0xd800 && value <= 0xdfff)) {
            result.error = ConversionError::InvalidInput;
            result.issues.push_back({offset, 0});
            return false;
        }
        scalars.push_back({static_cast<char32_t>(value), offset});
    }
    return true;
}

void appendUtf8(std::string& output, char32_t value) {
    uint8_t buffer[4];
    int32_t length = 0;
    U8_APPEND_UNSAFE(buffer, length, value);
    output.append(reinterpret_cast<const char*>(buffer), length);
}

void appendUnicode(std::string& output, char32_t value, const std::string& name) {
    if (name == "utf8") { appendUtf8(output, value); return; }
    const bool big = name == "utf16be" || name == "utf32be";
    const int width = (name == "utf16" || name == "utf16be") ? 2 : 4;
    auto unit = [&](uint32_t n) {
        for (int i = 0; i < width; ++i) {
            int shift = (big ? width - 1 - i : i) * 8;
            output.push_back(static_cast<char>((n >> shift) & 0xff));
        }
    };
    if (width == 2 && value > 0xffff) {
        value -= 0x10000;
        unit(0xd800 + (value >> 10));
        unit(0xdc00 + (value & 0x3ff));
    } else unit(value);
}

std::array<std::u32string, 256> byteTable(Codepage& codepage) {
    std::array<std::u32string, 256> table;
    for (size_t i = 0; i < table.size(); ++i) {
        char byte = static_cast<char>(i);
        table[i] = codepage.toU32(std::string_view(&byte, 1));
    }
    return table;
}
}

ConversionResult Converter::toUtf8(const std::string& sourceCodepage,
                                    std::string_view bytes) const {
    ConversionResult result;
    Codepage* cp = codepages.getByName(sourceCodepage);
    if (!cp) { result.error = ConversionError::UnknownCodepage; return result; }
    std::vector<Scalar> scalars;
    if (const char* encoding = unicodeName(cp->getName())) {
        if (!decodeUnicode(encoding, bytes, scalars, result)) return result;
        for (const auto& scalar : scalars) appendUtf8(result.output, scalar.value);
    } else {
        if (cp->maxCharLen() != 1) {
            result.error = ConversionError::UnsupportedCodepage;
            return result;
        }
        auto table = byteTable(*cp);
        for (size_t i = 0; i < bytes.size(); ++i) {
            const auto& decoded = table[static_cast<unsigned char>(bytes[i])];
            if (decoded.size() != 1) result.issues.push_back({i, 0});
            else appendUtf8(result.output, decoded.front());
        }
        if (!result.issues.empty()) {
            result.error = ConversionError::InvalidInput;
            result.output.clear();
            return result;
        }
    }
    result.success = true;
    return result;
}

ConversionResult Converter::fromUtf8(const std::string& targetCodepage,
                                      std::string_view utf8,
                                      UnmappablePolicy policy) const {
    ConversionResult result;
    Codepage* cp = codepages.getByName(targetCodepage);
    if (!cp) { result.error = ConversionError::UnknownCodepage; return result; }
    std::vector<Scalar> scalars;
    if (!decodeUnicode("UTF-8", utf8, scalars, result)) return result;
    if (unicodeName(cp->getName())) {
        for (const auto& scalar : scalars)
            appendUnicode(result.output, scalar.value, cp->getName());
    } else {
        if (cp->maxCharLen() != 1) {
            result.error = ConversionError::UnsupportedCodepage;
            return result;
        }
        std::unordered_map<char32_t, char> reverse;
        auto table = byteTable(*cp);
        for (size_t i = 0; i < table.size(); ++i)
            if (table[i].size() == 1) reverse.emplace(table[i].front(), static_cast<char>(i));
        for (const auto& scalar : scalars) {
            auto found = reverse.find(scalar.value);
            if (found != reverse.end()) result.output.push_back(found->second);
            else {
                result.issues.push_back({scalar.offset, scalar.value});
                result.output.push_back('?');
            }
        }
        if (!result.issues.empty() && policy == UnmappablePolicy::Reject) {
            result.error = ConversionError::Unrepresentable;
            result.output.clear();
            return result;
        }
    }
    result.success = true;
    return result;
}
