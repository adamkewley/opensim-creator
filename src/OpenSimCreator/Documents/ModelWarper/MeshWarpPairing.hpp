#pragma once

#include <OpenSimCreator/Documents/ModelWarper/LandmarkPairing.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <vector>

namespace osc::mow
{
    class MeshWarpPairing final {
    public:
        MeshWarpPairing(
            std::filesystem::path const& osimFilepath,
            std::filesystem::path const& sourceMeshFilepath
        );

        std::filesystem::path getSourceMeshAbsoluteFilepath() const;

        bool hasSourceLandmarksFilepath() const;
        std::filesystem::path recommendedSourceLandmarksFilepath() const;
        std::optional<std::filesystem::path> tryGetSourceLandmarksFilepath() const;

        bool hasDestinationMeshFilepath() const;
        std::filesystem::path recommendedDestinationMeshFilepath() const;
        std::optional<std::filesystem::path> tryGetDestinationMeshAbsoluteFilepath() const;

        bool hasDestinationLandmarksFilepath() const;
        std::filesystem::path recommendedDestinationLandmarksFilepath() const;
        std::optional<std::filesystem::path> tryGetDestinationLandmarksFilepath() const;

        size_t getNumLandmarks() const;
        size_t getNumSourceLandmarks() const;
        size_t getNumDestinationLandmarks() const;
        size_t getNumFullyPairedLandmarks() const;
        size_t getNumUnpairedLandmarks() const;
        bool hasUnpairedLandmarks() const;

        bool hasLandmarkNamed(std::string_view) const;
        LandmarkPairing const* tryGetLandmarkPairingByName(std::string_view) const;

    private:
        std::filesystem::path m_SourceMeshAbsoluteFilepath;

        std::filesystem::path m_ExpectedSourceLandmarksAbsoluteFilepath;
        bool m_SourceLandmarksFileExists;

        std::filesystem::path m_ExpectedDestinationMeshAbsoluteFilepath;
        bool m_DestinationMeshFileExists;

        std::filesystem::path m_ExpectedDestinationLandmarksAbsoluteFilepath;
        bool m_DestinationLandmarksFileExists;

        std::vector<LandmarkPairing> m_Landmarks;
    };
}
