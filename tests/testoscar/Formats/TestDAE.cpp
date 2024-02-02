#include <oscar/Formats/DAE.hpp>

#include <testoscar/testoscarconfig.hpp>

#include <gtest/gtest.h>
#include <oscar/Graphics/Mesh.hpp>
#include <oscar/Graphics/MeshGenerators.hpp>
#include <oscar/Scene/SceneDecoration.hpp>
#include <oscar/Utils/StringHelpers.hpp>

#include <sstream>

using osc::Contains;
using osc::DAEMetadata;
using osc::GenerateCubeMesh;
using osc::SceneDecoration;

TEST(DAE, WriteDecorationsAsDAEWorksForEmptyScene)
{
    DAEMetadata metadata{TESTOSCAR_APPNAME_STRING, TESTOSCAR_APPNAME_STRING};

    std::stringstream ss;
    WriteDecorationsAsDAE(ss, {}, metadata);

    ASSERT_FALSE(ss.str().empty());
}

TEST(DAE, WriteDecorationsAsDAEWorksForNonEmptyScene)
{
    DAEMetadata metadata{TESTOSCAR_APPNAME_STRING, TESTOSCAR_APPNAME_STRING};

    SceneDecoration const dec{.mesh = GenerateCubeMesh()};

    std::stringstream ss;
    WriteDecorationsAsDAE(ss, {{dec}}, metadata);

    ASSERT_FALSE(ss.str().empty());
}

TEST(DAE, SetAuthorWritesAuthorToOutput)
{
    DAEMetadata metadata{TESTOSCAR_APPNAME_STRING, TESTOSCAR_APPNAME_STRING};
    metadata.author = "TestThis";

    std::stringstream ss;
    WriteDecorationsAsDAE(ss, {}, metadata);

    ASSERT_TRUE(Contains(ss.str(), metadata.author));
}

TEST(DAE, SetAuthoringToolsWritesAuthoringToolToOutput)
{
    DAEMetadata metadata{TESTOSCAR_APPNAME_STRING, TESTOSCAR_APPNAME_STRING};
    metadata.authoringTool = "TestThis";

    std::stringstream ss;
    WriteDecorationsAsDAE(ss, {}, metadata);

    ASSERT_TRUE(Contains(ss.str(), metadata.authoringTool));
}
