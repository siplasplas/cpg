#include <cpg/Version.h>

namespace cpg {

// The macros as this library was built: what libraryFits() compares the
// headers of a program with.
LibraryVersion libraryVersion()
{
    return {CPG_ABI_VERSION, CPG_VERSION_MAJOR, CPG_VERSION_MINOR, CPG_VERSION_PATCH};
}

} // namespace cpg
