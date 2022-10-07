#include "MeshGen.hpp"

#include "src/Graphics/Mesh.hpp"
#include "src/Maths/Constants.hpp"
#include "src/Maths/MathHelpers.hpp"
#include "src/Utils/Assertions.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{
    struct UntexturedVert {
        glm::vec3 pos;
        glm::vec3 norm;
    };

    struct TexturedVert {
        glm::vec3 pos;
        glm::vec3 norm;
        glm::vec2 uv;
    };

    struct NewMeshData final {
        std::vector<glm::vec3> verts;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texcoords;
        std::vector<uint32_t> indices;
        osc::MeshTopography topography = osc::MeshTopography::Triangles;

        void clear()
        {
            verts.clear();
            normals.clear();
            texcoords.clear();
            indices.clear();
            topography = osc::MeshTopography::Triangles;
        }

        void reserve(size_t s)
        {
            verts.reserve(s);
            normals.reserve(s);
            texcoords.reserve(s);
            indices.reserve(s);
        }
    };

    osc::Mesh CreateMeshFromData(NewMeshData&& data)
    {
        osc::Mesh rv;
        rv.setTopography(data.topography);
        rv.setVerts(data.verts);
        rv.setNormals(data.normals);
        rv.setTexCoords(data.texcoords);
        rv.setIndices(data.indices);
        return rv;
    }
}

// standard textured cube with dimensions [-1, +1] in xyz and uv coords of
// (0, 0) bottom-left, (1, 1) top-right for each (quad) face
static constexpr std::array<TexturedVert, 36> g_ShadedTexturedCubeVerts = {{

    // back face
    {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},  // bottom-left
    {{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},  // top-right
    {{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},  // bottom-right
    {{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},  // top-right
    {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},  // bottom-left
    {{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},  // top-left

    // front face
    {{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},  // bottom-left
    {{1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},  // bottom-right
    {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},  // top-right
    {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},  // top-right
    {{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},  // top-left
    {{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},  // bottom-left

    // left face
    {{-1.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},  // top-right
    {{-1.0f, 1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},  // top-left
    {{-1.0f, -1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},  // bottom-left
    {{-1.0f, -1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},  // bottom-left
    {{-1.0f, -1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},  // bottom-right
    {{-1.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},  // top-right

    // right face
    {{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},  // top-left
    {{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},  // bottom-right
    {{1.0f, 1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},  // top-right
    {{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},  // bottom-right
    {{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},  // top-left
    {{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},  // bottom-left

    // bottom face
    {{-1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},  // top-right
    {{1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},  // top-left
    {{1.0f, -1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},  // bottom-left
    {{1.0f, -1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},  // bottom-left
    {{-1.0f, -1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},  // bottom-right
    {{-1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},  // top-right

    // top face
    {{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},  // top-left
    {{1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},  // bottom-right
    {{1.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},  // top-right
    {{1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},  // bottom-right
    {{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},  // top-left
    {{-1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}}  // bottom-left
}};

// standard textured quad
// - dimensions [-1, +1] in xy and [0, 0] in z
// - uv coords are (0, 0) bottom-left, (1, 1) top-right
// - normal is +1 in Z, meaning that it faces toward the camera
static constexpr std::array<TexturedVert, 6> g_ShadedTexturedQuadVerts = {{

    // CCW winding (culling)
    {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},  // bottom-left
    {{1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},  // bottom-right
    {{1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},  // top-right

    {{1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},  // top-right
    {{-1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},  // top-left
    {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},  // bottom-left
}};

// a cube wire mesh, suitable for GL_LINES drawing
//
// a pair of verts per edge of the cube. The cube has 12 edges, so 24 lines
static constexpr std::array<UntexturedVert, 24> g_CubeEdgeLines = {{
    // back

    // back bottom left -> back bottom right
    {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}},
    {{+1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}},

    // back bottom right -> back top right
    {{+1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}},
    {{+1.0f, +1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}},

    // back top right -> back top left
    {{+1.0f, +1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}},
    {{-1.0f, +1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}},

    // back top left -> back bottom left
    {{-1.0f, +1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}},
    {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}},

    // front

    // front bottom left -> front bottom right
    {{-1.0f, -1.0f, +1.0f}, {0.0f, 0.0f, +1.0f}},
    {{+1.0f, -1.0f, +1.0f}, {0.0f, 0.0f, +1.0f}},

    // front bottom right -> front top right
    {{+1.0f, -1.0f, +1.0f}, {0.0f, 0.0f, +1.0f}},
    {{+1.0f, +1.0f, +1.0f}, {0.0f, 0.0f, +1.0f}},

    // front top right -> front top left
    {{+1.0f, +1.0f, +1.0f}, {0.0f, 0.0f, +1.0f}},
    {{-1.0f, +1.0f, +1.0f}, {0.0f, 0.0f, +1.0f}},

    // front top left -> front bottom left
    {{-1.0f, +1.0f, +1.0f}, {0.0f, 0.0f, +1.0f}},
    {{-1.0f, -1.0f, +1.0f}, {0.0f, 0.0f, +1.0f}},

    // front-to-back edges

    // front bottom left -> back bottom left
    {{-1.0f, -1.0f, +1.0f}, {-1.0f, -1.0f, +1.0f}},
    {{-1.0f, -1.0f, -1.0f}, {-1.0f, -1.0f, -1.0f}},

    // front bottom right -> back bottom right
    {{+1.0f, -1.0f, +1.0f}, {+1.0f, -1.0f, +1.0f}},
    {{+1.0f, -1.0f, -1.0f}, {+1.0f, -1.0f, -1.0f}},

    // front top left -> back top left
    {{-1.0f, +1.0f, +1.0f}, {-1.0f, +1.0f, +1.0f}},
    {{-1.0f, +1.0f, -1.0f}, {-1.0f, +1.0f, -1.0f}},

    // front top right -> back top right
    {{+1.0f, +1.0f, +1.0f}, {+1.0f, +1.0f, +1.0f}},
    {{+1.0f, +1.0f, -1.0f}, {+1.0f, +1.0f, -1.0f}}
}};

osc::Mesh osc::GenTexturedQuad()
{
    NewMeshData data;
    data.reserve(g_ShadedTexturedQuadVerts.size());

    uint16_t index = 0;
    for (TexturedVert const& v : g_ShadedTexturedQuadVerts)
    {
        data.verts.push_back(v.pos);
        data.normals.push_back(v.norm);
        data.texcoords.push_back(v.uv);
        data.indices.push_back(index++);
    }

    OSC_ASSERT(data.verts.size() % 3 == 0);
    OSC_ASSERT(data.verts.size() == data.normals.size() && data.verts.size() == data.indices.size());

    return CreateMeshFromData(std::move(data));
}

osc::Mesh osc::GenUntexturedUVSphere(size_t sectors, size_t stacks)
{
    NewMeshData data;
    data.reserve(2*3*stacks*sectors);

    // this is a shitty alg that produces a shitty UV sphere. I don't have
    // enough time to implement something better, like an isosphere, or
    // something like a patched sphere:
    //
    // https://www.iquilezles.org/www/articles/patchedsphere/patchedsphere.htm
    //
    // This one is adapted from:
    //    http://www.songho.ca/opengl/gl_sphere.html#example_cubesphere


    // polar coords, with [0, 0, -1] pointing towards the screen with polar
    // coords theta = 0, phi = 0. The coordinate [0, 1, 0] is theta = (any)
    // phi = PI/2. The coordinate [1, 0, 0] is theta = PI/2, phi = 0
    std::vector<UntexturedVert> points;

    float thetaStep = 2.0f * fpi / sectors;
    float phiStep = fpi / stacks;

    for (size_t stack = 0; stack <= stacks; ++stack)
    {
        float phi = fpi2 - static_cast<float>(stack) * phiStep;
        float y = std::sin(phi);

        for (unsigned sector = 0; sector <= sectors; ++sector)
        {
            float theta = sector * thetaStep;
            float x = std::sin(theta) * std::cos(phi);
            float z = -std::cos(theta) * std::cos(phi);
            glm::vec3 pos{x, y, z};
            glm::vec3 normal{pos};
            points.push_back({pos, normal});
        }
    }

    // the points are not triangles. They are *points of a triangle*, so the
    // points must be triangulated

    uint16_t index = 0;
    auto push = [&data, &index](UntexturedVert const& v)
    {
        data.verts.push_back(v.pos);
        data.normals.push_back(v.norm);
        data.indices.push_back(index++);
    };

    for (size_t stack = 0; stack < stacks; ++stack)
    {
        size_t k1 = stack * (sectors + 1);
        size_t k2 = k1 + sectors + 1;

        for (size_t sector = 0; sector < sectors; ++sector, ++k1, ++k2)
        {
            // 2 triangles per sector - excluding the first and last stacks
            // (which contain one triangle, at the poles)

            UntexturedVert const& p1 = points[k1];
            UntexturedVert const& p2 = points[k2];
            UntexturedVert const& p1Plus1 = points[k1+1];
            UntexturedVert const& p2Plus1 = points[k2+1];

            if (stack != 0)
            {
                push(p1);
                push(p1Plus1);
                push(p2);
            }

            if (stack != (stacks - 1))
            {
                push(p1Plus1);
                push(p2Plus1);
                push(p2);
            }
        }
    }

    OSC_ASSERT(data.verts.size() % 3 == 0);
    OSC_ASSERT(data.verts.size() == data.normals.size() && data.verts.size() == data.indices.size());

    return CreateMeshFromData(std::move(data));
}

osc::Mesh osc::GenUntexturedSimbodyCylinder(size_t nsides)
{
    constexpr float c_TopY = +1.0f;
    constexpr float c_BottomY = -1.0f;
    constexpr float c_Radius = 1.0f;
    constexpr float c_TopDirection = c_TopY;
    constexpr float c_BottomDirection = c_BottomY;

    OSC_ASSERT(3 <= nsides && nsides < 1000000 && "the backend only supports 32-bit indices, you should double-check that this code would work (change this assertion if it does)");

    const float stepAngle = 2.0f*fpi / nsides;

    NewMeshData data;

    // helper: push mesh *data* (i.e. vert and normal) to the output
    auto pushData = [&data](glm::vec3 const& pos, glm::vec3 const& norm)
    {
        uint32_t idx = static_cast<uint32_t>(data.verts.size());
        data.verts.push_back(pos);
        data.normals.push_back(norm);
        return idx;
    };

    // helper: push primitive *indices* (into data) to the output
    auto pushTriangle = [&data](uint32_t p0Index, uint32_t p1Index, uint32_t p2Index)
    {
        data.indices.push_back(p0Index);
        data.indices.push_back(p1Index);
        data.indices.push_back(p2Index);
    };

    // top: a triangle fan
    {
        // preemptively push the middle and the first point and hold onto their indices
        // because the middle is used for all triangles in the fan and the first point
        // is used when completing the loop

        glm::vec3 const topNormal = {0.0f, c_TopDirection, 0.0f};
        uint32_t const midpointIndex = pushData({0.0f, c_TopY, 0.0f}, topNormal);
        uint32_t const loopStartIndex = pushData({c_Radius, c_TopY, 0.0f}, topNormal);

        // then go through each outer vertex one-by-one, creating a triangle between
        // the new vertex, the middle, and the previous vertex

        uint32_t p1Index = loopStartIndex;
        for (size_t side = 1; side < nsides; ++side)
        {
            float const theta = side * stepAngle;
            glm::vec3 const p2 = {std::cos(theta), c_TopY, std::sin(theta)};
            uint32_t const p2Index = pushData(p2, topNormal);

            pushTriangle(midpointIndex, p1Index, p2Index);
            p1Index = p2Index;
        }

        // finish loop
        pushTriangle(midpointIndex, p1Index, loopStartIndex);
    }

    // bottom: another triangle fan
    {
        // preemptively push the middle and the first point and hold onto their indices
        // because the middle is used for all triangles in the fan and the first point
        // is used when completing the loop

        glm::vec3 const bottomNormal = {0.0f, c_BottomDirection, 0.0f};
        uint32_t const midpointIndex = pushData({0.0f, c_BottomY, 0.0f}, bottomNormal);
        uint32_t const loopStartIndex = pushData({c_Radius, c_BottomY, 0.0f}, bottomNormal);

        // then go through each outer vertex one-by-one, creating a triangle between
        // the new vertex, the middle, and the previous vertex

        uint32_t p1Index = loopStartIndex;
        for (size_t side = 1; side < nsides; ++side)
        {
            float const theta = side * stepAngle;
            glm::vec3 const p2 = {c_Radius * std::cos(theta), c_BottomY, c_Radius * std::sin(theta)};
            uint32_t const p2Index = pushData(p2, bottomNormal);

            pushTriangle(midpointIndex, p1Index, p2Index);
            p1Index = p2Index;
        }

        // finish loop
        pushTriangle(midpointIndex, p1Index, loopStartIndex);
    }

    // sides: a loop of quads along the edges
    constexpr bool smoothShaded = true;
    static_assert(smoothShaded, "if you want rigid edges then it requires copying edge loops etc");

    if (smoothShaded)
    {
        glm::vec3 const initialNormal = glm::vec3{1.0f, 0.0f, 0.0f};
        uint32_t firstEdgeTop = pushData({c_Radius, c_TopY, 0.0f}, initialNormal);
        uint32_t firstEdgeBottom = pushData({c_Radius, c_BottomY, 0.0f}, initialNormal);

        uint32_t e1TopIdx = firstEdgeTop;
        uint32_t e1BottomIdx = firstEdgeBottom;
        for (size_t i = 1; i < nsides; ++i)
        {
            float const theta = i * stepAngle;
            float const xDir = std::cos(theta);
            float const zDir = std::sin(theta);
            float const x = c_Radius * xDir;
            float const z = c_Radius * zDir;

            glm::vec3 const normal = {xDir, 0.0f, zDir};
            uint32_t const e2TopIdx = pushData({x, c_TopY, z}, normal);
            uint32_t const e2BottomIdx = pushData({x, c_BottomY, z}, normal);

            pushTriangle(e1TopIdx, e1BottomIdx, e2BottomIdx);
            pushTriangle(e2BottomIdx, e2TopIdx, e1TopIdx);

            e1TopIdx = e2TopIdx;
            e1BottomIdx = e2BottomIdx;
        }
        // finish loop
        pushTriangle(e1TopIdx, e1BottomIdx, firstEdgeBottom);
        pushTriangle(firstEdgeBottom, firstEdgeTop, e1TopIdx);
    }

    return CreateMeshFromData(std::move(data));
}

osc::Mesh osc::GenUntexturedSimbodyCone(size_t nsides)
{
    NewMeshData data;
    data.reserve(2*3*nsides);

    constexpr float topY = +1.0f;
    constexpr float bottomY = -1.0f;
    const float stepAngle = 2.0f*fpi / nsides;

    uint16_t index = 0;
    auto push = [&data, &index](glm::vec3 const& pos, glm::vec3 const& norm)
    {
        data.verts.push_back(pos);
        data.normals.push_back(norm);
        data.indices.push_back(index++);
    };

    // bottom
    {
        glm::vec3 normal = {0.0f, -1.0f, 0.0f};
        glm::vec3 middle = {0.0f, bottomY, 0.0f};

        for (size_t i = 0; i < nsides; ++i)
        {
            float thetaStart = i * stepAngle;
            float thetaEnd = (i + 1) * stepAngle;

            glm::vec3 p1 = {std::cos(thetaStart), bottomY, std::sin(thetaStart)};
            glm::vec3 p2 = {std::cos(thetaEnd), bottomY, std::sin(thetaEnd)};

            push(middle, normal);
            push(p1, normal);
            push(p2, normal);
        }
    }

    // sides
    {
        for (size_t i = 0; i < nsides; ++i)
        {
            float thetaStart = i * stepAngle;
            float thetaEnd = (i + 1) * stepAngle;

            glm::vec3 points[3] =
            {
                {0.0f, topY, 0.0f},
                {std::cos(thetaEnd), bottomY, std::sin(thetaEnd)},
                {std::cos(thetaStart), bottomY, std::sin(thetaStart)},
            };

            glm::vec3 normal = osc::TriangleNormal(points);

            push(points[0], normal);
            push(points[1], normal);
            push(points[2], normal);
        }
    }

    OSC_ASSERT(data.verts.size() % 3 == 0);
    OSC_ASSERT(data.verts.size() == data.normals.size() && data.verts.size() == data.indices.size());

    return CreateMeshFromData(std::move(data));
}

osc::Mesh osc::GenNbyNGrid(size_t n)
{
    static constexpr float z = 0.0f;
    static constexpr float min = -1.0f;
    static constexpr float max = 1.0f;

    float stepSize = (max - min) / static_cast<float>(n);

    size_t nlines = n + 1;

    NewMeshData data;
    data.reserve(4 * nlines);
    data.topography = MeshTopography::Lines;

    uint16_t index = 0;
    auto push = [&index, &data](glm::vec3 const& pos)
    {
        data.verts.push_back(pos);
        data.indices.push_back(index++);
        data.normals.push_back(glm::vec3{0.0f, 0.0f, 1.0f});
    };

    // lines parallel to X axis
    for (size_t i = 0; i < nlines; ++i)
    {
        float y = min + i * stepSize;

        push({-1.0f, y, z});
        push({+1.0f, y, z});
    }

    // lines parallel to Y axis
    for (size_t i = 0; i < nlines; ++i)
    {
        float x = min + i * stepSize;

        push({x, -1.0f, z});
        push({x, +1.0f, z});
    }

    OSC_ASSERT(data.verts.size() % 2 == 0);  // lines, not triangles
    OSC_ASSERT(data.normals.size() == data.verts.size());  // they contain dummy normals
    OSC_ASSERT(data.verts.size() == data.indices.size());

    return CreateMeshFromData(std::move(data));
}

osc::Mesh osc::GenYLine()
{
    NewMeshData data;
    data.verts = {{0.0f, -1.0f, 0.0f}, {0.0f, +1.0f, 0.0f}};
    data.normals = {{0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}};  // just give them *something* in-case they are rendered through a shader that requires normals
    data.indices = {0, 1};
    data.topography = MeshTopography::Lines;

    OSC_ASSERT(data.verts.size() % 2 == 0);
    OSC_ASSERT(data.normals.size() % 2 == 0);
    OSC_ASSERT(data.verts.size() == data.indices.size());

    return CreateMeshFromData(std::move(data));
}

osc::Mesh osc::GenCube()
{
    NewMeshData data;
    data.reserve(g_ShadedTexturedCubeVerts.size());

    uint16_t index = 0;
    for (auto const& v : g_ShadedTexturedCubeVerts)
    {
        data.verts.push_back(v.pos);
        data.normals.push_back(v.norm);
        data.texcoords.push_back(v.uv);
        data.indices.push_back(index++);
    }

    OSC_ASSERT(data.verts.size() % 3 == 0);
    OSC_ASSERT(data.verts.size() == data.normals.size() && data.verts.size() == data.indices.size());

    return CreateMeshFromData(std::move(data));
}

osc::Mesh osc::GenCubeLines()
{
    NewMeshData data;
    data.verts.reserve(g_CubeEdgeLines.size());
    data.indices.reserve(g_CubeEdgeLines.size());
    data.topography = MeshTopography::Lines;

    uint16_t index = 0;
    for (auto const& v : g_CubeEdgeLines)
    {
        data.verts.push_back(v.pos);
        data.indices.push_back(index++);
    }

    OSC_ASSERT(data.verts.size() % 2 == 0);  // lines, not triangles
    OSC_ASSERT(data.normals.empty());
    OSC_ASSERT(data.verts.size() == data.indices.size());

    return CreateMeshFromData(std::move(data));
}

osc::Mesh osc::GenCircle(size_t nsides)
{
    NewMeshData data;
    data.verts.reserve(3*nsides);
    data.topography = MeshTopography::Triangles;

    uint16_t index = 0;
    auto push = [&data, &index](float x, float y, float z)
    {
        data.verts.push_back({x, y, z});
        data.indices.push_back(index++);
    };

    float step = 2.0f*fpi / static_cast<float>(nsides);
    for (size_t i = 0; i < nsides; ++i)
    {
        float theta1 = i * step;
        float theta2 = (i+1) * step;

        push(0.0f, 0.0f, 0.0f);
        push(std::sin(theta1), std::cos(theta1), 0.0f);
        push(std::sin(theta2), std::cos(theta2), 0.0f);
    }

    return CreateMeshFromData(std::move(data));
}

osc::Mesh osc::GenLearnOpenGLCube()
{
    osc::Mesh cube = osc::GenCube();

    std::vector<glm::vec3> verts{cube.getVerts().begin(), cube.getVerts().end()};
    for (glm::vec3& vert : verts)
    {
        vert *= 0.5f;  // makes the verts match LearnOpenGL
    }
    cube.setVerts(verts);

    return cube;
}