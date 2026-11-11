#include <cpg/Version.h>

#include <gtest/gtest.h>

#include <string>

TEST(VersionTest, LibraryMatchesTheHeaders)
{
    const cpg::LibraryVersion v = cpg::libraryVersion();
    EXPECT_EQ(v.abi, CPG_ABI_VERSION);
    EXPECT_EQ(v.major, CPG_VERSION_MAJOR);
    EXPECT_EQ(v.minor, CPG_VERSION_MINOR);
    EXPECT_EQ(v.patch, CPG_VERSION_PATCH);
    EXPECT_EQ(std::to_string(v.major) + "." + std::to_string(v.minor) + "." + std::to_string(v.patch),
              CPG_VERSION_STRING);
    EXPECT_TRUE(cpg::libraryFits());
}
