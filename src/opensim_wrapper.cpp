#include "opensim_wrapper.hpp"

#include "cfg.hpp"

#include <OpenSim/Simulation/Model/Model.h>
#include <OpenSim/Simulation/Model/ModelVisualizer.h>

osmv::Model::Model(std::unique_ptr<OpenSim::Model> _m) noexcept : handle{std::move(_m)} {
}

osmv::Model::Model(Model const& m) : Model{static_cast<OpenSim::Model const&>(m)} {
}

osmv::Model::Model(std::filesystem::path const& p) : handle{nullptr} {
    // HACK: osmv has a `geometry/` dir packaged with it, which OpenSim
    //       should search in
    //
    // this sets a global in OpenSim, so only needs to be called once
    static bool _ = []() {
        std::filesystem::path geometry_dir = cfg::resource_path("geometry");
        OpenSim::ModelVisualizer::addDirToGeometrySearchPaths(geometry_dir.string());
        return true;
    }();
    (void)_;  // we don't actually use the boolean, we just want its side-effect

    handle = std::make_unique<OpenSim::Model>(p.string());
}
osmv::Model::Model(OpenSim::Model const& m) : handle{new OpenSim::Model{m}} {
}
osmv::Model::Model(Model&&) noexcept = default;
osmv::Model& osmv::Model::operator=(Model&&) noexcept = default;
osmv::Model::~Model() noexcept = default;

osmv::State::State(SimTK::State const& st) : handle{new SimTK::State(st)} {
}
osmv::State& osmv::State::operator=(SimTK::State const& st) {
    if (handle != nullptr) {
        *handle = st;
    } else {
        handle = std::make_unique<SimTK::State>(st);
    }
    return *this;
}
osmv::State::State(State&&) noexcept = default;
osmv::State& osmv::State::operator=(State&&) noexcept = default;
osmv::State::~State() noexcept = default;
