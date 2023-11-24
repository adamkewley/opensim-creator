#pragma once

#include <OpenSimCreator/Documents/MeshWarp/TPSDocument.hpp>
#include <OpenSimCreator/Documents/MeshWarp/TPSDocumentLandmarkPair.hpp>
#include <OpenSimCreator/Documents/MeshWarp/TPSDocumentInputIdentifier.hpp>
#include <OpenSimCreator/Utils/TPS3D.hpp>

#include <oscar/Graphics/Mesh.hpp>
#include <oscar/Maths/Vec3.hpp>
#include <oscar/Utils/EnumHelpers.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <sstream>
#include <utility>

// TPS document helper functions
namespace osc
{
    // returns the (mutable) source/destination of the given landmark pair, if available
    std::optional<Vec3>& UpdLocation(TPSDocumentLandmarkPair& landmarkPair, TPSDocumentInputIdentifier which)
    {
        static_assert(NumOptions<TPSDocumentInputIdentifier>() == 2);
        return which == TPSDocumentInputIdentifier::Source ? landmarkPair.maybeSourceLocation : landmarkPair.maybeDestinationLocation;
    }

    // returns the source/destination of the given landmark pair, if available
    std::optional<Vec3> const& GetLocation(TPSDocumentLandmarkPair const& landmarkPair, TPSDocumentInputIdentifier which)
    {
        static_assert(NumOptions<TPSDocumentInputIdentifier>() == 2);
        return which == TPSDocumentInputIdentifier::Source ? landmarkPair.maybeSourceLocation : landmarkPair.maybeDestinationLocation;
    }

    // returns `true` if the given landmark pair has a location assigned for `which`
    bool HasLocation(TPSDocumentLandmarkPair const& landmarkPair, TPSDocumentInputIdentifier which)
    {
        return GetLocation(landmarkPair, which).has_value();
    }

    // returns the (mutable) source/destination mesh in the given document
    Mesh& UpdMesh(TPSDocument& doc, TPSDocumentInputIdentifier which)
    {
        static_assert(NumOptions<TPSDocumentInputIdentifier>() == 2);
        return which == TPSDocumentInputIdentifier::Source ? doc.sourceMesh : doc.destinationMesh;
    }

    // returns the source/destination mesh in the given document
    Mesh const& GetMesh(TPSDocument const& doc, TPSDocumentInputIdentifier which)
    {
        static_assert(NumOptions<TPSDocumentInputIdentifier>() == 2);
        return which == TPSDocumentInputIdentifier::Source ? doc.sourceMesh : doc.destinationMesh;
    }

    // returns `true` if both the source and destination are defined for the given UI landmark
    bool IsFullyPaired(TPSDocumentLandmarkPair const& p)
    {
        return p.maybeSourceLocation && p.maybeDestinationLocation;
    }

    // returns `true` if the given UI landmark has either a source or a destination defined
    bool HasSourceOrDestinationLocation(TPSDocumentLandmarkPair const& p)
    {
        return p.maybeSourceLocation || p.maybeDestinationLocation;
    }

    // returns source + destination landmark pair, if both are fully defined; otherwise, returns std::nullopt
    std::optional<LandmarkPair3D> TryExtractLandmarkPair(TPSDocumentLandmarkPair const& p)
    {
        if (IsFullyPaired(p))
        {
            return LandmarkPair3D{*p.maybeSourceLocation, *p.maybeDestinationLocation};
        }
        else
        {
            return std::nullopt;
        }
    }

    // returns all fully paired landmarks in `doc`
    std::vector<LandmarkPair3D> GetLandmarkPairs(TPSDocument const& doc)
    {
        std::vector<LandmarkPair3D> rv;
        rv.reserve(std::count_if(doc.landmarkPairs.begin(), doc.landmarkPairs.end(), IsFullyPaired));

        for (TPSDocumentLandmarkPair const& p : doc.landmarkPairs)
        {
            if (std::optional<LandmarkPair3D> maybePair = TryExtractLandmarkPair(p))
            {
                rv.emplace_back(*maybePair);
            }
        }
        return rv;
    }

    // returns the positions of all non-participating landmarks in-order
    std::vector<Vec3> GetNonParticipatingLandmarkPositions(TPSDocument const& doc)
    {
        std::vector<Vec3> rv;
        rv.reserve(doc.nonParticipatingLandmarks.size());
        for (auto const& nonParticipatingLandmark : doc.nonParticipatingLandmarks)
        {
            rv.push_back(nonParticipatingLandmark.position);
        }
        return rv;
    }

    bool NonParticipatingLandmarkPositionsEqual(TPSDocument const& doc, std::span<Vec3 const> positions)
    {
        return std::equal(
            doc.nonParticipatingLandmarks.begin(),
            doc.nonParticipatingLandmarks.end(),
            positions.begin(),
            positions.end(),
            [](TPSDocumentNonParticipatingLandmark const& lm, Vec3 const& pos)
            {
                return lm.position == pos;
            }
        );
    }

    // returns the count of landmarks in the document for which `which` is defined
    size_t CountNumLandmarksForInput(TPSDocument const& doc, TPSDocumentInputIdentifier which)
    {
        auto const hasLocation = [which](TPSDocumentLandmarkPair const& p) { return HasLocation(p, which); };
        return std::count_if(doc.landmarkPairs.begin(), doc.landmarkPairs.end(), hasLocation);
    }

    // returns `true` if a landmark with the given name exists in the document
    bool ContainsLandmarkWithName(TPSDocument const& doc, StringName const& name)
    {
        auto const& pairs = doc.landmarkPairs;
        return std::any_of(pairs.begin(), pairs.end(), [&name](auto const& p) { return p.id == name; });
    }

    // returns the next available (presumably, unique) landmark ID
    StringName NextLandmarkID(TPSDocument const& doc)
    {
        // keep generating a name until an unused one is found
        for (size_t i = 0; i < std::numeric_limits<size_t>::max()-1; ++i)
        {
            std::stringstream ss;
            ss << "landmark_" << i;
            StringName name{std::move(ss).str()};

            if (!ContainsLandmarkWithName(doc, name))
            {
                return name;
            }
        }
        throw std::runtime_error{"unable to generate a unique name for the landmark"};
    }

    // helper: add a source/destination landmark at the given location
    void AddLandmarkToInput(
        TPSDocument& doc,
        TPSDocumentInputIdentifier which,
        Vec3 const& pos)
    {
        // first, try assigning it to an empty slot in the existing data
        //
        // (e.g. imagine the caller added a few source points and is now
        //       trying to add destination points - they should probably
        //       be paired in-sequence with the unpaired source points)
        bool wasAssignedToExistingEmptySlot = false;
        for (TPSDocumentLandmarkPair& p : doc.landmarkPairs)
        {
            std::optional<Vec3>& maybeLoc = UpdLocation(p, which);
            if (!maybeLoc)
            {
                maybeLoc = pos;
                wasAssignedToExistingEmptySlot = true;
                break;
            }
        }

        // if there wasn't an empty slot, then create a new landmark pair and
        // assign the location to the relevant part of the pair
        if (!wasAssignedToExistingEmptySlot)
        {
            TPSDocumentLandmarkPair& p = doc.landmarkPairs.emplace_back(NextLandmarkID(doc));
            UpdLocation(p, which) = pos;
        }
    }
}
