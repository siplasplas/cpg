#include <cpg/Codepage.h>

#include <utility>

Codepage::Codepage(std::string name):name(std::move(name)) {
}