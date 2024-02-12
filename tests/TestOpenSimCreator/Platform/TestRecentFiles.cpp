#include <OpenSimCreator/Platform/RecentFiles.h>

#include <TestOpenSimCreator/TestOpenSimCreatorConfig.h>

#include <gtest/gtest.h>

#include <filesystem>

using namespace osc;

// automated repro for a bug found in #811
//
// the bug was that the `RecentFiles` implementation had a bad comparison
// function with std::sort, which is UB and caused a segfault in more recent
// MacOS standard library implementations
TEST(RecentFiles, CorrectlyLoadsRecentFilesFixture)
{
    std::filesystem::path const srcDir{OSC_TESTING_SOURCE_DIR};
    std::filesystem::path const fixture = srcDir / "build_resources/TestOpenSimCreator/Platform/811_repro.txt";
    ASSERT_NO_THROW({ RecentFiles rfs(fixture); });
}
