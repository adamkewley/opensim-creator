#include "FrameDefinitionTab.hpp"

#include "OpenSimCreator/Bindings/SimTKHelpers.hpp"
#include "OpenSimCreator/Bindings/SimTKMeshLoader.hpp"
#include "OpenSimCreator/Graphics/CustomRenderingOptions.hpp"
#include "OpenSimCreator/Graphics/OpenSimDecorationOptions.hpp"
#include "OpenSimCreator/Graphics/OpenSimDecorationGenerator.hpp"
#include "OpenSimCreator/Graphics/OverlayDecorationGenerator.hpp"
#include "OpenSimCreator/Graphics/OpenSimGraphicsHelpers.hpp"
#include "OpenSimCreator/MiddlewareAPIs/EditorAPI.hpp"
#include "OpenSimCreator/Model/UndoableModelStatePair.hpp"
#include "OpenSimCreator/Panels/ModelEditorViewerPanel.hpp"
#include "OpenSimCreator/Panels/ModelEditorViewerPanelLayer.hpp"
#include "OpenSimCreator/Panels/ModelEditorViewerPanelParameters.hpp"
#include "OpenSimCreator/Panels/ModelEditorViewerPanelRightClickEvent.hpp"
#include "OpenSimCreator/Panels/ModelEditorViewerPanelState.hpp"
#include "OpenSimCreator/Panels/NavigatorPanel.hpp"
#include "OpenSimCreator/Panels/PropertiesPanel.hpp"
#include "OpenSimCreator/Utils/OpenSimHelpers.hpp"
#include "OpenSimCreator/Utils/UndoableModelActions.hpp"
#include "OpenSimCreator/Widgets/BasicWidgets.hpp"
#include "OpenSimCreator/Widgets/MainMenu.hpp"

#include <oscar/Bindings/ImGuiHelpers.hpp>
#include <oscar/Formats/OBJ.hpp>
#include <oscar/Graphics/Color.hpp>
#include <oscar/Graphics/GraphicsHelpers.hpp>
#include <oscar/Graphics/MeshCache.hpp>
#include <oscar/Graphics/SceneDecoration.hpp>
#include <oscar/Graphics/SceneRenderer.hpp>
#include <oscar/Graphics/SceneRendererParams.hpp>
#include <oscar/Graphics/ShaderCache.hpp>
#include <oscar/Maths/BVH.hpp>
#include <oscar/Maths/MathHelpers.hpp>
#include <oscar/Maths/Rect.hpp>
#include <oscar/Panels/LogViewerPanel.hpp>
#include <oscar/Panels/Panel.hpp>
#include <oscar/Panels/PanelManager.hpp>
#include <oscar/Panels/StandardPanel.hpp>
#include <oscar/Platform/App.hpp>
#include <oscar/Platform/Log.hpp>
#include <oscar/Platform/os.hpp>
#include <oscar/Utils/Algorithms.hpp>
#include <oscar/Utils/Assertions.hpp>
#include <oscar/Utils/CStringView.hpp>
#include <oscar/Utils/FilesystemHelpers.hpp>
#include <oscar/Utils/UID.hpp>
#include <oscar/Widgets/Popup.hpp>
#include <oscar/Widgets/PopupManager.hpp>
#include <oscar/Widgets/StandardPopup.hpp>
#include <oscar/Widgets/WindowMenu.hpp>
#include <OscarConfiguration.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <OpenSim/Common/Component.h>
#include <OpenSim/Common/ComponentPath.h>
#include <OpenSim/Common/ComponentSocket.h>
#include <OpenSim/Simulation/Model/Model.h>
#include <OpenSim/Simulation/Model/ModelComponent.h>
#include <OpenSim/Simulation/Model/PhysicalOffsetFrame.h>
#include <SDL_events.h>
#include <SimTKcommon/internal/DecorativeGeometry.h>

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

// top-level constants
namespace
{
    constexpr osc::CStringView c_TabStringID = "OpenSim/Experimental/FrameDefinition";
    constexpr double c_SphereDefaultRadius = 0.01;
    constexpr osc::Color c_SphereDefaultColor = {1.0f, 1.0f, 0.75f};
    constexpr osc::Color c_MidpointDefaultColor = {0.75f, 1.0f, 1.0f};
    constexpr osc::Color c_PointToPointEdgeDefaultColor = {0.75f, 1.0f, 1.0f};
    constexpr osc::Color c_CrossProductEdgeDefaultColor = {0.75f, 1.0f, 1.0f};
}

// helper functions
namespace
{
    // returns the ground-based location re-expressed w.r.t. the given frame
    SimTK::Vec3 CalcLocationInFrame(
        OpenSim::Frame const& frame,
        SimTK::State const& state,
        glm::vec3 const& locationInGround)
    {
        SimTK::Transform const mesh2ground = frame.getTransformInGround(state);
        SimTK::Transform const ground2mesh = mesh2ground.invert();
        SimTK::Vec3 const translationInGround = osc::ToSimTKVec3(locationInGround);
        return ground2mesh * translationInGround;
    }

    // returns the RGB components of `color`
    SimTK::Vec3 ToRGBVec3(osc::Color const& color)
    {
        return {color.r, color.g, color.b};
    }

    // sets the appearance of `geometry` (SimTK) from `appearance` (OpenSim)
    void SetGeomAppearance(
        SimTK::DecorativeGeometry& geometry,
        OpenSim::Appearance const& appearance)
    {
        geometry.setColor(appearance.get_color());
        geometry.setOpacity(appearance.get_opacity());
        if (appearance.get_visible())
        {
            geometry.setRepresentation(appearance.get_representation());
        }
        else
        {
            geometry.setRepresentation(SimTK::DecorativeGeometry::Hide);
        }
    }

    // sets the color and opacity of `appearance` from `color`
    void SetColorAndOpacity(
        OpenSim::Appearance& appearance,
        osc::Color const& color)
    {
        appearance.set_color(ToRGBVec3(color));
        appearance.set_opacity(color.a);
    }

    // returns a decorative sphere with `radius`, `position`, and `appearance`
    SimTK::DecorativeSphere CreateDecorativeSphere(
        double radius,
        SimTK::Vec3 position,
        OpenSim::Appearance const& appearance)
    {
        SimTK::DecorativeSphere sphere{radius};
        sphere.setTransform(SimTK::Transform{position});
        SetGeomAppearance(sphere, appearance);
        return sphere;
    }

    // returns a decorative line between `startPosition` and `endPosition` with `appearance`
    SimTK::DecorativeLine CreateDecorativeLine(
        SimTK::Vec3 const& startPosition,
        SimTK::Vec3 const& endPosition,
        OpenSim::Appearance const& appearance)
    {
        SimTK::DecorativeLine line{startPosition, endPosition};
        SetGeomAppearance(line, appearance);
        return line;
    }

    // returns a decorative arrow between `startPosition` and `endPosition` with `appearance`
    SimTK::DecorativeArrow CreateDecorativeArrow(
        SimTK::Vec3 const& startPosition,
        SimTK::Vec3 const& endPosition,
        OpenSim::Appearance const& appearance)
    {
        SimTK::DecorativeArrow arrow{startPosition, endPosition, 1.75 * c_SphereDefaultRadius};
        arrow.setLineThickness(0.5 * c_SphereDefaultRadius);
        SetGeomAppearance(arrow, appearance);
        return arrow;
    }

    // returns a decorative frame based on the provided transform
    SimTK::DecorativeFrame CreateDecorativeFrame(
        SimTK::Transform const& transformInGround)
    {
        // adapted from OpenSim::FrameGeometry (Geometry.cpp)
        SimTK::DecorativeFrame frame(1.0);
        frame.setTransform(transformInGround);
        frame.setScale(0.2);
        frame.setLineThickness(0.004);
        return frame;
    }

    // returns a SimTK::DecorativeMesh reperesentation of the parallelogram formed between
    // two (potentially disconnected) edges, starting at `origin`
    SimTK::DecorativeMesh CreateParallelogramMesh(
        SimTK::Vec3 const& origin,
        SimTK::Vec3 const& firstEdge,
        SimTK::Vec3 const& secondEdge,
        OpenSim::Appearance const& appearance)
    {
        SimTK::PolygonalMesh polygonalMesh;
        {
            std::array<SimTK::Vec3, 4> const verts
            {
                origin,
                origin + firstEdge,
                origin + firstEdge + secondEdge,
                origin + secondEdge,
            };

            SimTK::Array_<int> face;
            face.reserve(static_cast<unsigned int>(verts.size()));
            for (SimTK::Vec3 const& vert : verts)
            {
                face.push_back(polygonalMesh.addVertex(vert));
            }
            polygonalMesh.addFace(face);
        }

        SimTK::DecorativeMesh rv{std::move(polygonalMesh)};
        SetGeomAppearance(rv, appearance);
        return rv;
    }

    // custom helper that customizes the OpenSim model defaults to be more
    // suitable for the frame definition UI
    std::shared_ptr<osc::UndoableModelStatePair> MakeSharedUndoableFrameDefinitionModel()
    {
        auto model = std::make_unique<OpenSim::Model>();
        model->updDisplayHints().set_show_frames(true);
        return std::make_shared<osc::UndoableModelStatePair>(std::move(model));
    }

    // gets the next unique suffix numer for geometry
    int32_t GetNextGlobalGeometrySuffix()
    {
        static std::atomic<int32_t> s_GeometryCounter = 0;
        return s_GeometryCounter++;
    }

    // returns a scene element name that should
    std::string GenerateSceneElementName(std::string_view prefix)
    {
        std::stringstream ss;
        ss << prefix;
        ss << GetNextGlobalGeometrySuffix();
        return std::move(ss).str();
    }

    // returns an appropriate commit message for adding `somethingName` to a model
    std::string GenerateAddedSomethingCommitMessage(std::string_view somethingName)
    {
        std::string_view const prefix = "added ";
        std::string rv;
        rv.reserve(prefix.size() + somethingName.size());
        rv += prefix;
        rv += somethingName;
        return rv;
    }

    // mutates the given render params to match the style of the frame definition UI
    void SetupDefault3DViewportRenderingParams(osc::ModelRendererParams& renderParams)
    {
        renderParams.renderingOptions.setDrawFloor(false);
        renderParams.overlayOptions.setDrawXZGrid(true);
        renderParams.backgroundColor = {48.0f/255.0f, 48.0f/255.0f, 48.0f/255.0f, 1.0f};
    }
}

// custom OpenSim components for this screen
namespace OpenSim
{
    // HACK: OpenSim namespace is REQUIRED
    //
    // because OpenSim's property macros etc. assume so much, see:
    //
    //  - https://github.com/opensim-org/opensim-core/pull/3469

    // returns `true` if the given component is a point in the frame definition scene
    bool IsPoint(OpenSim::Component const& component)
    {
        return dynamic_cast<OpenSim::Point const*>(&component);
    }

    // a sphere landmark, where the center of the sphere is the point of interest
    class SphereLandmark final : public OpenSim::Station {
        OpenSim_DECLARE_CONCRETE_OBJECT(SphereLandmark, OpenSim::Station)
    public:
        OpenSim_DECLARE_PROPERTY(radius, double, "The radius of the sphere (decorative)");
        OpenSim_DECLARE_UNNAMED_PROPERTY(Appearance, "The appearance of the sphere (decorative)");

        SphereLandmark()
        {
            constructProperty_radius(c_SphereDefaultRadius);
            constructProperty_Appearance(Appearance{});
            SetColorAndOpacity(upd_Appearance(), c_SphereDefaultColor);
        }

        void generateDecorations(
            bool,
            const ModelDisplayHints&,
            const SimTK::State& state,
            SimTK::Array_<SimTK::DecorativeGeometry>& appendOut) const final
        {
            appendOut.push_back(CreateDecorativeSphere(
                get_radius(),
                getLocationInGround(state),
                get_Appearance()
            ));
        }
    };

    // a landmark defined as a point between two other points
    class MidpointLandmark final : public OpenSim::Point {
        OpenSim_DECLARE_CONCRETE_OBJECT(MidpointLandmark, OpenSim::Point)
    public:
        OpenSim_DECLARE_PROPERTY(radius, double, "The radius of the midpoint (decorative)");
        OpenSim_DECLARE_UNNAMED_PROPERTY(Appearance, "The appearance of the midpoint (decorative)");
        OpenSim_DECLARE_SOCKET(pointA, OpenSim::Point, "The first point that the midpoint is between");
        OpenSim_DECLARE_SOCKET(pointB, OpenSim::Point, "The second point that the midpoint is between");

        MidpointLandmark()
        {
            constructProperty_radius(c_SphereDefaultRadius);
            constructProperty_Appearance(Appearance{});
            SetColorAndOpacity(upd_Appearance(), c_MidpointDefaultColor);
        }

        void generateDecorations(
            bool,
            const ModelDisplayHints&,
            const SimTK::State& state,
            SimTK::Array_<SimTK::DecorativeGeometry>& appendOut) const final
        {
            appendOut.push_back(CreateDecorativeSphere(
                get_radius(),
                getLocationInGround(state),
                get_Appearance()
            ));
        }

    private:
        SimTK::Vec3 calcLocationInGround(const SimTK::State& state) const final
        {
            SimTK::Vec3 const pointALocation = getConnectee<OpenSim::Point>("pointA").getLocationInGround(state);
            SimTK::Vec3 const pointBLocation = getConnectee<OpenSim::Point>("pointB").getLocationInGround(state);
            return 0.5*(pointALocation + pointBLocation);
        }

        SimTK::Vec3 calcVelocityInGround(const SimTK::State& state) const final
        {
            SimTK::Vec3 const pointAVelocity = getConnectee<OpenSim::Point>("pointA").getVelocityInGround(state);
            SimTK::Vec3 const pointBVelocity = getConnectee<OpenSim::Point>("pointB").getVelocityInGround(state);
            return 0.5*(pointAVelocity + pointBVelocity);
        }

        SimTK::Vec3 calcAccelerationInGround(const SimTK::State& state) const final
        {
            SimTK::Vec3 const pointAAcceleration = getConnectee<OpenSim::Point>("pointA").getAccelerationInGround(state);
            SimTK::Vec3 const pointBAcceleration = getConnectee<OpenSim::Point>("pointB").getAccelerationInGround(state);
            return 0.5*(pointAAcceleration + pointBAcceleration);
        }
    };

    // the start and end locations of an edge in 3D space
    struct EdgePoints final {
        SimTK::Vec3 start;
        SimTK::Vec3 end;
    };

    // returns the direction vector between the `start` and `end` points
    SimTK::UnitVec3 CalcDirection(EdgePoints const& a)
    {
        return SimTK::UnitVec3{a.end - a.start};
    }

    // returns points for an edge that:
    //
    // - originates at `a.start`
    // - points in the direction of `a x b`
    // - has a magnitude of min(|a|, |b|) - handy for rendering
    EdgePoints CrossProduct(EdgePoints const& a, EdgePoints const& b)
    {
        // TODO: if cross product isn't possible (e.g. angle between vectors is zero)
        // then this needs to fail or fallback
        SimTK::Vec3 const firstEdge = a.end - a.start;
        SimTK::Vec3 const secondEdge = b.end - b.start;
        SimTK::Vec3 const resultEdge = SimTK::cross(firstEdge, secondEdge).normalize();
        double const resultEdgeLength = std::min(firstEdge.norm(), secondEdge.norm());

        return {a.start, a.start + (resultEdgeLength*resultEdge)};
    }

    // virtual base class for an edge that starts at one location in ground and ends at
    // some other location in ground
    class FDVirtualEdge : public ModelComponent {
        OpenSim_DECLARE_ABSTRACT_OBJECT(FDVirtualEdge, ModelComponent)
    public:
        EdgePoints getEdgePointsInGround(SimTK::State const& state) const
        {
            return implGetEdgePointsInGround(state);
        }
    private:
        virtual EdgePoints implGetEdgePointsInGround(SimTK::State const&) const = 0;
    };

    bool IsEdge(OpenSim::Component const& component)
    {
        return dynamic_cast<OpenSim::FDVirtualEdge const*>(&component);
    }

    // an edge that starts at virtual `pointA` and ends at virtual `pointB`
    class FDPointToPointEdge final : public FDVirtualEdge {
        OpenSim_DECLARE_CONCRETE_OBJECT(FDPointToPointEdge, FDVirtualEdge)
    public:
        OpenSim_DECLARE_UNNAMED_PROPERTY(Appearance, "The appearance of the edge (decorative)");
        OpenSim_DECLARE_SOCKET(pointA, Point, "The first point that the edge is connected to");
        OpenSim_DECLARE_SOCKET(pointB, Point, "The second point that the edge is connected to");

        FDPointToPointEdge()
        {
            constructProperty_Appearance(Appearance{});
            SetColorAndOpacity(upd_Appearance(), c_PointToPointEdgeDefaultColor);
        }

        void generateDecorations(
            bool,
            const ModelDisplayHints&,
            const SimTK::State& state,
            SimTK::Array_<SimTK::DecorativeGeometry>& appendOut) const final
        {
            EdgePoints const coords = getEdgePointsInGround(state);

            appendOut.push_back(CreateDecorativeArrow(
                coords.start,
                coords.end,
                get_Appearance()
            ));
        }

    private:
        EdgePoints implGetEdgePointsInGround(SimTK::State const& state) const final
        {
            OpenSim::Point const& pointA = getConnectee<OpenSim::Point>("pointA");
            SimTK::Vec3 const pointAGroundLoc = pointA.getLocationInGround(state);

            OpenSim::Point const& pointB = getConnectee<OpenSim::Point>("pointB");
            SimTK::Vec3 const pointBGroundLoc = pointB.getLocationInGround(state);

            return {pointAGroundLoc, pointBGroundLoc};
        }
    };

    // an edge that is computed from `edgeA x edgeB`
    //
    // - originates at `a.start`
    // - points in the direction of `a x b`
    // - has a magnitude of min(|a|, |b|) - handy for rendering
    class FDCrossProductEdge final : public FDVirtualEdge {
        OpenSim_DECLARE_CONCRETE_OBJECT(FDCrossProductEdge, FDVirtualEdge)
    public:
        OpenSim_DECLARE_PROPERTY(showPlane, bool, "Whether to show the plane of the two edges the cross product was created from (decorative)");
        OpenSim_DECLARE_UNNAMED_PROPERTY(Appearance, "The appearance of the edge (decorative)");
        OpenSim_DECLARE_SOCKET(edgeA, FDVirtualEdge, "The first edge parameter to the cross product calculation");
        OpenSim_DECLARE_SOCKET(edgeB, FDVirtualEdge, "The second edge parameter to the cross product calculation");

        FDCrossProductEdge()
        {
            constructProperty_showPlane(false);
            constructProperty_Appearance(Appearance{});
            SetColorAndOpacity(upd_Appearance(), c_CrossProductEdgeDefaultColor);
        }

        void generateDecorations(
            bool,
            const ModelDisplayHints&,
            const SimTK::State& state,
            SimTK::Array_<SimTK::DecorativeGeometry>& appendOut) const final
        {
            EdgePoints const coords = getEdgePointsInGround(state);

            // draw edge
            appendOut.push_back(CreateDecorativeArrow(
                coords.start,
                coords.end,
                get_Appearance()
            ));

            // if requested, draw a parallelogram from the two edges
            if (get_showPlane())
            {
                auto const [aPoints, bPoints] = getBothEdgePoints(state);
                appendOut.push_back(CreateParallelogramMesh(
                    coords.start,
                    aPoints.end - aPoints.start,
                    bPoints.end - bPoints.start,
                    get_Appearance()
                ));
            }
        }

    private:
        std::pair<EdgePoints, EdgePoints> getBothEdgePoints(SimTK::State const& state) const
        {
            return
            {
                getConnectee<FDVirtualEdge>("edgeA").getEdgePointsInGround(state),
                getConnectee<FDVirtualEdge>("edgeB").getEdgePointsInGround(state),
            };
        }

        EdgePoints implGetEdgePointsInGround(SimTK::State const& state) const final
        {
            std::pair<EdgePoints, EdgePoints> const edgePoints = getBothEdgePoints(state);
            return  CrossProduct(edgePoints.first, edgePoints.second);
        }
    };

    // enumeration of the possible axes a user may define
    enum class AxisIndex : int32_t {
        X = 0,
        Y,
        Z,
        TOTAL
    };

    // returns the next `AxisIndex` in the circular sequence X -> Y -> Z
    constexpr AxisIndex Next(AxisIndex axis)
    {
        return static_cast<AxisIndex>((static_cast<int32_t>(axis) + 1) % static_cast<int32_t>(AxisIndex::TOTAL));
    }
    static_assert(Next(AxisIndex::X) == AxisIndex::Y);
    static_assert(Next(AxisIndex::Y) == AxisIndex::Z);
    static_assert(Next(AxisIndex::Z) == AxisIndex::X);

    // returns a char representation of the given `AxisIndex`
    char ToChar(AxisIndex axis)
    {
        static_assert(static_cast<size_t>(AxisIndex::TOTAL) == 3);
        switch (axis)
        {
        case AxisIndex::X:
            return 'x';
        case AxisIndex::Y:
            return 'y';
        case AxisIndex::Z:
        default:
            return 'z';
        }
    }

    // returns `c` parsed as an `AxisIndex`, or `std::nullopt` if the given char
    // cannot be parsed as an axis index
    std::optional<AxisIndex> ParseAxisIndex(char c)
    {
        switch (c)
        {
        case 'x':
        case 'X':
            return AxisIndex::X;
        case 'y':
        case 'Y':
            return AxisIndex::Y;
        case 'z':
        case 'Z':
            return AxisIndex::Z;
        default:
            return std::nullopt;
        }
    }

    // returns the integer index equivalent of the given `AxisIndex`
    ptrdiff_t ToIndex(AxisIndex axis)
    {
        return static_cast<ptrdiff_t>(axis);
    }

    // the potentially negated index of an axis in n-dimensional space
    struct MaybeNegatedAxis final {
        MaybeNegatedAxis(
            AxisIndex axisIndex_,
            bool isNegated_) :

            axisIndex{axisIndex_},
            isNegated{isNegated_}
        {
        }
        AxisIndex axisIndex;
        bool isNegated;
    };

    MaybeNegatedAxis Next(MaybeNegatedAxis ax)
    {
        return MaybeNegatedAxis{Next(ax.axisIndex), ax.isNegated};
    }

    // returns `true` if the arguments are orthogonal to eachover; otherwise, returns `false`
    bool IsOrthogonal(MaybeNegatedAxis const& a, MaybeNegatedAxis const& b)
    {
        return a.axisIndex != b.axisIndex;
    }

    // returns a (possibly negated) `AxisIndex` parsed from the given input
    //
    // if the input is invalid in some way, returns `std::nullopt`
    std::optional<MaybeNegatedAxis> ParseAxisDimension(std::string_view s)
    {
        if (s.empty())
        {
            return std::nullopt;
        }

        // handle and consume sign prefix
        bool const isNegated = s.front() == '-';
        if (isNegated || s.front() == '+')
        {
            s = s.substr(1);
        }

        if (s.empty())
        {
            return std::nullopt;
        }

        // handle axis suffix
        std::optional<AxisIndex> const maybeAxisIndex = ParseAxisIndex(s.front());
        if (!maybeAxisIndex)
        {
            return std::nullopt;
        }

        return MaybeNegatedAxis{*maybeAxisIndex, isNegated};
    }

    // returns a string representation of the given (possibly negated) axis
    std::string ToString(MaybeNegatedAxis const& ax)
    {
        std::string rv;
        rv.reserve(2);
        rv.push_back(ax.isNegated ? '-' : '+');
        rv.push_back(ToChar(ax.axisIndex));
        return rv;
    }

    // a frame that is defined by:
    //
    // - an "axis" edge
    // - a designation of what axis the "axis" edge lies along
    // - an "other" edge, which should be non-parallel to the "axis" edge
    // - a desgination of what axis the cross product `axis x other` lies along
    // - an "origin" point, which is where the origin of the frame should be defined
    class LandmarkDefinedFrame final : public OpenSim::PhysicalFrame {
        OpenSim_DECLARE_CONCRETE_OBJECT(LandmarkDefinedFrame, Frame)
    public:
        OpenSim_DECLARE_SOCKET(axisEdge, FDVirtualEdge, "The edge from which to create the first axis");
        OpenSim_DECLARE_SOCKET(otherEdge, FDVirtualEdge, "Some other edge that is non-parallel to `axisEdge` and can be used (via a cross product) to define the frame");
        OpenSim_DECLARE_SOCKET(origin, OpenSim::Point, "The origin (position) of the frame");
        OpenSim_DECLARE_PROPERTY(axisEdgeDimension, std::string, "The dimension to assign to `axisEdge`. Can be -x, +x, -y, +y, -z, or +z");
        OpenSim_DECLARE_PROPERTY(secondAxisDimension, std::string, "The dimension to assign to the second axis that is generated from the cross-product of `axisEdge` with `otherEdge`. Can be -x, +x, -y, +y, -z, or +z and must be orthogonal to `axisEdgeDimension`");
        OpenSim_DECLARE_PROPERTY(forceShowingFrame, bool, "Whether to forcibly show the frame's decoration, even if showing frames is disabled at the model-level (decorative)");

        LandmarkDefinedFrame()
        {
            constructProperty_axisEdgeDimension("+x");
            constructProperty_secondAxisDimension("+y");
            constructProperty_forceShowingFrame(true);
        }

    private:
        void generateDecorations(
            bool fixed,
            const ModelDisplayHints& hints,
            const SimTK::State& state,
            SimTK::Array_<SimTK::DecorativeGeometry>& appendOut) const final
        {
            if (get_forceShowingFrame() ||
                getModel().get_ModelVisualPreferences().get_ModelDisplayHints().get_show_frames())
            {
                appendOut.push_back(CreateDecorativeFrame(
                    getTransformInGround(state)
                ));
            }
        }

        void extendFinalizeFromProperties() final
        {
            Super::extendFinalizeFromProperties();
            tryParseAxisArgumentsAsOrthogonalAxes();  // throws on error
        }

        struct ParsedAxisArguments final {
            MaybeNegatedAxis axisEdge;
            MaybeNegatedAxis otherEdge;
        };
        ParsedAxisArguments tryParseAxisArgumentsAsOrthogonalAxes() const
        {
            // ensure `axisEdge` is a correct property value
            std::optional<MaybeNegatedAxis> const maybeAxisEdge = ParseAxisDimension(get_axisEdgeDimension());
            if (!maybeAxisEdge)
            {
                std::stringstream ss;
                ss << getProperty_axisEdgeDimension().getName() << ": has an invalid value ('" << get_axisEdgeDimension() << "'): permitted values are -x, +x, -y, +y, -z, or +z";
                OPENSIM_THROW_FRMOBJ(OpenSim::Exception, std::move(ss).str());
            }
            MaybeNegatedAxis const& axisEdge = *maybeAxisEdge;

            // ensure `otherEdge` is a correct property value
            std::optional<MaybeNegatedAxis> const maybeOtherEdge = ParseAxisDimension(get_secondAxisDimension());
            if (!maybeOtherEdge)
            {
                std::stringstream ss;
                ss << getProperty_secondAxisDimension().getName() << ": has an invalid value ('" << get_secondAxisDimension() << "'): permitted values are -x, +x, -y, +y, -z, or +z";
                OPENSIM_THROW_FRMOBJ(OpenSim::Exception, std::move(ss).str());
            }
            MaybeNegatedAxis const& otherEdge = *maybeOtherEdge;

            // ensure `axisEdge` is orthogonal to `otherEdge`
            if (!IsOrthogonal(axisEdge, otherEdge))
            {
                std::stringstream ss;
                ss << getProperty_axisEdgeDimension().getName() << " (" << get_axisEdgeDimension() << ") and " << getProperty_secondAxisDimension().getName() << " (" << get_secondAxisDimension() << ") are not orthogonal";
                OPENSIM_THROW_FRMOBJ(OpenSim::Exception, std::move(ss).str());
            }

            return ParsedAxisArguments{axisEdge, otherEdge};
        }

        SimTK::Transform calcTransformInGround(SimTK::State const& state) const final
        {
            // parse axis properties
            auto const [axisEdge, otherEdge] = tryParseAxisArgumentsAsOrthogonalAxes();

            // get other edges/points via sockets
            SimTK::UnitVec3 const axisEdgeDir =
                CalcDirection(getConnectee<FDVirtualEdge>("axisEdge").getEdgePointsInGround(state));
            SimTK::UnitVec3 const otherEdgeDir =
                CalcDirection(getConnectee<FDVirtualEdge>("otherEdge").getEdgePointsInGround(state));
            SimTK::Vec3 const originLocationInGround =
                getConnectee<OpenSim::Point>("origin").getLocationInGround(state);

            // this is what the algorithm must ultimately compute in order to
            // calculate a change-of-basis (rotation) matrix
            std::array<SimTK::UnitVec3, 3> axes{};
            static_assert(axes.size() == static_cast<size_t>(AxisIndex::TOTAL));

            // assign first axis
            SimTK::UnitVec3& firstAxisDir = axes.at(ToIndex(axisEdge.axisIndex));
            firstAxisDir = axisEdge.isNegated ? -axisEdgeDir : axisEdgeDir;

            // compute second axis (via cross product)
            SimTK::UnitVec3& secondAxisDir = axes.at(ToIndex(otherEdge.axisIndex));
            {
                secondAxisDir = SimTK::UnitVec3{SimTK::cross(axisEdgeDir, otherEdgeDir)};
                if (otherEdge.isNegated)
                {
                    secondAxisDir = -secondAxisDir;
                }
            }

            // compute third axis (via cross product)
            {
                // care: the user is allowed to specify axes out-of-order
                //
                // so this bit of code calculates the correct ordering, assuming that
                // axes are in a circular X -> Y -> Z relationship w.r.t. cross products
                struct ThirdEdgeOperands final
                {
                    SimTK::UnitVec3 const& firstDir;
                    SimTK::UnitVec3 const& secondDir;
                    AxisIndex resultAxisIndex;
                };
                ThirdEdgeOperands const ops = Next(axisEdge.axisIndex) == otherEdge.axisIndex ?
                    ThirdEdgeOperands{firstAxisDir, secondAxisDir, Next(otherEdge.axisIndex)} :
                    ThirdEdgeOperands{secondAxisDir, firstAxisDir, Next(axisEdge.axisIndex)};

                SimTK::UnitVec3 const thirdAxisDir = SimTK::UnitVec3{SimTK::cross(ops.firstDir, ops.secondDir)};
                axes.at(ToIndex(ops.resultAxisIndex)) = thirdAxisDir;
            }

            // create transform from orthogonal axes and origin
            SimTK::Mat33 const rotationMatrix{SimTK::Vec3{axes[0]}, SimTK::Vec3{axes[1]}, SimTK::Vec3{axes[2]}};
            SimTK::Rotation const rotation{rotationMatrix};

            return SimTK::Transform{rotation, originLocationInGround};
        }

        SimTK::SpatialVec calcVelocityInGround(SimTK::State const& state) const final
        {
            return {};  // TODO: see OffsetFrame::calcVelocityInGround
        }

        SimTK::SpatialVec calcAccelerationInGround(SimTK::State const& state) const final
        {
            return {};  // TODO: see OffsetFrame::calcAccelerationInGround
        }

        void extendAddToSystem(SimTK::MultibodySystem& system) const final
        {
            Super::extendAddToSystem(system);
            setMobilizedBodyIndex(getModel().getGround().getMobilizedBodyIndex()); // TODO: the frame must be associated to a mobod
        }
    };
}

// general (not layer-system-dependent) user-enactable actions
namespace
{
    // user-enactable actions
    namespace
    {
        void ActionPromptUserToAddMeshFile(osc::UndoableModelStatePair& model)
        {
            std::optional<std::filesystem::path> const maybeMeshPath =
                osc::PromptUserForFile(osc::GetCommaDelimitedListOfSupportedSimTKMeshFormats());
            if (!maybeMeshPath)
            {
                return;  // user didn't select anything
            }
            std::filesystem::path const& meshPath = *maybeMeshPath;
            std::string const meshName = osc::FileNameWithoutExtension(meshPath);

            // add an offset frame that is connected to ground - this will become
            // the mesh's offset frame
            auto meshPhysicalOffsetFrame = std::make_unique<OpenSim::PhysicalOffsetFrame>();
            meshPhysicalOffsetFrame->setParentFrame(model.getModel().getGround());
            meshPhysicalOffsetFrame->setName(meshName + "_offset");

            // attach the mesh to the frame
            {
                auto mesh = std::make_unique<OpenSim::Mesh>(meshPath.string());
                mesh->setName(meshName);
                meshPhysicalOffsetFrame->attachGeometry(mesh.release());
            }

            // create a human-readable commit message
            std::string const commitMessage = GenerateAddedSomethingCommitMessage(meshPath.filename().string());

            // perform the model mutation
            {
                OpenSim::Model& mutableModel = model.updModel();
                OpenSim::PhysicalOffsetFrame const* const pofPtr = meshPhysicalOffsetFrame.get();

                mutableModel.addModelComponent(meshPhysicalOffsetFrame.release());
                mutableModel.finalizeConnections();
                osc::InitializeModel(mutableModel);
                osc::InitializeState(mutableModel);
                model.setSelected(pofPtr);
                model.commit(commitMessage);
            }
        }

        void ActionAddSphereInMeshFrame(
            osc::UndoableModelStatePair& model,
            OpenSim::Mesh const& mesh,
            std::optional<glm::vec3> const& maybeClickPosInGround)
        {
            // if the caller requests a location via a click, set the position accordingly
            SimTK::Vec3 const locationInMeshFrame = maybeClickPosInGround ?
                CalcLocationInFrame(mesh.getFrame(), model.getState(), *maybeClickPosInGround) :
                SimTK::Vec3{0.0, 0.0, 0.0};

            std::string const sphereName = GenerateSceneElementName("sphere_");
            std::string const commitMessage = GenerateAddedSomethingCommitMessage(sphereName);

            // create sphere component
            std::unique_ptr<OpenSim::SphereLandmark> sphere = [&mesh, &locationInMeshFrame, &sphereName]()
            {
                auto rv = std::make_unique<OpenSim::SphereLandmark>();
                rv->setName(sphereName);
                rv->set_location(locationInMeshFrame);
                rv->connectSocket_parent_frame(mesh.getFrame());
                return rv;
            }();

            // perform the model mutation
            {
                OpenSim::Model& mutableModel = model.updModel();
                OpenSim::SphereLandmark const* const spherePtr = sphere.get();

                mutableModel.addModelComponent(sphere.release());
                mutableModel.finalizeConnections();
                osc::InitializeModel(mutableModel);
                osc::InitializeState(mutableModel);
                model.setSelected(spherePtr);
                model.commit(commitMessage);
            }
        }

        void ActionAddOffsetFrameInMeshFrame(
            osc::UndoableModelStatePair& model,
            OpenSim::Mesh const& mesh,
            std::optional<glm::vec3> const& maybeClickPosInGround)
        {
            // if the caller requests a location via a click, set the position accordingly
            SimTK::Vec3 const locationInMeshFrame = maybeClickPosInGround ?
                CalcLocationInFrame(mesh.getFrame(), model.getState(), *maybeClickPosInGround) :
                SimTK::Vec3{0.0, 0.0, 0.0};

            std::string const pofName = GenerateSceneElementName("pof_");
            std::string const commitMessage = GenerateAddedSomethingCommitMessage(pofName);

            // create physical offset frame
            std::unique_ptr<OpenSim::PhysicalOffsetFrame> pof = [&mesh, &locationInMeshFrame, &pofName]()
            {
                auto rv = std::make_unique<OpenSim::PhysicalOffsetFrame>();
                rv->setName(pofName);
                rv->set_translation(locationInMeshFrame);
                rv->connectSocket_parent(mesh.getFrame());
                return rv;
            }();

            // perform model mutation
            {
                OpenSim::Model& mutableModel = model.updModel();
                OpenSim::PhysicalOffsetFrame const* const pofPtr = pof.get();

                mutableModel.addModelComponent(pof.release());
                mutableModel.finalizeConnections();
                osc::InitializeModel(mutableModel);
                osc::InitializeState(mutableModel);
                model.setSelected(pofPtr);
                model.commit(commitMessage);
            }
        }

        void ActionAddPointToPointEdge(
            osc::UndoableModelStatePair& model,
            OpenSim::Point const& pointA,
            OpenSim::Point const& pointB)
        {
            std::string const edgeName = GenerateSceneElementName("edge_");
            std::string const commitMessage = GenerateAddedSomethingCommitMessage(edgeName);

            // create edge
            auto edge = std::make_unique<OpenSim::FDPointToPointEdge>();
            edge->connectSocket_pointA(pointA);
            edge->connectSocket_pointB(pointB);

            // perform model mutation
            {
                OpenSim::Model& mutableModel = model.updModel();
                OpenSim::FDPointToPointEdge const* edgePtr = edge.get();

                mutableModel.addModelComponent(edge.release());
                mutableModel.finalizeConnections();
                osc::InitializeModel(mutableModel);
                osc::InitializeState(mutableModel);
                model.setSelected(edgePtr);
                model.commit(commitMessage);
            }
        }

        void ActionAddMidpoint(
            osc::UndoableModelStatePair& model,
            OpenSim::Point const& pointA,
            OpenSim::Point const& pointB)
        {
            std::string const midpointName = GenerateSceneElementName("midpoint_");
            std::string const commitMessage = GenerateAddedSomethingCommitMessage(midpointName);

            // create midpoint component
            auto midpoint = std::make_unique<OpenSim::MidpointLandmark>();
            midpoint->connectSocket_pointA(pointA);
            midpoint->connectSocket_pointB(pointB);

            // perform model mutation
            {
                OpenSim::Model& mutableModel = model.updModel();
                OpenSim::MidpointLandmark const* midpointPtr = midpoint.get();

                mutableModel.addModelComponent(midpoint.release());
                mutableModel.finalizeConnections();
                osc::InitializeModel(mutableModel);
                osc::InitializeState(mutableModel);
                model.setSelected(midpointPtr);
                model.commit(commitMessage);
            }
        }

        void ActionAddCrossProductEdge(
            osc::UndoableModelStatePair& model,
            OpenSim::FDVirtualEdge const& edgeA,
            OpenSim::FDVirtualEdge const& edgeB)
        {
            std::string const edgeName = GenerateSceneElementName("crossproduct_");
            std::string const commitMessage = GenerateAddedSomethingCommitMessage(edgeName);

            // create cross product edge component
            auto edge = std::make_unique<OpenSim::FDCrossProductEdge>();
            edge->connectSocket_edgeA(edgeA);
            edge->connectSocket_edgeB(edgeB);

            // perform model mutation
            {
                OpenSim::Model& mutableModel = model.updModel();
                OpenSim::FDCrossProductEdge const* edgePtr = edge.get();

                mutableModel.addModelComponent(edge.release());
                mutableModel.finalizeConnections();
                osc::InitializeModel(mutableModel);
                osc::InitializeState(mutableModel);
                model.setSelected(edgePtr);
                model.commit(commitMessage);
            }
        }

        void ActionSwapSocketAssignments(
            osc::UndoableModelStatePair& model,
            OpenSim::ComponentPath componentAbsPath,
            std::string firstSocketName,
            std::string secondSocketName)
        {
            // create commit message
            std::string const commitMessage = [&componentAbsPath, &firstSocketName, &secondSocketName]()
            {
                std::stringstream ss;
                ss << "swapped socket '" << firstSocketName << "' with socket '" << secondSocketName << " in " << componentAbsPath.getComponentName().c_str();
                return std::move(ss).str();
            }();

            // look things up in the mutable model
            OpenSim::Model& mutModel = model.updModel();
            OpenSim::Component* const component = osc::FindComponentMut(mutModel, componentAbsPath);
            if (!component)
            {
                osc::log::error("failed to find %s in model, skipping action", componentAbsPath.toString().c_str());
                return;
            }

            OpenSim::AbstractSocket* const firstSocket = osc::FindSocketMut(*component, firstSocketName);
            if (!firstSocket)
            {
                osc::log::error("failed to find socket %s in %s, skipping action", firstSocketName.c_str(), component->getName().c_str());
                return;
            }

            OpenSim::AbstractSocket* const secondSocket = osc::FindSocketMut(*component, secondSocketName);
            if (!secondSocket)
            {
                osc::log::error("failed to find socket %s in %s, skipping action", secondSocketName.c_str(), component->getName().c_str());
                return;
            }

            // perform swap
            std::string const firstSocketPath = firstSocket->getConnecteePath();
            firstSocket->setConnecteePath(secondSocket->getConnecteePath());
            secondSocket->setConnecteePath(firstSocketPath);

            // finalize and commit
            osc::InitializeModel(mutModel);
            osc::InitializeState(mutModel);
            model.commit(commitMessage);
        }

        void ActionSwapPointToPointEdgeEnds(
            osc::UndoableModelStatePair& model,
            OpenSim::FDPointToPointEdge const& edge)
        {
            ActionSwapSocketAssignments(model, edge.getAbsolutePath(), "pointA", "pointB");
        }

        void ActionSwapCrossProductEdgeOperands(
            osc::UndoableModelStatePair& model,
            OpenSim::FDCrossProductEdge const& edge)
        {
            ActionSwapSocketAssignments(model, edge.getAbsolutePath(), "edgeA", "edgeB");
        }

        void ActionAddFrame(
            std::shared_ptr<osc::UndoableModelStatePair> model,
            OpenSim::FDVirtualEdge const& firstEdge,
            OpenSim::MaybeNegatedAxis firstEdgeAxis,
            OpenSim::FDVirtualEdge const& otherEdge,
            OpenSim::Point const& origin)
        {
            std::string const frameName = GenerateSceneElementName("frame_");
            std::string const commitMessage = GenerateAddedSomethingCommitMessage(frameName);

            // create the frame
            auto frame = std::make_unique<OpenSim::LandmarkDefinedFrame>();
            frame->set_axisEdgeDimension(OpenSim::ToString(firstEdgeAxis));
            frame->set_secondAxisDimension(ToString(Next(firstEdgeAxis)));
            frame->connectSocket_axisEdge(firstEdge);
            frame->connectSocket_otherEdge(otherEdge);
            frame->connectSocket_origin(origin);

            // perform model mutation
            {
                OpenSim::Model& mutModel = model->updModel();
                OpenSim::LandmarkDefinedFrame const* const framePtr = frame.get();

                mutModel.addModelComponent(frame.release());
                mutModel.finalizeConnections();
                osc::InitializeModel(mutModel);
                osc::InitializeState(mutModel);
                model->setSelected(framePtr);
                model->commit(commitMessage);
            }
        }

        osc::Transform CalcTransformWithRespectTo(
            OpenSim::Mesh const& mesh,
            OpenSim::Frame const& frame,
            SimTK::State const& state)
        {
            osc::Transform rv = osc::ToTransform(mesh.getFrame().findTransformBetween(state, frame));
            rv.scale = osc::ToVec3(mesh.get_scale_factors());
            return rv;
        }

        void ActionReexportMeshOBJWithRespectTo(
            OpenSim::Model const& model,
            SimTK::State const& state,
            OpenSim::Mesh const& openSimMesh,
            OpenSim::Frame const& frame)
        {
            // prompt user for a save location
            std::optional<std::filesystem::path> const maybeUserSaveLocation =
                osc::PromptUserForFileSaveLocationAndAddExtensionIfNecessary("obj");
            if (!maybeUserSaveLocation)
            {
                return;  // user didn't select a save location
            }
            std::filesystem::path const& userSaveLocation = *maybeUserSaveLocation;

            // load raw mesh data into an osc mesh for processing
            osc::Mesh oscMesh = osc::LoadMeshViaSimTK(openSimMesh.get_mesh_file());

            // bake transform into mesh data
            oscMesh.transformVerts(CalcTransformWithRespectTo(openSimMesh, frame, state));

            // write transformed mesh to output
            std::ios_base::openmode const outputFlags =
                std::ios_base::out |
                std::ios_base::trunc |
                std::ios_base::binary;
            auto outFile = std::make_shared<std::ofstream>(userSaveLocation, outputFlags);
            if (!(*outFile))
            {
                std::string const error = osc::StrerrorThreadsafe(errno);
                osc::log::error("%s: could not save obj output: %s", userSaveLocation.string().c_str(), error.c_str());
                return;
            }
            osc::ObjWriter writer{outFile};
            writer.write(oscMesh, osc::ObjWriterFlags_IgnoreNormals);
        }
    }
}

// choose `n` components UI flow
namespace
{
    // parameters used to create a "choose components" layer
    struct ChooseComponentsEditorLayerParameters final {
        std::string popupHeaderText = "choose something";

        bool userCanChoosePoints = true;
        bool userCanChooseEdges = true;

        // (maybe) the components that the user has already chosen, or is
        // assigning to (and, therefore, should maybe be highlighted but
        // non-selectable)
        std::unordered_set<std::string> componentsBeingAssignedTo;

        size_t numComponentsUserMustChoose = 1;

        std::function<bool(std::unordered_set<std::string> const&)> onUserFinishedChoosing = [](std::unordered_set<std::string> const&)
        {
            return true;
        };
    };

    // top-level shared state for the "choose components" layer
    struct ChooseComponentsEditorLayerSharedState final {

        explicit ChooseComponentsEditorLayerSharedState(
            std::shared_ptr<osc::UndoableModelStatePair> model_,
            ChooseComponentsEditorLayerParameters parameters_) :

            model{std::move(model_)},
            popupParams{std::move(parameters_)}
        {
        }

        std::shared_ptr<osc::MeshCache> meshCache = osc::App::singleton<osc::MeshCache>();
        std::shared_ptr<osc::UndoableModelStatePair> model;
        ChooseComponentsEditorLayerParameters popupParams;
        osc::ModelRendererParams renderParams;
        std::string hoveredComponent;
        std::unordered_set<std::string> alreadyChosenComponents;
        bool shouldClosePopup = false;
    };

    // grouping of scene (3D) decorations and an associated scene BVH
    struct BVHedDecorations final {
        void clear()
        {
            decorations.clear();
            bvh.clear();
        }

        std::vector<osc::SceneDecoration> decorations;
        osc::BVH bvh;
    };

    // generates scene decorations for the "choose components" layer
    void GenerateChooseComponentsDecorations(
        ChooseComponentsEditorLayerSharedState const& state,
        BVHedDecorations& out)
    {
        out.clear();

        auto const onModelDecoration = [&state, &out](OpenSim::Component const& component, osc::SceneDecoration&& decoration)
        {
            // update flags based on path
            std::string const absPath = osc::GetAbsolutePathString(component);
            if (osc::Contains(state.popupParams.componentsBeingAssignedTo, absPath))
            {
                decoration.flags |= osc::SceneDecorationFlags_IsSelected;
            }
            if (osc::Contains(state.alreadyChosenComponents, absPath))
            {
                decoration.flags |= osc::SceneDecorationFlags_IsSelected;
            }
            if (absPath == state.hoveredComponent)
            {
                decoration.flags |= osc::SceneDecorationFlags_IsHovered;
            }

            if (state.popupParams.userCanChoosePoints && IsPoint(component))
            {
                decoration.id = absPath;
            }
            else if (state.popupParams.userCanChooseEdges && IsEdge(component))
            {
                decoration.id = absPath;
            }
            else
            {
                decoration.color.a *= 0.2f;  // fade non-selectable objects
            }

            out.decorations.push_back(std::move(decoration));
        };

        osc::GenerateModelDecorations(
            *state.meshCache,
            state.model->getModel(),
            state.model->getState(),
            state.renderParams.decorationOptions,
            state.model->getFixupScaleFactor(),
            onModelDecoration
        );

        osc::UpdateSceneBVH(out.decorations, out.bvh);

        auto const onOverlayDecoration = [&](osc::SceneDecoration&& decoration)
        {
            out.decorations.push_back(std::move(decoration));
        };

        osc::GenerateOverlayDecorations(
            *state.meshCache,
            state.renderParams.overlayOptions,
            out.bvh,
            onOverlayDecoration
        );
    }

    // modal popup that prompts the user to select components in the model (e.g.
    // to define an edge, or a frame)
    class ChooseComponentsEditorLayer final : public osc::ModelEditorViewerPanelLayer {
    public:
        ChooseComponentsEditorLayer(
            std::shared_ptr<osc::UndoableModelStatePair> model_,
            ChooseComponentsEditorLayerParameters parameters_) :

            m_State{std::move(model_), std::move(parameters_)},
            m_Renderer{osc::App::get().config(), *osc::App::singleton<osc::MeshCache>(), *osc::App::singleton<osc::ShaderCache>()}
        {
        }

    private:
        bool implHandleKeyboardInputs(
            osc::ModelEditorViewerPanelParameters& params,
            osc::ModelEditorViewerPanelState& state) final
        {
            return osc::UpdatePolarCameraFromImGuiKeyboardInputs(
                params.updRenderParams().camera,
                state.viewportRect,
                m_Decorations.bvh.getRootAABB()
            );
        }

        bool implHandleMouseInputs(
            osc::ModelEditorViewerPanelParameters& params,
            osc::ModelEditorViewerPanelState& state) final
        {
            bool rv = osc::UpdatePolarCameraFromImGuiMouseInputs(
                osc::Dimensions(state.viewportRect),
                params.updRenderParams().camera
            );

            if (osc::IsDraggingWithAnyMouseButtonDown())
            {
                m_State.hoveredComponent = {};
            }

            if (m_IsLeftClickReleasedWithoutDragging)
            {
                rv = tryToggleHover() || rv;
            }

            return rv;
        }

        void implOnDraw(
            osc::ModelEditorViewerPanelParameters& panelParams,
            osc::ModelEditorViewerPanelState& panelState) final
        {
            bool const layerIsHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

            // update this layer's state from provided state
            m_State.renderParams = panelParams.getRenderParams();
            m_IsLeftClickReleasedWithoutDragging = osc::IsMouseReleasedWithoutDragging(ImGuiMouseButton_Left);
            m_IsRightClickReleasedWithoutDragging = osc::IsMouseReleasedWithoutDragging(ImGuiMouseButton_Right);
            if (ImGui::IsKeyReleased(ImGuiKey_Escape))
            {
                m_State.shouldClosePopup = true;
            }

            // generate decorations + rendering params
            GenerateChooseComponentsDecorations(m_State, m_Decorations);
            osc::SceneRendererParams const rendererParameters = osc::CalcSceneRendererParams(
                m_State.renderParams,
                osc::Dimensions(panelState.viewportRect),
                osc::App::get().getMSXAASamplesRecommended(),
                m_State.model->getFixupScaleFactor()
            );

            // render to a texture (no caching)
            m_Renderer.draw(m_Decorations.decorations, rendererParameters);

            // blit texture as ImGui image
            osc::DrawTextureAsImGuiImage(
                m_Renderer.updRenderTexture(),
                osc::Dimensions(panelState.viewportRect)
            );

            // do hovertest
            if (layerIsHovered)
            {
                std::optional<osc::SceneCollision> const collision = osc::GetClosestCollision(
                    m_Decorations.bvh,
                    m_Decorations.decorations,
                    m_State.renderParams.camera,
                    ImGui::GetMousePos(),
                    panelState.viewportRect
                );
                if (collision)
                {
                    m_State.hoveredComponent = collision->decorationID;
                }
                else
                {
                    m_State.hoveredComponent = {};
                }
            }

            // show tooltip
            if (OpenSim::Component const* c = osc::FindComponent(m_State.model->getModel(), m_State.hoveredComponent))
            {
                osc::DrawComponentHoverTooltip(*c);
            }

            // show header
            ImGui::SetCursorScreenPos(panelState.viewportRect.p1);
            ImGui::TextUnformatted(m_State.popupParams.popupHeaderText.c_str());

            // handle completion state (i.e. user selected enough components)
            if (m_State.alreadyChosenComponents.size() == m_State.popupParams.numComponentsUserMustChoose)
            {
                m_State.popupParams.onUserFinishedChoosing(m_State.alreadyChosenComponents);
                m_State.shouldClosePopup = true;
            }
        }

        float implGetBackgroundAlpha() const final
        {
            return 1.0f;
        }

        bool implShouldClose() const final
        {
            return m_State.shouldClosePopup;
        }

        bool tryToggleHover()
        {
            std::string const& absPath = m_State.hoveredComponent;
            OpenSim::Component const* component = osc::FindComponent(m_State.model->getModel(), absPath);

            if (!component)
            {
                return false;  // nothing hovered
            }
            else if (osc::Contains(m_State.popupParams.componentsBeingAssignedTo, absPath))
            {
                return false;  // cannot be selected
            }
            else if (auto it = m_State.alreadyChosenComponents.find(absPath); it != m_State.alreadyChosenComponents.end())
            {
                m_State.alreadyChosenComponents.erase(it);
                return true;   // de-selected
            }
            else if (
                m_State.alreadyChosenComponents.size() < m_State.popupParams.numComponentsUserMustChoose &&
                (
                    (m_State.popupParams.userCanChoosePoints && IsPoint(*component)) ||
                    (m_State.popupParams.userCanChooseEdges && IsEdge(*component))
                )
            )
            {
                m_State.alreadyChosenComponents.insert(absPath);
                return true;   // selected
            }
            else
            {
                return false;  // don't know how to handle
            }
        }

        ChooseComponentsEditorLayerSharedState m_State;
        BVHedDecorations m_Decorations;
        osc::SceneRenderer m_Renderer;
        bool m_IsLeftClickReleasedWithoutDragging = false;
        bool m_IsRightClickReleasedWithoutDragging = false;
    };

    /////
    // layer pushing routines
    /////

    void PushCreateEdgeToOtherPointLayer(
        osc::EditorAPI& editor,
        std::shared_ptr<osc::UndoableModelStatePair> model,
        OpenSim::Point const& point,
        std::optional<osc::ModelEditorViewerPanelRightClickEvent> const& maybeSourceEvent)
    {
        osc::ModelEditorViewerPanel* const visualizer =
            editor.getPanelManager()->tryUpdPanelByNameT<osc::ModelEditorViewerPanel>(maybeSourceEvent->sourcePanelName);
        if (!visualizer)
        {
            return;  // can't figure out which visualizer to push the layer to
        }

        ChooseComponentsEditorLayerParameters options;
        options.popupHeaderText = "choose other point";
        options.userCanChoosePoints = true;
        options.userCanChooseEdges = false;
        options.componentsBeingAssignedTo = {point.getAbsolutePathString()};
        options.numComponentsUserMustChoose = 1;
        options.onUserFinishedChoosing = [model, pointAPath = point.getAbsolutePathString()](std::unordered_set<std::string> const& choices) -> bool
        {
            if (choices.empty())
            {
                osc::log::error("user selections from the 'choose components' layer was empty: this bug should be reported");
                return false;
            }
            if (choices.size() > 1)
            {
                osc::log::warn("number of user selections from 'choose components' layer was greater than expected: this bug should be reported");
            }
            std::string const& pointBPath = *choices.begin();

            OpenSim::Point const* pointA = osc::FindComponent<OpenSim::Point>(model->getModel(), pointAPath);
            if (!pointA)
            {
                osc::log::error("point A's component path (%s) does not exist in the model", pointAPath.c_str());
                return false;
            }

            OpenSim::Point const* pointB = osc::FindComponent<OpenSim::Point>(model->getModel(), pointBPath);
            if (!pointB)
            {
                osc::log::error("point B's component path (%s) does not exist in the model", pointBPath.c_str());
                return false;
            }

            ActionAddPointToPointEdge(*model, *pointA, *pointB);
            return true;
        };

        visualizer->pushLayer(std::make_unique<ChooseComponentsEditorLayer>(model, options));
    }

    void PushCreateMidpointToAnotherPointLayer(
        osc::EditorAPI& editor,
        std::shared_ptr<osc::UndoableModelStatePair> model,
        OpenSim::Point const& point,
        std::optional<osc::ModelEditorViewerPanelRightClickEvent> const& maybeSourceEvent)
    {
        osc::ModelEditorViewerPanel* const visualizer =
            editor.getPanelManager()->tryUpdPanelByNameT<osc::ModelEditorViewerPanel>(maybeSourceEvent->sourcePanelName);
        if (!visualizer)
        {
            return;  // can't figure out which visualizer to push the layer to
        }

        ChooseComponentsEditorLayerParameters options;
        options.popupHeaderText = "choose other point";
        options.userCanChoosePoints = true;
        options.userCanChooseEdges = false;
        options.componentsBeingAssignedTo = {point.getAbsolutePathString()};
        options.numComponentsUserMustChoose = 1;
        options.onUserFinishedChoosing = [model, pointAPath = point.getAbsolutePathString()](std::unordered_set<std::string> const& choices) -> bool
        {
            if (choices.empty())
            {
                osc::log::error("user selections from the 'choose components' layer was empty: this bug should be reported");
                return false;
            }
            if (choices.size() > 1)
            {
                osc::log::warn("number of user selections from 'choose components' layer was greater than expected: this bug should be reported");
            }
            std::string const& pointBPath = *choices.begin();

            OpenSim::Point const* pointA = osc::FindComponent<OpenSim::Point>(model->getModel(), pointAPath);
            if (!pointA)
            {
                osc::log::error("point A's component path (%s) does not exist in the model", pointAPath.c_str());
                return false;
            }

            OpenSim::Point const* pointB = osc::FindComponent<OpenSim::Point>(model->getModel(), pointBPath);
            if (!pointB)
            {
                osc::log::error("point B's component path (%s) does not exist in the model", pointBPath.c_str());
                return false;
            }

            ActionAddMidpoint(*model, *pointA, *pointB);
            return true;
        };

        visualizer->pushLayer(std::make_unique<ChooseComponentsEditorLayer>(model, options));
    }

    void PushCreateCrossProductEdgeLayer(
        osc::EditorAPI& editor,
        std::shared_ptr<osc::UndoableModelStatePair> model,
        OpenSim::FDVirtualEdge const& firstEdge,
        std::optional<osc::ModelEditorViewerPanelRightClickEvent> const& maybeSourceEvent)
    {
        osc::ModelEditorViewerPanel* const visualizer =
            editor.getPanelManager()->tryUpdPanelByNameT<osc::ModelEditorViewerPanel>(maybeSourceEvent->sourcePanelName);
        if (!visualizer)
        {
            return;  // can't figure out which visualizer to push the layer to
        }

        ChooseComponentsEditorLayerParameters options;
        options.popupHeaderText = "choose other edge";
        options.userCanChoosePoints = false;
        options.userCanChooseEdges = true;
        options.componentsBeingAssignedTo = {firstEdge.getAbsolutePathString()};
        options.numComponentsUserMustChoose = 1;
        options.onUserFinishedChoosing = [model, edgeAPath = firstEdge.getAbsolutePathString()](std::unordered_set<std::string> const& choices) -> bool
        {
            if (choices.empty())
            {
                osc::log::error("user selections from the 'choose components' layer was empty: this bug should be reported");
                return false;
            }
            if (choices.size() > 1)
            {
                osc::log::warn("number of user selections from 'choose components' layer was greater than expected: this bug should be reported");
            }
            std::string const& edgeBPath = *choices.begin();

            OpenSim::FDVirtualEdge const* edgeA = osc::FindComponent<OpenSim::FDVirtualEdge>(model->getModel(), edgeAPath);
            if (!edgeA)
            {
                osc::log::error("edge A's component path (%s) does not exist in the model", edgeAPath.c_str());
                return false;
            }

            OpenSim::FDVirtualEdge const* edgeB = osc::FindComponent<OpenSim::FDVirtualEdge>(model->getModel(), edgeBPath);
            if (!edgeB)
            {
                osc::log::error("point B's component path (%s) does not exist in the model", edgeBPath.c_str());
                return false;
            }

            ActionAddCrossProductEdge(*model, *edgeA, *edgeB);
            return true;
        };

        visualizer->pushLayer(std::make_unique<ChooseComponentsEditorLayer>(model, options));
    }

    void PushPickOriginForFrameDefinitionLayer(
        osc::ModelEditorViewerPanel& visualizer,
        std::shared_ptr<osc::UndoableModelStatePair> model,
        std::string const& firstEdgeAbsPath,
        OpenSim::MaybeNegatedAxis firstEdgeAxis,
        std::string const& secondEdgeAbsPath)
    {
        ChooseComponentsEditorLayerParameters options;
        options.popupHeaderText = "choose frame origin";
        options.userCanChoosePoints = true;
        options.userCanChooseEdges = false;
        options.numComponentsUserMustChoose = 1;
        options.onUserFinishedChoosing = [
            model,
                firstEdgeAbsPath = firstEdgeAbsPath,
                firstEdgeAxis,
                secondEdgeAbsPath
        ](std::unordered_set<std::string> const& choices) -> bool
        {
            if (choices.empty())
            {
                osc::log::error("user selections from the 'choose components' layer was empty: this bug should be reported");
                return false;
            }
            if (choices.size() > 1)
            {
                osc::log::warn("number of user selections from 'choose components' layer was greater than expected: this bug should be reported");
            }
            std::string const& originPath = *choices.begin();

            OpenSim::FDVirtualEdge const* firstEdge = osc::FindComponent<OpenSim::FDVirtualEdge>(model->getModel(), firstEdgeAbsPath);
            if (!firstEdge)
            {
                osc::log::error("the first edge's component path (%s) does not exist in the model", firstEdgeAbsPath.c_str());
                return false;
            }

            OpenSim::FDVirtualEdge const* otherEdge = osc::FindComponent<OpenSim::FDVirtualEdge>(model->getModel(), secondEdgeAbsPath);
            if (!otherEdge)
            {
                osc::log::error("the second edge's component path (%s) does not exist in the model", secondEdgeAbsPath.c_str());
                return false;
            }

            OpenSim::Point const* originPoint = osc::FindComponent<OpenSim::Point>(model->getModel(), originPath);
            if (!originPoint)
            {
                osc::log::error("the origin's component path (%s) does not exist in the model", originPath.c_str());
                return false;
            }

            ActionAddFrame(
                model,
                *firstEdge,
                firstEdgeAxis,
                *otherEdge,
                *originPoint
            );
            return true;
        };

        visualizer.pushLayer(std::make_unique<ChooseComponentsEditorLayer>(model, options));
    }

    void PushPickOtherEdgeStateForFrameDefinitionLayer(
        osc::ModelEditorViewerPanel& visualizer,
        std::shared_ptr<osc::UndoableModelStatePair> model,
        OpenSim::FDVirtualEdge const& firstEdge,
        OpenSim::MaybeNegatedAxis firstEdgeAxis)
    {
        ChooseComponentsEditorLayerParameters options;
        options.popupHeaderText = "choose other edge";
        options.userCanChoosePoints = false;
        options.userCanChooseEdges = true;
        options.componentsBeingAssignedTo = {firstEdge.getAbsolutePathString()};
        options.numComponentsUserMustChoose = 1;
        options.onUserFinishedChoosing = [
            visualizerPtr = &visualizer,  // TODO: implement weak_ptr for panel lookup
                model,
                firstEdgeAbsPath = firstEdge.getAbsolutePathString(),
                firstEdgeAxis
        ](std::unordered_set<std::string> const& choices) -> bool
        {
            // go into "pick origin" state

            if (choices.empty())
            {
                osc::log::error("user selections from the 'choose components' layer was empty: this bug should be reported");
                return false;
            }
            if (choices.size() > 1)
            {
                osc::log::warn("number of user selections from 'choose components' layer was greater than expected: this bug should be reported");
            }
            std::string const& otherEdgePath = *choices.begin();

            PushPickOriginForFrameDefinitionLayer(
                *visualizerPtr,  // TODO: unsafe if not guarded by weak_ptr or similar
                model,
                firstEdgeAbsPath,
                firstEdgeAxis,
                otherEdgePath
            );
            return true;
        };

        visualizer.pushLayer(std::make_unique<ChooseComponentsEditorLayer>(model, options));
    }
}

namespace
{
    void ActionPushCreateFrameLayer(
        osc::EditorAPI& editor,
        std::shared_ptr<osc::UndoableModelStatePair> model,
        OpenSim::FDVirtualEdge const& firstEdge,
        OpenSim::MaybeNegatedAxis firstEdgeAxis,
        std::optional<osc::ModelEditorViewerPanelRightClickEvent> const& maybeSourceEvent)
    {
        if (!maybeSourceEvent)
        {
            return;
        }

        osc::ModelEditorViewerPanel* const visualizer =
            editor.getPanelManager()->tryUpdPanelByNameT<osc::ModelEditorViewerPanel>(maybeSourceEvent->sourcePanelName);

        if (!visualizer)
        {
            return;  // can't figure out which visualizer to push the layer to
        }

        PushPickOtherEdgeStateForFrameDefinitionLayer(
            *visualizer,
            model,
            firstEdge,
            firstEdgeAxis
        );
    }
}

// context menu
namespace
{
    void DrawGenericRightClickComponentContextMenuActions(
        osc::EditorAPI& editor,
        std::shared_ptr<osc::UndoableModelStatePair> model,
        std::optional<osc::ModelEditorViewerPanelRightClickEvent> const& maybeSourceEvent,
        OpenSim::Component const&)
    {
        if (ImGui::BeginMenu(ICON_FA_CAMERA " Focus Camera"))
        {
            if (ImGui::MenuItem("On Ground"))
            {
                osc::ModelEditorViewerPanel* visualizer =
                    editor.getPanelManager()->tryUpdPanelByNameT<osc::ModelEditorViewerPanel>(maybeSourceEvent->sourcePanelName);
                if (visualizer)
                {
                    visualizer->focusOn({});
                }
            }

            if (maybeSourceEvent &&
                maybeSourceEvent->maybeClickPositionInGround &&
                ImGui::MenuItem("On Click Position"))
            {
                osc::ModelEditorViewerPanel* visualizer =
                    editor.getPanelManager()->tryUpdPanelByNameT<osc::ModelEditorViewerPanel>(maybeSourceEvent->sourcePanelName);
                if (visualizer)
                {
                    visualizer->focusOn(*maybeSourceEvent->maybeClickPositionInGround);
                }
            }

            ImGui::EndMenu();
        }
    }

    void DrawGenericRightClickEdgeContextMenuActions(
        osc::EditorAPI& editor,
        std::shared_ptr<osc::UndoableModelStatePair> model,
        std::optional<osc::ModelEditorViewerPanelRightClickEvent> const& maybeSourceEvent,
        OpenSim::FDVirtualEdge const& edge)
    {
        if (maybeSourceEvent && ImGui::MenuItem(ICON_FA_TIMES " Create Cross Product Edge"))
        {
            PushCreateCrossProductEdgeLayer(editor, model, edge, maybeSourceEvent);
        }

        if (maybeSourceEvent && ImGui::BeginMenu(ICON_FA_ARROWS_ALT " Create Frame With This Edge as"))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, {1.0f, 0.5f, 0.5f, 1.0f});
            if (ImGui::MenuItem("+x"))
            {
                ActionPushCreateFrameLayer(
                    editor,
                    model,
                    edge,
                    OpenSim::MaybeNegatedAxis{OpenSim::AxisIndex::X, false},
                    maybeSourceEvent
                );
            }
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, {0.5f, 1.0f, 0.5f, 1.0f});
            if (ImGui::MenuItem("+y"))
            {
                ActionPushCreateFrameLayer(
                    editor,
                    model,
                    edge,
                    OpenSim::MaybeNegatedAxis{OpenSim::AxisIndex::Y, false},
                    maybeSourceEvent
                );
            }
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, {0.5f, 0.5f, 1.0f, 1.0f});
            if (ImGui::MenuItem("+z"))
            {
                ActionPushCreateFrameLayer(
                    editor,
                    model,
                    edge,
                    OpenSim::MaybeNegatedAxis{OpenSim::AxisIndex::Z, false},
                    maybeSourceEvent
                );
            }
            ImGui::PopStyleColor();

            ImGui::Separator();

            ImGui::PushStyleColor(ImGuiCol_Text, {1.0f, 0.5f, 0.5f, 1.0f});
            if (ImGui::MenuItem("-x"))
            {
                ActionPushCreateFrameLayer(
                    editor,
                    model,
                    edge,
                    OpenSim::MaybeNegatedAxis{OpenSim::AxisIndex::X, true},
                    maybeSourceEvent
                );
            }
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, {0.5f, 1.0f, 0.5f, 1.0f});
            if (ImGui::MenuItem("-y"))
            {
                ActionPushCreateFrameLayer(
                    editor,
                    model,
                    edge,
                    OpenSim::MaybeNegatedAxis{OpenSim::AxisIndex::Y, true},
                    maybeSourceEvent
                );
            }
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, {0.5f, 0.5f, 1.0f, 1.0f});
            if (ImGui::MenuItem("-z"))
            {
                ActionPushCreateFrameLayer(
                    editor,
                    model,
                    edge,
                    OpenSim::MaybeNegatedAxis{OpenSim::AxisIndex::Z, true},
                    maybeSourceEvent
                );
            }
            ImGui::PopStyleColor();

            ImGui::EndMenu();
        }
    }

    void DrawRightClickedNothingContextMenu(
        osc::UndoableModelStatePair& model)
    {
        osc::DrawNothingRightClickedContextMenuHeader();
        osc::DrawContextMenuSeparator();

        if (ImGui::MenuItem(ICON_FA_CUBE " Add Mesh"))
        {
            ActionPromptUserToAddMeshFile(model);
        }
    }

    void DrawRightClickedMeshContextMenu(
        osc::EditorAPI& editor,
        std::shared_ptr<osc::UndoableModelStatePair> model,
        std::optional<osc::ModelEditorViewerPanelRightClickEvent> const& maybeSourceEvent,
        OpenSim::Mesh const& mesh)
    {
        osc::DrawRightClickedComponentContextMenuHeader(mesh);
        osc::DrawContextMenuSeparator();

        if (ImGui::MenuItem(ICON_FA_CIRCLE " Add Sphere Landmark"))
        {
            ActionAddSphereInMeshFrame(
                *model,
                mesh,
                maybeSourceEvent ? maybeSourceEvent->maybeClickPositionInGround : std::nullopt
            );
        }

        if (ImGui::MenuItem(ICON_FA_ARROWS_ALT " Add Offset Frame"))
        {
            ActionAddOffsetFrameInMeshFrame(
                *model,
                mesh,
                maybeSourceEvent ? maybeSourceEvent->maybeClickPositionInGround : std::nullopt
            );
        }

        if (ImGui::BeginMenu(ICON_FA_FILE_EXPORT " Export"))
        {
            if (ImGui::BeginMenu(".obj"))
            {
                if (ImGui::BeginMenu("With Respect to"))
                {
                    int imguiID = 0;
                    for (OpenSim::Frame const& frame : model->getModel().getComponentList<OpenSim::Frame>())
                    {
                        ImGui::PushID(imguiID++);
                        if (ImGui::MenuItem(frame.getName().c_str()))
                        {
                            ActionReexportMeshOBJWithRespectTo(
                                model->getModel(),
                                model->getState(),
                                mesh,
                                frame
                            );
                        }
                        ImGui::PopID();
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        DrawGenericRightClickComponentContextMenuActions(editor, model, maybeSourceEvent, mesh);
    }

    void DrawRightClickedPointContextMenu(
        osc::EditorAPI& editor,
        std::shared_ptr<osc::UndoableModelStatePair> model,
        std::optional<osc::ModelEditorViewerPanelRightClickEvent> const& maybeSourceEvent,
        OpenSim::Point const& point)
    {
        osc::DrawRightClickedComponentContextMenuHeader(point);
        osc::DrawContextMenuSeparator();

        if (maybeSourceEvent && ImGui::MenuItem(ICON_FA_GRIP_LINES " Create Edge"))
        {
            PushCreateEdgeToOtherPointLayer(editor, model, point, maybeSourceEvent);
        }

        if (maybeSourceEvent && ImGui::MenuItem(ICON_FA_DOT_CIRCLE " Create Midpoint"))
        {
            PushCreateMidpointToAnotherPointLayer(editor, model, point, maybeSourceEvent);
        }

        if (ImGui::BeginMenu(ICON_FA_CALCULATOR " Calculate Position"))
        {
            if (ImGui::BeginMenu("With Respect to"))
            {
                int imguiID = 0;
                for (OpenSim::Frame const& frame : model->getModel().getComponentList<OpenSim::Frame>())
                {
                    ImGui::PushID(imguiID++);
                    if (ImGui::BeginMenu(frame.getName().c_str()))
                    {
                        SimTK::Transform const groundToFrame = frame.getTransformInGround(model->getState()).invert();
                        glm::vec3 position = osc::ToVec3(groundToFrame * point.getLocationInGround(model->getState()));

                        ImGui::Text("translation");
                        ImGui::SameLine();
                        osc::DrawHelpMarker("translation", "Translational offset (in meters) of the point expressed in the chosen frame");
                        ImGui::SameLine();
                        ImGui::InputFloat3("##translation", glm::value_ptr(position), OSC_DEFAULT_FLOAT_INPUT_FORMAT, ImGuiInputTextFlags_ReadOnly);

                        ImGui::EndMenu();
                    }
                    ImGui::PopID();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        DrawGenericRightClickComponentContextMenuActions(editor, model, maybeSourceEvent, point);
    }

    void DrawRightClickedPointToPointEdgeContextMenu(
        osc::EditorAPI& editor,
        std::shared_ptr<osc::UndoableModelStatePair> model,
        std::optional<osc::ModelEditorViewerPanelRightClickEvent> const& maybeSourceEvent,
        OpenSim::FDPointToPointEdge const& edge)
    {
        osc::DrawRightClickedComponentContextMenuHeader(edge);
        osc::DrawContextMenuSeparator();
        DrawGenericRightClickEdgeContextMenuActions(editor, model, maybeSourceEvent, edge);
        if (ImGui::MenuItem(ICON_FA_RECYCLE " Swap Direction"))
        {
            ActionSwapPointToPointEdgeEnds(*model, edge);
        }
        DrawGenericRightClickComponentContextMenuActions(editor, model, maybeSourceEvent, edge);
    }

    void DrawRightClickedCrossProductEdgeContextMenu(
        osc::EditorAPI& editor,
        std::shared_ptr<osc::UndoableModelStatePair> model,
        std::optional<osc::ModelEditorViewerPanelRightClickEvent> const& maybeSourceEvent,
        OpenSim::FDCrossProductEdge const& edge)
    {
        osc::DrawRightClickedComponentContextMenuHeader(edge);
        osc::DrawContextMenuSeparator();
        DrawGenericRightClickEdgeContextMenuActions(editor, model, maybeSourceEvent, edge);
        if (ImGui::MenuItem(ICON_FA_RECYCLE " Swap Operands"))
        {
            ActionSwapCrossProductEdgeOperands(*model, edge);
        }
        DrawGenericRightClickComponentContextMenuActions(editor, model, maybeSourceEvent, edge);
    }

    void DrawRightClickedFrameContextMenu(
        osc::EditorAPI& editor,
        std::shared_ptr<osc::UndoableModelStatePair> model,
        std::optional<osc::ModelEditorViewerPanelRightClickEvent> const& maybeSourceEvent,
        OpenSim::Frame const& frame)
    {
        osc::DrawRightClickedComponentContextMenuHeader(frame);
        osc::DrawContextMenuSeparator();

        if (ImGui::BeginMenu(ICON_FA_CALCULATOR " Calculate Transform"))
        {
            if (ImGui::BeginMenu("With Respect to"))
            {
                int imguiID = 0;
                for (OpenSim::Frame const& otherFrame : model->getModel().getComponentList<OpenSim::Frame>())
                {
                    ImGui::PushID(imguiID++);
                    if (ImGui::BeginMenu(otherFrame.getName().c_str()))
                    {
                        SimTK::Transform const xform = frame.findTransformBetween(model->getState(), otherFrame);
                        glm::vec3 position = osc::ToVec3(xform.p());
                        glm::vec3 rotationEulers = osc::ToVec3(xform.R().convertRotationToBodyFixedXYZ());

                        ImGui::Text("translation");
                        ImGui::SameLine();
                        osc::DrawHelpMarker("translation", "Translational offset (in meters) of the frame's origin expressed in the chosen frame");
                        ImGui::SameLine();
                        ImGui::InputFloat3("##translation", glm::value_ptr(position), OSC_DEFAULT_FLOAT_INPUT_FORMAT, ImGuiInputTextFlags_ReadOnly);

                        ImGui::Text("orientation");
                        ImGui::SameLine();
                        osc::DrawHelpMarker("orientation", "Orientation offset (in radians) of the frame, expressed in the chosen frame as a frame-fixed x-y-z rotation sequence");
                        ImGui::SameLine();
                        ImGui::InputFloat3("##orientation", glm::value_ptr(rotationEulers), OSC_DEFAULT_FLOAT_INPUT_FORMAT, ImGuiInputTextFlags_ReadOnly);

                        ImGui::EndMenu();
                    }
                    ImGui::PopID();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        DrawGenericRightClickComponentContextMenuActions(editor, model, maybeSourceEvent, frame);
    }

    void DrawRightClickedUnknownComponentContextMenu(
        osc::EditorAPI& editor,
        std::shared_ptr<osc::UndoableModelStatePair> model,
        std::optional<osc::ModelEditorViewerPanelRightClickEvent> const& maybeSourceEvent,
        OpenSim::Component const& component)
    {
        osc::DrawRightClickedComponentContextMenuHeader(component);
        osc::DrawContextMenuSeparator();
        DrawGenericRightClickComponentContextMenuActions(editor, model, maybeSourceEvent, component);
    }

    // popup state for the frame definition tab's general context menu
    class FrameDefinitionContextMenu final : public osc::StandardPopup {
    public:
        FrameDefinitionContextMenu(
            std::string_view popupName_,
            osc::EditorAPI* editorAPI_,
            std::shared_ptr<osc::UndoableModelStatePair> model_,
            OpenSim::ComponentPath componentPath_,
            std::optional<osc::ModelEditorViewerPanelRightClickEvent> maybeSourceVisualizerEvent_ = std::nullopt) :

            StandardPopup{popupName_, {10.0f, 10.0f}, ImGuiWindowFlags_NoMove},
            m_EditorAPI{editorAPI_},
            m_Model{std::move(model_)},
            m_ComponentPath{std::move(componentPath_)},
            m_MaybeSourceVisualizerEvent{maybeSourceVisualizerEvent_}
        {
            OSC_ASSERT(m_EditorAPI != nullptr);
            OSC_ASSERT(m_Model != nullptr);

            setModal(false);
        }

    private:
        void implDrawContent() final
        {
            OpenSim::Component const* const maybeComponent = osc::FindComponent(m_Model->getModel(), m_ComponentPath);
            if (!maybeComponent)
            {
                DrawRightClickedNothingContextMenu(*m_Model);
            }
            else if (OpenSim::Mesh const* maybeMesh = dynamic_cast<OpenSim::Mesh const*>(maybeComponent))
            {
                DrawRightClickedMeshContextMenu(*m_EditorAPI, m_Model, m_MaybeSourceVisualizerEvent, *maybeMesh);
            }
            else if (OpenSim::Point const* maybePoint = dynamic_cast<OpenSim::Point const*>(maybeComponent))
            {
                DrawRightClickedPointContextMenu(*m_EditorAPI, m_Model, m_MaybeSourceVisualizerEvent, *maybePoint);
            }
            else if (OpenSim::Frame const* maybeFrame = dynamic_cast<OpenSim::Frame const*>(maybeComponent))
            {
                DrawRightClickedFrameContextMenu(*m_EditorAPI, m_Model, m_MaybeSourceVisualizerEvent, *maybeFrame);
            }
            else if (OpenSim::FDPointToPointEdge const* maybeP2PEdge = dynamic_cast<OpenSim::FDPointToPointEdge const*>(maybeComponent))
            {
                DrawRightClickedPointToPointEdgeContextMenu(*m_EditorAPI, m_Model, m_MaybeSourceVisualizerEvent, *maybeP2PEdge);
            }
            else if (OpenSim::FDCrossProductEdge const* maybeCPEdge = dynamic_cast<OpenSim::FDCrossProductEdge const*>(maybeComponent))
            {
                DrawRightClickedCrossProductEdgeContextMenu(*m_EditorAPI, m_Model, m_MaybeSourceVisualizerEvent, *maybeCPEdge);
            }
            else
            {
                DrawRightClickedUnknownComponentContextMenu(*m_EditorAPI, m_Model, m_MaybeSourceVisualizerEvent, *maybeComponent);
            }
        }

        osc::EditorAPI* m_EditorAPI;
        std::shared_ptr<osc::UndoableModelStatePair> m_Model;
        OpenSim::ComponentPath m_ComponentPath;
        std::optional<osc::ModelEditorViewerPanelRightClickEvent> m_MaybeSourceVisualizerEvent;
    };
}

// other panels/widgets
namespace
{
    class FrameDefinitionTabMainMenu final {
    public:
        explicit FrameDefinitionTabMainMenu(
            std::shared_ptr<osc::UndoableModelStatePair> model_,
            std::shared_ptr<osc::PanelManager> panelManager_) :

            m_Model{std::move(model_)},
            m_WindowMenu{std::move(panelManager_)}
        {
        }

        void draw()
        {
            drawEditMenu();
            m_WindowMenu.draw();
            m_AboutMenu.draw();
        }

    private:
        void drawEditMenu()
        {
            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem(ICON_FA_UNDO " Undo", nullptr, false, m_Model->canUndo()))
                {
                    osc::ActionUndoCurrentlyEditedModel(*m_Model);
                }

                if (ImGui::MenuItem(ICON_FA_REDO " Redo", nullptr, false, m_Model->canRedo()))
                {
                    osc::ActionRedoCurrentlyEditedModel(*m_Model);
                }
                ImGui::EndMenu();
            }
        }

        std::shared_ptr<osc::UndoableModelStatePair> m_Model;
        osc::WindowMenu m_WindowMenu;
        osc::MainMenuAboutTab m_AboutMenu;
    };
}

class osc::FrameDefinitionTab::Impl final : public EditorAPI {
public:

    Impl(std::weak_ptr<TabHost> parent_) :
        m_Parent{std::move(parent_)}
    {
        // register user-visible panels that this tab can host

        m_PanelManager->registerToggleablePanel(
            "Navigator",
            [this](std::string_view panelName)
            {
                return std::make_shared<NavigatorPanel>(
                    panelName,
                    m_Model
                );
            }
        );
        m_PanelManager->registerToggleablePanel(
            "Properties",
            [this](std::string_view panelName)
            {
                return std::make_shared<PropertiesPanel>(panelName, this, m_Model);
            }
        );
        m_PanelManager->registerToggleablePanel(
            "Log",
            [this](std::string_view panelName)
            {
                return std::make_shared<LogViewerPanel>(panelName);
            }
        );
        m_PanelManager->registerSpawnablePanel(
            "viewer",
            [this](std::string_view panelName)
            {
                ModelEditorViewerPanelParameters panelParams
                {
                    m_Model,
                    [this](ModelEditorViewerPanelRightClickEvent const& e)
                    {
                        pushPopup(std::make_unique<FrameDefinitionContextMenu>(
                            "##ContextMenu",
                            this,
                            m_Model,
                            e.componentAbsPathOrEmpty,
                            e
                        ));
                    }
                };
                SetupDefault3DViewportRenderingParams(panelParams.updRenderParams());

                return std::make_shared<ModelEditorViewerPanel>(panelName, panelParams);
            },
            1
        );
    }

    UID getID() const
    {
        return m_TabID;
    }

    CStringView getName() const
    {
        return c_TabStringID;
    }

    void onMount()
    {
        App::upd().makeMainEventLoopWaiting();
        m_PanelManager->onMount();
        m_PopupManager.onMount();
    }

    void onUnmount()
    {
        m_PanelManager->onUnmount();
        App::upd().makeMainEventLoopPolling();
    }

    bool onEvent(SDL_Event const& e)
    {
        if (e.type == SDL_KEYDOWN)
        {
            return onKeydownEvent(e.key);
        }
        else
        {
            return false;
        }
    }

    void onTick()
    {
        m_PanelManager->onTick();
    }

    void onDrawMainMenu()
    {
        m_MainMenu.draw();
    }

    void onDraw()
    {
        ImGui::DockSpaceOverViewport(
            ImGui::GetMainViewport(),
            ImGuiDockNodeFlags_PassthruCentralNode
        );
        m_PanelManager->onDraw();
        m_PopupManager.draw();
    }

private:
    bool onKeydownEvent(SDL_KeyboardEvent const& e)
    {
        bool const ctrlOrSuperDown = osc::IsCtrlOrSuperDown();

        if (ctrlOrSuperDown && e.keysym.mod & KMOD_SHIFT && e.keysym.sym == SDLK_z)
        {
            // Ctrl+Shift+Z: redo
            osc::ActionRedoCurrentlyEditedModel(*m_Model);
            return true;
        }
        else if (ctrlOrSuperDown && e.keysym.sym == SDLK_z)
        {
            // Ctrl+Z: undo
            osc::ActionUndoCurrentlyEditedModel(*m_Model);
            return true;
        }
        else if (e.keysym.sym == SDLK_BACKSPACE || e.keysym.sym == SDLK_DELETE)
        {
            // BACKSPACE/DELETE: delete selection
            osc::ActionTryDeleteSelectionFromEditedModel(*m_Model);
            return true;
        }
        else
        {
            return false;
        }
    }

    void implPushComponentContextMenuPopup(OpenSim::ComponentPath const& componentPath) final
    {
        pushPopup(std::make_unique<FrameDefinitionContextMenu>(
            "##ContextMenu",
            this,
            m_Model,
            componentPath
        ));
    }

    void implPushPopup(std::unique_ptr<Popup> popup) final
    {
        popup->open();
        m_PopupManager.push_back(std::move(popup));
    }

    void implAddMusclePlot(OpenSim::Coordinate const&, OpenSim::Muscle const&)
    {
        // ignore: not applicable in this tab
    }

    std::shared_ptr<PanelManager> implGetPanelManager()
    {
        return m_PanelManager;
    }

    UID m_TabID;
    std::weak_ptr<TabHost> m_Parent;

    std::shared_ptr<UndoableModelStatePair> m_Model = MakeSharedUndoableFrameDefinitionModel();
    std::shared_ptr<PanelManager> m_PanelManager = std::make_shared<PanelManager>();
    PopupManager m_PopupManager;

    FrameDefinitionTabMainMenu m_MainMenu{m_Model, m_PanelManager};
};


// public API (PIMPL)

osc::CStringView osc::FrameDefinitionTab::id() noexcept
{
    return c_TabStringID;
}

osc::FrameDefinitionTab::FrameDefinitionTab(std::weak_ptr<TabHost> parent_) :
    m_Impl{std::make_unique<Impl>(std::move(parent_))}
{
}

osc::FrameDefinitionTab::FrameDefinitionTab(FrameDefinitionTab&&) noexcept = default;
osc::FrameDefinitionTab& osc::FrameDefinitionTab::operator=(FrameDefinitionTab&&) noexcept = default;
osc::FrameDefinitionTab::~FrameDefinitionTab() noexcept = default;

osc::UID osc::FrameDefinitionTab::implGetID() const
{
    return m_Impl->getID();
}

osc::CStringView osc::FrameDefinitionTab::implGetName() const
{
    return m_Impl->getName();
}

void osc::FrameDefinitionTab::implOnMount()
{
    m_Impl->onMount();
}

void osc::FrameDefinitionTab::implOnUnmount()
{
    m_Impl->onUnmount();
}

bool osc::FrameDefinitionTab::implOnEvent(SDL_Event const& e)
{
    return m_Impl->onEvent(e);
}

void osc::FrameDefinitionTab::implOnTick()
{
    m_Impl->onTick();
}

void osc::FrameDefinitionTab::implOnDrawMainMenu()
{
    m_Impl->onDrawMainMenu();
}

void osc::FrameDefinitionTab::implOnDraw()
{
    m_Impl->onDraw();
}
