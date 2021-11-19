#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/mat4x3.hpp>
#include <glm/mat3x3.hpp>

#include <iosfwd>
#include <array>
#include <cstdint>
#include <cstddef>
#include <vector>

// 3d: low-level 3D rendering primitives based on OpenGL
//
// these are the low-level datastructures/functions used for rendering
// 3D elements in OSC. The renderer is not dependent on SimTK/OpenSim at
// all and has a very low-level view of view of things (verts, drawlists)
namespace osc {


    // VEC STUFF

    // glm printing utilities - handy for debugging
    std::ostream& operator<<(std::ostream&, glm::vec2 const&);
    std::ostream& operator<<(std::ostream&, glm::vec3 const&);
    std::ostream& operator<<(std::ostream&, glm::vec4 const&);
    std::ostream& operator<<(std::ostream&, glm::mat3 const&);
    std::ostream& operator<<(std::ostream&, glm::mat4x3 const&);
    std::ostream& operator<<(std::ostream&, glm::mat4 const&);

    // returns true if the provided vectors are (effectively) at the same location
    bool AreAtSameLocation(glm::vec3 const&, glm::vec3 const&) noexcept;

    // returns a vector containing min(a[dim], b[dim]) for each dimension
    glm::vec3 VecMin(glm::vec3 const&, glm::vec3 const&) noexcept;

    // returns a vector containing min(a[dim], b[dim]) for each dimension
    glm::vec2 VecMin(glm::vec2 const&, glm::vec2 const&) noexcept;

    // returns a vector containing max(a[dim], b[dim]) for each dimension
    glm::vec3 VecMax(glm::vec3 const&, glm::vec3 const&) noexcept;

    // returns a vector containing max(a[dim], b[dim]) for each dimension
    glm::vec2 VecMax(glm::vec2 const&, glm::vec2 const&) noexcept;

    // returns the *index* of a vector's longest dimension
    glm::vec3::length_type VecLongestDimIdx(glm::vec3 const&) noexcept;

    // returns the *index* of a vector's longest dimension
    glm::vec2::length_type VecLongestDimIdx(glm::vec2) noexcept;

    // returns the *index* of a vector's longest dimension
    glm::ivec2::length_type VecLongestDimIdx(glm::ivec2) noexcept;

    // returns the *value* of a vector's longest dimension
    float VecLongestDimVal(glm::vec3 const&) noexcept;

    // returns the *value* of a vector's longest dimension
    float VecLongestDimVal(glm::vec2) noexcept;

    // returns the *value* of a vector's longest dimension
    int VecLongestDimVal(glm::ivec2) noexcept;

    // returns the aspect ratio of the vec (effectively: x/y)
    float VecAspectRatio(glm::ivec2) noexcept;

    // returns the aspect ratio of the vec (effectively: x/y)
    float VecAspectRatio(glm::vec2) noexcept;

    // returns the sum of `n` vectors using the "Kahan Summation Algorithm" to reduce errors
    glm::vec3 VecKahanSum(glm::vec3 const*, size_t n) noexcept;

    // returns the average of `n` vectors using whichever numerically stable average happens to work
    glm::vec3 VecNumericallyStableAverage(glm::vec3 const*, size_t n) noexcept;


    // MATRIX STUFF

    // returns a normal matrix created from the supplied xform matrix
    glm::mat3 NormalMatrix(glm::mat4 const&) noexcept;

    // returns a normal matrix created from the supplied xform matrix
    glm::mat3 NormalMatrix(glm::mat4x3 const&) noexcept;

    // returns matrix that rotates dir1 to point in the same direction as dir2
    glm::mat4 Dir1ToDir2Xform(glm::vec3 const& dir1, glm::vec3 const& dir2) noexcept;


    // ANALYTICAL GEOMETRY STUFF

    // a 3D triangle
    struct Triangle final {
        glm::vec3 p1;
        glm::vec3 p2;
        glm::vec3 p3;
    };

    // a 2D rectangle
    struct Rect final {
        glm::vec2 topLeft;
        glm::vec2 bottomRight;
    };

    // a 3D axis-aligned bounding box (AABB)
    struct AABB final {
        glm::vec3 min;
        glm::vec3 max;
    };

    // a 3D sphere
    struct Sphere final {
        glm::vec3 origin;
        float radius;
    };

    // a 3D ray (an infinitely-long line)
    struct Ray final {
        glm::vec3 origin;
        glm::vec3 dir;
    };

    // a 3D line (a finite-length line)
    struct Line final {
        glm::vec3 p1;
        glm::vec3 p2;
    };

    // a 3D plane
    struct Plane final {
        glm::vec3 origin;
        glm::vec3 normal;
    };

    // a 3D disc (effectively, a 2D circle at some location in 3D space)
    struct Disc final {
        glm::vec3 origin;
        glm::vec3 normal;
        float radius;
    };

    // prints the supplied geometry in a human-readable format
    std::ostream& operator<<(std::ostream&, Triangle const&);
    std::ostream& operator<<(std::ostream&, Rect const&);
    std::ostream& operator<<(std::ostream&, AABB const&);
    std::ostream& operator<<(std::ostream&, Sphere const&);
    std::ostream& operator<<(std::ostream&, Ray const&);
    std::ostream& operator<<(std::ostream&, Plane const&);
    std::ostream& operator<<(std::ostream&, Disc const&);
    std::ostream& operator<<(std::ostream&, Line const&);

    // returns a normal vector of the supplied (pointed to) triangle (i.e. (v[1]-v[0]) x (v[2]-v[0]))
    glm::vec3 TriangleNormal(Triangle const&) noexcept;

    // returns the rect's X and Y dimensions
    glm::vec2 RectDims(Rect const&) noexcept;

    // returns the rect's aspect ratio (effectively, RectDims(X)/RectDims(Y))
    float RectAspectRatio(Rect const&) noexcept;

    // returns true if the provided point is within the Rect
    bool PointIsInRect(Rect const&, glm::vec2 const&) noexcept;

    // returns the center-point of the AABB
    glm::vec3 AABBCenter(AABB const&) noexcept;

    // returns the 3D dimensions of the AABB
    glm::vec3 AABBDims(AABB const&) noexcept;

    // returns the smallest AABB that spans both of the provided AABBs
    AABB AABBUnion(AABB const&, AABB const&) noexcept;

    // advanced: returns the smallest AABB that spans all of the provided AABBs
    //
    // `offset`: offset in bytes from the start of `data` where AABB is
    // `stride`: stride between the `n` AABB instances
    AABB AABBUnion(void const* data, size_t n, size_t stride, size_t offset);

    // returns true if the AABB has an effective volume of 0
    bool AABBIsEmpty(AABB const&) noexcept;

    // returns the *index* of the longest dimension of an AABB
    glm::vec3::length_type AABBLongestDimIdx(AABB const&) noexcept;

    // returns the *length* of the longest dimension of an AABB
    float AABBLongestDim(AABB const&) noexcept;

    // returns the eight corner points of the cuboid representation of the AABB
    std::array<glm::vec3, 8> AABBVerts(AABB const&) noexcept;

    // returns an AABB that spans the provided AABB after applying the transform
    AABB AABBApplyXform(AABB const&, glm::mat4 const&) noexcept;

    // returns an AABB that spans the provided 3D vertices
    AABB AABBFromVerts(glm::vec3 const*, size_t n) noexcept;

    // returns a sphere that contains all of the provided verts
    Sphere SphereFromVerts(glm::vec3 const*, size_t n) noexcept;

    // returns an AABB that contains the provided sphere
    AABB SphereToAABB(Sphere const&) noexcept;

    // returns an xform that maps an origin centered r=1 sphere into an in-scene sphere
    glm::mat4 SphereXform(Sphere const&) noexcept;

    // returns an xform that maps a sphere to another sphere
    glm::mat4 SphereToSphereXform(Sphere const&, Sphere const&) noexcept;

    // returns a ray that has been transformed by the provided transform matrix
    Ray RayApplyXform(Ray const&, glm::mat4 const&) noexcept;

    // returns an xform that maps a disc to another disc
    glm::mat4 DiscToDiscXform(Disc const&, Disc const&) noexcept;

    // returns an xform that maps a path segment to another path segment
    glm::mat4 SegmentToSegmentXform(Line const&, Line const&) noexcept;


    // COLLISION STUFF (depends on analytical geometry)

    // a 3D collision between a ray and a piece of geometry
    struct RayCollision final {
        bool hit;
        float distance;

        operator bool () {
            return hit;
        }
    };

    // collision tests
    RayCollision GetRayCollisionSphere(Ray const&, Sphere const&) noexcept;
    RayCollision GetRayCollisionAABB(Ray const&, AABB const&) noexcept;
    RayCollision GetRayCollisionPlane(Ray const&, Plane const&) noexcept;
    RayCollision GetRayCollisionDisc(Ray const&, Disc const&) noexcept;
    RayCollision GetRayCollisionTriangle(Ray const&, glm::vec3 const*) noexcept;


    // COLOR STUFF

    struct Rgba32 final {
        unsigned char r;
        unsigned char g;
        unsigned char b;
        unsigned char a;
    };

    struct Rgb24 final {
        unsigned char r;
        unsigned char g;
        unsigned char b;
    };

    // float-/double-based inputs assume linear color range (i.e. 0 to 1)
    Rgba32 Rgba32FromVec4(glm::vec4 const&) noexcept;
    Rgba32 Rgba32FromF4(float, float, float, float) noexcept;
    Rgba32 Rgba32FromU32(uint32_t) noexcept;  // R at MSB


    // MESH STUFF

    enum class MeshTopography {
        Triangles,
        Lines,
    };

    // CPU-side mesh data
    //
    // These can be generated/manipulated on any CPU core without having to worry
    // about the GPU
    //
    // see `Mesh` for the GPU-facing and user-friendly version of this. The separation
    // between mesh data and meshes exists because the algs in this header are supposed
    // to be simple and portable, rather than fast and usable, so that lower-level
    // CPU-only code can use these without having to worry about which GPU API is active,
    // buffer packing, etc.
    struct MeshData {
        std::vector<glm::vec3> verts;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texcoords;
        std::vector<uint32_t> indices;
        MeshTopography topography = MeshTopography::Triangles;

        void clear();
        void reserve(size_t);
    };

    // prints top-level mesh information (eg amount of each thing) to the stream
    std::ostream& operator<<(std::ostream&, MeshData const&);

    // generates a textured quad with:
    //
    // - positions: Z == 0, X == [-1, 1], and Y == [-1, 1]
    // - texcoords: (0, 0) to (1, 1)
    MeshData GenTexturedQuad();

    // generates UV sphere centered at (0,0,0) with radius = 1
    MeshData GenUntexturedUVSphere(size_t sectors, size_t stacks);

    // generates a "Simbody" cylinder, where the bottom/top are -1.0f/+1.0f in Y
    MeshData GenUntexturedSimbodyCylinder(size_t nsides);

    // generates a "Simbody" cone, where the bottom/top are -1.0f/+1.0f in Y
    MeshData GenUntexturedSimbodyCone(size_t nsides);

    // generates 2D grid lines at Z == 0, X/Y == [-1,+1]
    MeshData GenNbyNGrid(size_t nticks);

    // generates a single two-point line from (0,-1,0) to (0,+1,0)
    MeshData GenYLine();

    // generates a cube with [-1,+1] in each dimension
    MeshData GenCube();

    // generates the *lines* of a cube with [-1,+1] in each dimension
    MeshData GenCubeLines();

    // generates a circle at Z == 0, X/Y == [-1, +1] (r = 1)
    MeshData GenCircle(size_t nsides);


    // CAMERA/VIEWPORT STUFF

    // converts a topleft-origin RELATIVE `pos` (0 to 1 in XY starting topleft) into an
    // XY location in NDC (-1 to +1 in XY starting in the middle)
    glm::vec2 TopleftRelPosToNDCPoint(glm::vec2 relpos);

    // converts a topleft-origin RELATIVE `pos` (0 to 1 in XY, starting topleft) into
    // the equivalent POINT on the front of the NDC cube (i.e. "as if" a viewer was there)
    //
    // i.e. {X_ndc, Y_ndc, -1.0f, 1.0f}
    glm::vec4 TopleftRelPosToNDCCube(glm::vec2 relpos);

    // a camera that focuses on and swivels around a focal point (e.g. for 3D model viewers)
    struct PolarPerspectiveCamera final {
        float radius;
        float theta;
        float phi;
        glm::vec3 focusPoint;
        float fov;
        float znear;
        float zfar;

        PolarPerspectiveCamera();

        // reset the camera to its initial state
        void reset();

        // note: relative deltas here are relative to whatever "screen" the camera
        // is handling.
        //
        // e.g. moving a mouse 400px in X in a screen that is 800px wide should
        //      have a delta.x of 0.5f

        // pan: pan along the current view plane
        void pan(float aspectRatio, glm::vec2 mouseDelta) noexcept;

        // drag: spin the view around the origin, such that the distance between
        //       the camera and the origin remains constant
        void drag(glm::vec2 mouseDelta) noexcept;

        // autoscale znear and zfar based on the camera's distance from what it's looking at
        //
        // important for looking at extremely small/large scenes. znear and zfar dictates
        // both the culling planes of the camera *and* rescales the Z values of elements
        // in the scene. If the znear-to-zfar range is too large then Z-fighting will happen
        // and the scene will look wrong.
        void rescaleZNearAndZFarBasedOnRadius() noexcept;

        glm::mat4 getViewMtx() const noexcept;
        glm::mat4 getProjMtx(float aspect_ratio) const noexcept;

        // project's a worldspace coordinate onto a screen-space rectangle
        glm::vec2 projectOntoScreenRect(glm::vec3 const& worldspaceLoc, Rect const& screenRect) const noexcept;

        glm::vec3 getPos() const noexcept;

        // converts a `pos` (top-left) in the output `dims` into a line in worldspace by unprojection
        Ray unprojectTopLeftPosToWorldRay(glm::vec2 pos, glm::vec2 dims) const noexcept;
    };

    // camera that moves freely through space (e.g. FPS games)
    struct EulerPerspectiveCamera final {
        glm::vec3 pos;
        float pitch;
        float yaw;
        float fov;
        float znear;
        float zfar;

        EulerPerspectiveCamera();

        [[nodiscard]] glm::vec3 getFront() const noexcept;
        [[nodiscard]] glm::vec3 getUp() const noexcept;
        [[nodiscard]] glm::vec3 getRight() const noexcept;
        [[nodiscard]] glm::mat4 getViewMtx() const noexcept;
        [[nodiscard]] glm::mat4 getProjMtx(float aspectRatio) const noexcept;
    };
}
