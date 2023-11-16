#pragma once

#include <OpenSimCreator/ModelGraph/SceneElClass.hpp>
#include <OpenSimCreator/ModelGraph/SceneElVariant.hpp>

#include <oscar/Maths/Vec3.hpp>

#include <array>
#include <variant>

namespace osc { class MeshEl; }
namespace osc { class SceneEl; }

namespace osc
{
    // returns true if a mesh can be attached to the given element
    bool CanAttachMeshTo(SceneEl const&);

    // returns `true` if a `StationEl` can be attached to the element
    bool CanAttachStationTo(SceneEl const&);

    // returns `true` if `EdgeEl` can be attached to `el`
    bool CanAttachEdgeTo(SceneEl const&);

    std::array<SceneElClass, std::variant_size_v<SceneElVariant>> const& GetSceneElClasses();

    Vec3 AverageCenter(MeshEl const&);
    Vec3 MassCenter(MeshEl const&);
}
