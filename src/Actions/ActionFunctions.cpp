#include "ActionFunctions.hpp"

#include "src/Bindings/SimTKHelpers.hpp"
#include "src/MiddlewareAPIs/MainUIStateAPI.hpp"
#include "src/OpenSimBindings/BasicModelStatePair.hpp"
#include "src/OpenSimBindings/ForwardDynamicSimulation.hpp"
#include "src/OpenSimBindings/ForwardDynamicSimulatorParams.hpp"
#include "src/OpenSimBindings/OpenSimHelpers.hpp"
#include "src/OpenSimBindings/Simulation.hpp"
#include "src/OpenSimBindings/StoFileSimulation.hpp"
#include "src/OpenSimBindings/TypeRegistry.hpp"
#include "src/OpenSimBindings/UndoableModelStatePair.hpp"
#include "src/Platform/App.hpp"
#include "src/Platform/Log.hpp"
#include "src/Platform/os.hpp"
#include "src/Tabs/ModelEditorTab.hpp"
#include "src/Tabs/LoadingTab.hpp"
#include "src/Tabs/SimulatorTab.hpp"
#include "src/Tabs/PerformanceAnalyzerTab.hpp"
#include "src/Utils/Algorithms.hpp"
#include "src/Utils/UID.hpp"
#include "src/Widgets/ObjectPropertiesEditor.hpp"

#include <OpenSim/Common/Component.h>
#include <OpenSim/Common/ComponentList.h>
#include <OpenSim/Common/ComponentPath.h>
#include <OpenSim/Common/ComponentSocket.h>
#include <OpenSim/Common/Exception.h>
#include <OpenSim/Common/ModelDisplayHints.h>
#include <OpenSim/Common/Property.h>
#include <OpenSim/Common/PropertyObjArray.h>
#include <OpenSim/Common/Set.h>
#include <OpenSim/Simulation/Model/ContactGeometry.h>
#include <OpenSim/Simulation/Model/GeometryPath.h>
#include <OpenSim/Simulation/Model/ModelVisualPreferences.h>
#include <OpenSim/Simulation/Model/OffsetFrame.h>
#include <OpenSim/Simulation/Model/PathActuator.h>
#include <OpenSim/Simulation/Model/PathPointSet.h>
#include <OpenSim/Simulation/Model/JointSet.h>
#include <OpenSim/Simulation/Model/HuntCrossleyForce.h>
#include <OpenSim/Simulation/Model/Model.h>
#include <OpenSim/Simulation/Model/PhysicalFrame.h>
#include <OpenSim/Simulation/Model/PhysicalOffsetFrame.h>
#include <OpenSim/Simulation/SimbodyEngine/Body.h>
#include <OpenSim/Simulation/SimbodyEngine/Coordinate.h>
#include <OpenSim/Simulation/SimbodyEngine/Joint.h>
#include <OpenSim/Simulation/SimbodyEngine/WeldJoint.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <memory>
#include <optional>
#include <sstream>
#include <typeinfo>
#include <utility>
#include <vector>

static void OpenOsimInLoadingTab(osc::MainUIStateAPI* api, std::filesystem::path p)
{
    osc::UID tabID = api->addTab<osc::LoadingTab>(api, p);
    api->selectTab(tabID);
}

static void DoOpenFileViaDialog(osc::MainUIStateAPI* api)
{
    std::filesystem::path p = osc::PromptUserForFile("osim");
    if (!p.empty())
    {
        OpenOsimInLoadingTab(api, p);
    }
}

static std::optional<std::filesystem::path> PromptSaveOneFile()
{
    std::filesystem::path p = osc::PromptUserForFileSaveLocationAndAddExtensionIfNecessary("osim");
    return !p.empty() ? std::optional{p} : std::nullopt;
}

static bool IsAnExampleFile(std::filesystem::path const& path)
{
    return osc::IsSubpath(osc::App::resource("models"), path);
}

static std::optional<std::string> TryGetModelSaveLocation(OpenSim::Model const& m)
{
    if (std::string const& backing_path = m.getInputFileName();
        backing_path != "Unassigned" && backing_path.size() > 0)
    {
        // the model has an associated file
        //
        // we can save over this document - *IF* it's not an example file
        if (IsAnExampleFile(backing_path))
        {
            auto maybePath = PromptSaveOneFile();
            return maybePath ? std::optional<std::string>{maybePath->string()} : std::nullopt;
        }
        else
        {
            return backing_path;
        }
    }
    else
    {
        // the model has no associated file, so prompt the user for a save
        // location
        auto maybePath = PromptSaveOneFile();
        return maybePath ? std::optional<std::string>{maybePath->string()} : std::nullopt;
    }
}

static bool TrySaveModel(OpenSim::Model const& model, std::string const& save_loc)
{
    try
    {
        model.print(save_loc);
        osc::log::info("saved model to %s", save_loc.c_str());
        return true;
    }
    catch (OpenSim::Exception const& ex)
    {
        osc::log::error("error saving model: %s", ex.what());
        return false;
    }
}

void osc::ActionSaveCurrentModelAs(osc::UndoableModelStatePair& uim)
{
    auto maybePath = PromptSaveOneFile();

    if (maybePath && TrySaveModel(uim.getModel(), maybePath->string()))
    {
        std::string oldPath = uim.getModel().getInputFileName();

        uim.updModel().setInputFileName(maybePath->string());
        uim.setFilesystemPath(*maybePath);

        if (*maybePath != oldPath)
        {
            uim.commit("changed osim path");
        }
        uim.setUpToDateWithFilesystem(std::filesystem::last_write_time(*maybePath));

        osc::App::upd().addRecentFile(*maybePath);
    }
}

void osc::ActionNewModel(MainUIStateAPI* api)
{
    auto p = std::make_unique<UndoableModelStatePair>();
    UID tabID = api->addTab<ModelEditorTab>(api, std::move(p));
    api->selectTab(tabID);
}

void osc::ActionOpenModel(MainUIStateAPI* api)
{
    DoOpenFileViaDialog(api);
}

void osc::ActionOpenModel(MainUIStateAPI* api, std::filesystem::path const& path)
{
    OpenOsimInLoadingTab(api, path);
}

bool osc::ActionSaveModel(MainUIStateAPI*, UndoableModelStatePair& model)
{
    std::optional<std::string> maybeUserSaveLoc = TryGetModelSaveLocation(model.getModel());

    if (maybeUserSaveLoc && TrySaveModel(model.getModel(), *maybeUserSaveLoc))
    {
        std::string oldPath = model.getModel().getInputFileName();
        model.updModel().setInputFileName(*maybeUserSaveLoc);
        model.setFilesystemPath(*maybeUserSaveLoc);

        if (*maybeUserSaveLoc != oldPath)
        {
            model.commit("changed osim path");
        }
        model.setUpToDateWithFilesystem(std::filesystem::last_write_time(*maybeUserSaveLoc));

        osc::App::upd().addRecentFile(*maybeUserSaveLoc);
        return true;
    }
    else
    {
        return false;
    }
}

void osc::ActionTryDeleteSelectionFromEditedModel(osc::UndoableModelStatePair& uim)
{
    OpenSim::Component const* selected = uim.getSelected();

    if (!selected)
    {
        return;
    }

    OpenSim::ComponentPath selectedPath = selected->getAbsolutePath();

    UID oldVersion = uim.getModelVersion();
    OpenSim::Model& mutModel = uim.updModel();
    OpenSim::Component* mutComponent = osc::FindComponentMut(mutModel, selectedPath);

    if (!mutComponent)
    {
        uim.setModelVersion(oldVersion);
        return;
    }

    std::string const selectedComponentName = mutComponent->getName();

    if (osc::TryDeleteComponentFromModel(mutModel, *mutComponent))
    {
        try
        {
            osc::InitializeModel(mutModel);
            osc::InitializeState(mutModel);

            std::stringstream ss;
            ss << "deleted " << selectedComponentName;
            uim.commit(std::move(ss).str());
        }
        catch (std::exception const& ex)
        {
            osc::log::error("error detected while deleting a component: %s", ex.what());
            uim.rollback();
        }
    }
    else
    {
        uim.setModelVersion(oldVersion);
    }
}

void osc::ActionUndoCurrentlyEditedModel(osc::UndoableModelStatePair& model)
{
    if (model.canUndo())
    {
        model.doUndo();
    }
}

void osc::ActionRedoCurrentlyEditedModel(osc::UndoableModelStatePair& model)
{
    if (model.canRedo())
    {
        model.doRedo();
    }
}

// disable all wrapping surfaces in the current model
void osc::ActionDisableAllWrappingSurfaces(osc::UndoableModelStatePair& model)
{
    try
    {
        OpenSim::Model& mutModel = model.updModel();
        osc::DeactivateAllWrapObjectsIn(mutModel);
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        model.commit("disabled all wrapping surfaces");
    }
    catch (std::exception const& ex)
    {
        osc::log::error("error detected while disabling wrapping surfaces: %s", ex.what());
        model.rollback();
    }
}

// enable all wrapping surfaces in the current model
void osc::ActionEnableAllWrappingSurfaces(osc::UndoableModelStatePair& model)
{
    try
    {
        OpenSim::Model& mutModel = model.updModel();
        osc::ActivateAllWrapObjectsIn(mutModel);
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        model.commit("enabled all wrapping surfaces");
    }
    catch (std::exception const& ex)
    {
        osc::log::error("error detected while enabling wrapping surfaces: %s", ex.what());
        model.rollback();
    }
}

void osc::ActionClearSelectionFromEditedModel(osc::UndoableModelStatePair& model)
{
    model.setSelected(nullptr);
}

bool osc::ActionLoadSTOFileAgainstModel(osc::MainUIStateAPI* parent, osc::UndoableModelStatePair const& uim, std::filesystem::path stoPath)
{
    try
    {
        std::unique_ptr<OpenSim::Model> cpy = std::make_unique<OpenSim::Model>(uim.getModel());
        osc::InitializeModel(*cpy);
        osc::InitializeState(*cpy);

        osc::UID tabID = parent->addTab<osc::SimulatorTab>(parent, std::make_shared<osc::Simulation>(osc::StoFileSimulation{std::move(cpy), stoPath, uim.getFixupScaleFactor()}));
        parent->selectTab(tabID);

        return true;
    }
    catch (std::exception const& ex)
    {
        osc::log::error("error detected while trying to load an STO file against the model: %s", ex.what());
        return false;
    }
}

bool osc::ActionStartSimulatingModel(osc::MainUIStateAPI* parent, osc::UndoableModelStatePair const& uim)
{
    osc::BasicModelStatePair modelState{uim};
    osc::ForwardDynamicSimulatorParams params = osc::FromParamBlock(parent->getSimulationParams());

    auto sim = std::make_shared<osc::Simulation>(osc::ForwardDynamicSimulation{std::move(modelState), std::move(params)});
    auto tab = std::make_unique<osc::SimulatorTab>(parent, std::move(sim));

    parent->selectTab(parent->addTab(std::move(tab)));

    return true;
}

bool osc::ActionUpdateModelFromBackingFile(osc::UndoableModelStatePair& uim)
{
    if (!uim.hasFilesystemLocation())
    {
        // there is no backing file?
        return false;
    }

    std::filesystem::path path = uim.getFilesystemPath();
    std::filesystem::file_time_type lastSaveTime = std::filesystem::last_write_time(path);

    if (uim.getLastFilesystemWriteTime() >= lastSaveTime)
    {
        // the backing file is probably up-to-date with the in-memory representation
        //
        // (e.g. because OSC just saved it and set the timestamp appropriately)
        return false;
    }

    // else: there is a backing file and it's newer than what's in-memory, so reload
    try
    {
        osc::log::info("file change detected: loading updated file");
        auto p = std::make_unique<OpenSim::Model>(uim.getModel().getInputFileName());
        osc::log::info("loaded updated file");
        uim.setModel(std::move(p));
        uim.commit("reloaded osim");
        uim.setUpToDateWithFilesystem(lastSaveTime);
        return true;
    }
    catch (std::exception const& ex)
    {
        osc::log::error("error detected while trying to automatically load a model file: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionCopyModelPathToClipboard(UndoableModelStatePair const& uim)
{
    if (!uim.hasFilesystemLocation())
    {
        // there is no backing file?
        return false;
    }

    std::filesystem::path absPath = std::filesystem::absolute(uim.getFilesystemPath());

    osc::SetClipboardText(absPath.string().c_str());

    return true;
}

bool osc::ActionAutoscaleSceneScaleFactor(osc::UndoableModelStatePair& uim)
{
    float sf = osc::GetRecommendedScaleFactor(uim);
    uim.setFixupScaleFactor(sf);
    return true;
}

bool osc::ActionToggleFrames(osc::UndoableModelStatePair& uim)
{
    try
    {
        OpenSim::Model& mutModel = uim.updModel();
        bool showingFrames = mutModel.get_ModelVisualPreferences().get_ModelDisplayHints().get_show_frames();
        mutModel.upd_ModelVisualPreferences().upd_ModelDisplayHints().set_show_frames(!showingFrames);
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        uim.commit(showingFrames ? "hidden frames" : "shown frames");
        return true;
    }
    catch (std::exception const& ex)
    {
        osc::log::error("error detected while trying to toggle frames: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionOpenOsimParentDirectory(osc::UndoableModelStatePair& uim)
{
    bool hasBackingFile = osc::HasInputFileName(uim.getModel());

    if (hasBackingFile)
    {
        std::filesystem::path p{uim.getModel().getInputFileName()};
        osc::OpenPathInOSDefaultApplication(p.parent_path());
        return true;
    }
    else
    {
        return false;
    }
}

bool osc::ActionOpenOsimInExternalEditor(osc::UndoableModelStatePair& uim)
{
    bool hasBackingFile = osc::HasInputFileName(uim.getModel());

    if (hasBackingFile)
    {
        osc::OpenPathInOSDefaultApplication(uim.getModel().getInputFileName());
        return true;
    }
    else
    {
        return false;
    }
}

bool osc::ActionReloadOsimFromDisk(osc::UndoableModelStatePair& uim)
{
    bool hasBackingFile = osc::HasInputFileName(uim.getModel());

    if (hasBackingFile)
    {
        try
        {
            osc::log::info("manual osim file reload requested: attempting to reload the file");
            auto p = std::make_unique<OpenSim::Model>(uim.getModel().getInputFileName());
            osc::log::info("loaded updated file");
            uim.setModel(std::move(p));
            uim.commit("reloaded from filesystem");
            uim.setUpToDateWithFilesystem(std::filesystem::last_write_time(uim.getFilesystemPath()));
            return true;
        }
        catch (std::exception const& ex)
        {
            osc::log::error("error detected while trying to reload a model file: %s", ex.what());
            uim.rollback();
            return false;
        }
    }
    else
    {
        osc::log::error("cannot reload the osim file: the model doesn't appear to have a backing file (is it saved?)");
        return false;
    }
}

bool osc::ActionSimulateAgainstAllIntegrators(osc::MainUIStateAPI* parent, osc::UndoableModelStatePair const& uim)
{
    osc::UID tabID = parent->addTab<osc::PerformanceAnalyzerTab>(parent, osc::BasicModelStatePair{uim}, parent->getSimulationParams());
    parent->selectTab(tabID);
    return true;
}

bool osc::ActionAddOffsetFrameToPhysicalFrame(osc::UndoableModelStatePair& uim, OpenSim::ComponentPath const& path)
{
    OpenSim::PhysicalFrame const* target = osc::FindComponent<OpenSim::PhysicalFrame>(uim.getModel(), path);

    if (!target)
    {
        return false;
    }

    std::string const newPofName = target->getName() + "_offsetframe";

    auto pof = std::make_unique<OpenSim::PhysicalOffsetFrame>();
    pof->setName(newPofName);
    pof->setParentFrame(*target);

    OpenSim::PhysicalOffsetFrame* pofptr = pof.get();

    UID oldVersion = uim.getModelVersion();

    try
    {
        OpenSim::Model& mutModel = uim.updModel();
        OpenSim::PhysicalFrame* mutTarget = osc::FindComponentMut<OpenSim::PhysicalFrame>(mutModel, path);

        if (!mutTarget)
        {
            uim.setModelVersion(oldVersion);
            return false;
        }

        mutTarget->addComponent(pof.release());
        mutModel.finalizeConnections();
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        uim.setSelected(pofptr);

        std::stringstream ss;
        ss << "added " << newPofName;
        uim.commit(std::move(ss).str());

        return true;
    }
    catch (std::exception const& ex)
    {
        osc::log::error("error detected while trying to add a frame to %s: %s", path.toString().c_str(), ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::CanRezeroJoint(osc::UndoableModelStatePair& uim, OpenSim::ComponentPath const& p)
{
    OpenSim::Joint const* joint = osc::FindComponent<OpenSim::Joint>(uim.getModel(), p);

    if (!joint)
    {
        return false;
    }

    // if the joint uses offset frames for both its parent and child frames then
    // it is possible to reorient those frames such that the joint's new zero
    // point is whatever the current arrangement is (effectively, by pre-transforming
    // the parent into the child and assuming a "zeroed" joint is an identity op)

    return osc::DerivesFrom<OpenSim::PhysicalOffsetFrame>(joint->getParentFrame());
}

bool osc::ActionRezeroJoint(osc::UndoableModelStatePair& uim, OpenSim::ComponentPath const& path)
{
    OpenSim::Joint const* target = osc::FindComponent<OpenSim::Joint>(uim.getModel(), path);

    if (!target)
    {
        // nothing/invalid component type specified
        return false;
    }

    OpenSim::PhysicalOffsetFrame const* parentPOF = dynamic_cast<OpenSim::PhysicalOffsetFrame const*>(&target->getParentFrame());

    if (!parentPOF)
    {
        // target has no parent
        return false;
    }

    OpenSim::PhysicalFrame const& childFrame = target->getChildFrame();

    SimTK::Transform parentXform = parentPOF->getTransformInGround(uim.getState());
    SimTK::Transform childXform = childFrame.getTransformInGround(uim.getState());
    SimTK::Transform child2parent = parentXform.invert() * childXform;
    SimTK::Transform newXform = parentPOF->getOffsetTransform() * child2parent;

    OpenSim::ComponentPath const& jointPath = path;
    OpenSim::ComponentPath parentPath = parentPOF->getAbsolutePath();
    UID oldVersion = uim.getModelVersion();

    try
    {
        OpenSim::Model& mutModel = uim.updModel();
        OpenSim::Joint* mutJoint = osc::FindComponentMut<OpenSim::Joint>(mutModel, jointPath);
        if (!mutJoint)
        {
            // cannot find mutable version of the joint
            uim.setModelVersion(oldVersion);
            return false;
        }

        OpenSim::PhysicalOffsetFrame* mutParent = osc::FindComponentMut<OpenSim::PhysicalOffsetFrame>(mutModel, parentPath);
        if (!mutParent)
        {
            // cannot find mutable version of the parent offset frame
            uim.setModelVersion(oldVersion);
            return false;
        }

        // else: perform model transformation

        std::string const jointName = mutJoint->getName();

        // first, zero all the joint's coordinates
        //
        // (we're assuming that the new transform performs the same function)
        for (int i = 0, nc = mutJoint->getProperty_coordinates().size(); i < nc; ++i)
        {
            OpenSim::Coordinate& c = mutJoint->upd_coordinates(i);
            c.setDefaultValue(0.0);
        }

        // then set the parent offset frame's transform to "do the work"
        mutParent->setOffsetTransform(newXform);

        // and then put the model back into a valid state, ready for committing etc.
        mutModel.finalizeConnections();
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);

        std::stringstream ss;
        ss << "rezeroed " << jointName;
        uim.commit(std::move(ss).str());

        return true;
    }
    catch (std::exception const& ex)
    {
        osc::log::error("error detected while trying to rezero a joint: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionAddParentOffsetFrameToJoint(osc::UndoableModelStatePair& uim, OpenSim::ComponentPath const& jointPath)
{
    OpenSim::Joint const* target = osc::FindComponent<OpenSim::Joint>(uim.getModel(), jointPath);

    if (!target)
    {
        return false;
    }

    auto pf = std::make_unique<OpenSim::PhysicalOffsetFrame>();
    pf->setParentFrame(target->getParentFrame());

    UID oldVersion = uim.getModelVersion();

    try
    {
        OpenSim::Model& mutModel = uim.updModel();
        OpenSim::Joint* mutJoint = osc::FindComponentMut<OpenSim::Joint>(mutModel, jointPath);

        if (!mutJoint)
        {
            uim.setModelVersion(oldVersion);
            return false;
        }

        std::string const jointName = mutJoint->getName();

        mutJoint->addFrame(pf.release());
        mutModel.finalizeConnections();
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);

        std::stringstream ss;
        ss << "added " << jointName;
        uim.commit(std::move(ss).str());

        return true;
    }
    catch (std::exception const& ex)
    {
        osc::log::error("error detected while trying to add a parent offset frame: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionAddChildOffsetFrameToJoint(osc::UndoableModelStatePair& uim, OpenSim::ComponentPath const& jointPath)
{
    OpenSim::Joint const* target = osc::FindComponent<OpenSim::Joint>(uim.getModel(), jointPath);

    if (!target)
    {
        return false;
    }

    auto pf = std::make_unique<OpenSim::PhysicalOffsetFrame>();
    pf->setParentFrame(target->getChildFrame());

    UID oldVersion = uim.getModelVersion();

    try
    {
        OpenSim::Model& mutModel = uim.updModel();
        OpenSim::Joint* mutJoint = osc::FindComponentMut<OpenSim::Joint>(mutModel, jointPath);

        if (!mutJoint)
        {
            uim.setModelVersion(oldVersion);
            return false;
        }

        std::string const jointName = mutJoint->getName();

        mutJoint->addFrame(pf.release());
        mutModel.finalizeConnections();
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);

        std::stringstream ss;
        ss << "added " << jointName;
        uim.commit(std::move(ss).str());

        return true;
    }
    catch (std::exception const& ex)
    {
        osc::log::error("error detected while trying to add a child offset frame: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionSetComponentName(osc::UndoableModelStatePair& uim, OpenSim::ComponentPath const& path, std::string const& newName)
{
    if (newName.empty())
    {
        return false;
    }

    OpenSim::Component const* target = osc::FindComponent(uim.getModel(), path);

    if (!target)
    {
        return false;
    }

    UID oldVersion = uim.getModelVersion();

    try
    {
        OpenSim::Model& mutModel = uim.updModel();
        OpenSim::Component* mutComponent = osc::FindComponentMut(mutModel, path);

        if (!mutComponent)
        {
            uim.setModelVersion(oldVersion);
            return false;
        }

        std::string const oldName = mutComponent->getName();
        mutComponent->setName(newName);
        mutModel.finalizeConnections();  // because pointers need to know the new name
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        uim.setSelected(mutComponent);  // because the name changed

        std::stringstream ss;
        ss << "renamed " << oldName << " to " << newName;
        uim.commit(std::move(ss).str());

        return true;
    }
    catch (std::exception const& ex)
    {
        osc::log::error("error detected while trying to set a component's name: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionChangeJointTypeTo(osc::UndoableModelStatePair& uim, OpenSim::ComponentPath const& jointPath, std::unique_ptr<OpenSim::Joint> newType)
{
    if (!newType)
    {
        osc::log::error("new joint type provided to ChangeJointType function is nullptr: cannot continue: this is a developer error and should be reported");
        return false;
    }

    OpenSim::Joint const* target = osc::FindComponent<OpenSim::Joint>(uim.getModel(), jointPath);

    if (!target)
    {
        return false;
    }

    OpenSim::JointSet const* owner = osc::GetOwner<OpenSim::JointSet>(*target);

    if (!owner)
    {
        return false;
    }

    OpenSim::ComponentPath ownerPath = owner->getAbsolutePath();

    int idx = FindJointInParentJointSet(*target);

    if (idx == -1)
    {
        return false;
    }

    std::string const oldTypeName = target->getConcreteClassName();
    std::string const newTypeName = newType->getConcreteClassName();

    osc::CopyCommonJointProperties(*target, *newType);

    // update: overwrite old joint in model
    //
    // note: this will invalidate the input joint, because the
    // OpenSim::JointSet container will automatically kill it

    UID oldVersion = uim.getModelVersion();

    try
    {
        OpenSim::Model& mutModel = uim.updModel();
        OpenSim::JointSet* mutParent = osc::FindComponentMut<OpenSim::JointSet>(mutModel, ownerPath);
        OpenSim::Joint* ptr = newType.get();

        mutParent->set(idx, newType.release());
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        uim.setSelected(ptr);

        std::stringstream ss;
        ss << "changed " << oldTypeName << " to " << newTypeName;
        uim.commit(std::move(ss).str());

        return true;
    }
    catch (std::exception const& ex)
    {
        osc::log::error("error detected while trying to change a joint's type: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionAttachGeometryToPhysicalFrame(osc::UndoableModelStatePair& uim, OpenSim::ComponentPath const& physFramePath, std::unique_ptr<OpenSim::Geometry> geom)
{

    OpenSim::PhysicalFrame const* target = osc::FindComponent<OpenSim::PhysicalFrame>(uim.getModel(), physFramePath);

    if (!target)
    {
        return false;
    }

    UID oldVersion = uim.getModelVersion();

    try
    {
        OpenSim::Model& mutModel = uim.updModel();
        OpenSim::PhysicalFrame* mutPof = osc::FindComponentMut<OpenSim::PhysicalFrame>(mutModel, physFramePath);

        if (!mutPof)
        {
            uim.setModelVersion(oldVersion);
            return false;
        }

        std::string const pofName = mutPof->getName();

        mutPof->attachGeometry(geom.release());
        mutModel.finalizeConnections();
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);

        std::stringstream ss;
        ss << "attached geometry to " << pofName;
        uim.commit(std::move(ss).str());

        return true;
    }
    catch (std::exception const& ex)
    {
        osc::log::error("error detected while trying to attach geometry to the a physical frame: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionAssignContactGeometryToHCF(osc::UndoableModelStatePair& uim, OpenSim::ComponentPath const& hcfPath, OpenSim::ComponentPath const& contactGeomPath)
{
    OpenSim::HuntCrossleyForce const* target = osc::FindComponent<OpenSim::HuntCrossleyForce>(uim.getModel(), hcfPath);
    if (!target)
    {
        return false;
    }

    OpenSim::ContactGeometry const* geom = osc::FindComponent<OpenSim::ContactGeometry>(uim.getModel(), contactGeomPath);
    if (!geom)
    {
        return false;
    }

    UID oldVersion = uim.getModelVersion();

    try
    {
        OpenSim::Model& mutModel = uim.updModel();
        OpenSim::HuntCrossleyForce* mutHCF = osc::FindComponentMut<OpenSim::HuntCrossleyForce>(mutModel, hcfPath);

        if (!mutHCF)
        {
            uim.setModelVersion(oldVersion);
            return false;
        }

        // HACK: if it has no parameters, give it some. The HuntCrossleyForce implementation effectively
        // does this internally anyway to satisfy its own API (e.g. `getStaticFriction` requires that
        // the HuntCrossleyForce has a parameter)
        if (mutHCF->get_contact_parameters().getSize() == 0)
        {
            mutHCF->updContactParametersSet().adoptAndAppend(new OpenSim::HuntCrossleyForce::ContactParameters());
        }

        mutHCF->updContactParametersSet()[0].updGeometry().appendValue(geom->getName());
        mutModel.finalizeConnections();

        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        uim.commit("added contact geometry");

        return true;
    }
    catch (std::exception const& ex)
    {
        osc::log::error("error detected while trying to assign contact geometry to a HCF: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionApplyPropertyEdit(osc::UndoableModelStatePair& uim, ObjectPropertyEdit& resp)
{
    UID oldVersion = uim.getModelVersion();

    try
    {
        OpenSim::Model& model = uim.updModel();

        OpenSim::Component* component = osc::FindComponentMut(model, resp.getComponentAbsPath());

        if (!component)
        {
            uim.setModelVersion(oldVersion);
            return false;
        }

        OpenSim::AbstractProperty* prop = osc::FindPropertyMut(*component, resp.getPropertyName());

        if (!prop)
        {
            uim.setModelVersion(oldVersion);
            return false;
        }

        std::string const propName = prop->getName();

        resp.apply(*prop);

        std::string const newValue = prop->toStringForDisplay(3);

        osc::InitializeModel(model);
        osc::InitializeState(model);

        std::stringstream ss;
        ss << "set " << propName << " to " << newValue;
        uim.commit(std::move(ss).str());

        return true;
    }
    catch (std::exception const& ex)
    {
        osc::log::error("error detected while trying to apply a property edit: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionAddPathPointToPathActuator(osc::UndoableModelStatePair& uim, OpenSim::ComponentPath const& pathActuatorPath, OpenSim::ComponentPath const& pointPhysFrame)
{
    OpenSim::PathActuator const* pa = osc::FindComponent<OpenSim::PathActuator>(uim.getModel(), pathActuatorPath);
    if (!pa)
    {
        return false;
    }

    OpenSim::PhysicalFrame const* pf = osc::FindComponent<OpenSim::PhysicalFrame>(uim.getModel(), pointPhysFrame);
    if (!pf)
    {
        return false;
    }

    int n = pa->getGeometryPath().getPathPointSet().getSize();
    std::string name = pa->getName() + "-P" + std::to_string(n + 1);
    SimTK::Vec3 pos{0.0f, 0.0f, 0.0f};

    try
    {
        OpenSim::Model& mutModel = uim.updModel();
        OpenSim::PathActuator* mutPA = osc::FindComponentMut<OpenSim::PathActuator>(mutModel, pathActuatorPath);
        OSC_ASSERT(mutPA);

        std::string const paName = mutPA->getName();

        mutPA->addNewPathPoint(name, *pf, pos);
        mutModel.finalizeConnections();
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);

        std::stringstream ss;
        ss << "added path point to " << paName;
        uim.commit(std::move(ss).str());

        return true;
    }
    catch (std::exception const& ex)
    {
        osc::log::error("error detected while trying to add a path point to a path actuator: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionReassignComponentSocket(
    osc::UndoableModelStatePair& uim,
    OpenSim::ComponentPath const& componentAbsPath,
    std::string const& socketName,
    OpenSim::Object const& connectee,
    std::string& error)
{
    OpenSim::Component const* target = osc::FindComponent(uim.getModel(), componentAbsPath);
    if (!target)
    {
        return false;
    }

    UID oldVersion = uim.getModelVersion();
    OpenSim::Model& mutModel = uim.updModel();

    OpenSim::Component* mutComponent = osc::FindComponentMut(mutModel, componentAbsPath);
    if (!mutComponent)
    {
        uim.setModelVersion(oldVersion);
        return false;
    }

    OpenSim::AbstractSocket* socket = osc::FindSocketMut(*mutComponent, socketName);
    if (!socket)
    {
        uim.setModelVersion(oldVersion);
        return false;
    }

    OpenSim::Object const& existing = socket->getConnecteeAsObject();

    try
    {
        socket->connect(connectee);
        mutModel.finalizeConnections();
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        uim.commit("reassigned socket");
        return true;
    }
    catch (std::exception const& ex)
    {
        error = ex.what();
        socket->connect(existing);
        uim.setModelVersion(oldVersion);
        return false;
    }
}

bool osc::ActionSetModelSceneScaleFactorTo(osc::UndoableModelStatePair& uim, float v)
{
    uim.setFixupScaleFactor(v);
    return true;
}

osc::BodyDetails::BodyDetails() :
    CenterOfMass{0.0f, 0.0f, 0.0f},
    Inertia{1.0f, 1.0f, 1.0f},
    Mass{1.0f},
    ParentFrameAbsPath{},
    BodyName{"new_body"},
    JointTypeIndex{static_cast<int>(osc::JointRegistry::indexOf<OpenSim::WeldJoint>().value_or(0))},
    JointName{},
    MaybeGeometry{nullptr},
    AddOffsetFrames{true}
{
}

// create a "standard" OpenSim::Joint
static std::unique_ptr<OpenSim::Joint> MakeJoint(
    osc::BodyDetails const& details,
    OpenSim::Body const& b,
    OpenSim::Joint const& jointPrototype,
    OpenSim::PhysicalFrame const& selectedPf)
{
    std::unique_ptr<OpenSim::Joint> copy{jointPrototype.clone()};
    copy->setName(details.JointName);

    if (!details.AddOffsetFrames)
    {
        copy->connectSocket_parent_frame(selectedPf);
        copy->connectSocket_child_frame(b);
    }
    else
    {
        // add first offset frame as joint's parent
        {
            auto pof1 = std::make_unique<OpenSim::PhysicalOffsetFrame>();
            pof1->setParentFrame(selectedPf);
            pof1->setName(selectedPf.getName() + "_offset");
            copy->addFrame(pof1.get());
            copy->connectSocket_parent_frame(*pof1.release());
        }

        // add second offset frame as joint's child
        {
            auto pof2 = std::make_unique<OpenSim::PhysicalOffsetFrame>();
            pof2->setParentFrame(b);
            pof2->setName(b.getName() + "_offset");
            copy->addFrame(pof2.get());
            copy->connectSocket_child_frame(*pof2.release());
        }
    }

    return copy;
}

bool osc::ActionAddBodyToModel(UndoableModelStatePair& uim, BodyDetails const& details)
{
    OpenSim::PhysicalFrame const* parent = osc::FindComponent<OpenSim::PhysicalFrame>(uim.getModel(), details.ParentFrameAbsPath);

    if (!parent)
    {
        return false;
    }

    SimTK::Vec3 com = ToSimTKVec3(details.CenterOfMass);
    SimTK::Inertia inertia = ToSimTKInertia(details.Inertia);
    double mass = static_cast<double>(details.Mass);

    // create body
    auto body = std::make_unique<OpenSim::Body>(details.BodyName, mass, com, inertia);

    // create joint between body and whatever the frame is
    OpenSim::Joint const& jointProto = *osc::JointRegistry::prototypes()[static_cast<size_t>(details.JointTypeIndex)];
    auto joint = MakeJoint(details, *body, jointProto, *parent);

    // attach decorative geom
    if (details.MaybeGeometry)
    {
        body->attachGeometry(details.MaybeGeometry->clone());
    }

    // mutate the model and perform the edit
    try
    {
        OpenSim::Model& mutModel = uim.updModel();
        mutModel.addJoint(joint.release());
        OpenSim::Body* ptr = body.get();
        mutModel.addBody(body.release());
        mutModel.finalizeConnections();
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);

        uim.setSelected(ptr);

        std::stringstream ss;
        ss << "added " << ptr->getName();
        uim.commit(std::move(ss).str());

        return true;
    }
    catch (std::exception const& ex)
    {
        osc::log::error("error detected while trying to add a body to the model: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionAddComponentToModel(UndoableModelStatePair& model, std::unique_ptr<OpenSim::Component> c)
{
    try
    {
        OpenSim::Model& mutModel = model.updModel();
        auto* ptr = c.get();
        AddComponentToModel(mutModel, std::move(c));
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        model.setSelected(ptr);
        std::string const name = ptr->getName();

        std::stringstream ss;
        ss << "added " << name;
        model.commit(std::move(ss).str());

        return true;
    }
    catch (std::exception const& ex)
    {
        osc::log::error("error detected while trying to add a component to the model: %s", ex.what());
        model.rollback();
        return false;
    }
}

bool osc::ActionSetCoordinateSpeed(UndoableModelStatePair& model, OpenSim::Coordinate const& coord, double v)
{
    auto coordPath = coord.getAbsolutePath();

    UID oldVersion = model.getModelVersion();

    try
    {
        OpenSim::Model& mutModel = model.updModel();
        OpenSim::Coordinate* mutCoord = osc::FindComponentMut<OpenSim::Coordinate>(mutModel, coordPath);

        if (!mutCoord)
        {
            // can't find the coordinate within the provided model
            model.setModelVersion(oldVersion);
            return false;
        }

        // PERF HACK: don't do a full model+state re-realization here: only do it
        //            when the caller wants to save the coordinate change
        mutCoord->setDefaultSpeedValue(v);
        mutCoord->setSpeedValue(mutModel.updWorkingState(), v);
        mutModel.equilibrateMuscles(mutModel.updWorkingState());
        mutModel.realizeDynamics(mutModel.updWorkingState());

        return true;
    }
    catch (std::exception const& ex)
    {
        osc::log::error("error detected while trying to set a coordinate's speed: %s", ex.what());
        model.rollback();
        return false;
    }
}

bool osc::ActionSetCoordinateSpeedAndSave(UndoableModelStatePair& model, OpenSim::Coordinate const& coord, double v)
{
    if (ActionSetCoordinateSpeed(model, coord, v))
    {
        OpenSim::Model& mutModel = model.updModel();
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);

        std::stringstream ss;
        ss << "set " << coord.getName() << "'s speed";
        model.commit(std::move(ss).str());

        return true;
    }
    else
    {
        // edit wasn't made
        return false;
    }
}

bool osc::ActionSetCoordinateLockedAndSave(UndoableModelStatePair& model, OpenSim::Coordinate const& coord, bool v)
{

    auto coordPath = coord.getAbsolutePath();
    UID oldVersion = model.getModelVersion();

    try
    {
        OpenSim::Model& mutModel = model.updModel();
        OpenSim::Coordinate* mutCoord = osc::FindComponentMut<OpenSim::Coordinate>(mutModel, coordPath);

        if (!mutCoord)
        {
            // can't find the coordinate within the provided model
            model.setModelVersion(oldVersion);
            return false;
        }

        mutCoord->setDefaultLocked(v);
        mutCoord->setLocked(mutModel.updWorkingState(), v);
        mutModel.equilibrateMuscles(mutModel.updWorkingState());
        mutModel.realizeDynamics(mutModel.updWorkingState());

        std::stringstream ss;
        ss << (v ? "locked " : "unlocked ") << mutCoord->getName();
        model.commit(std::move(ss).str());

        return true;
    }
    catch (std::exception const& ex)
    {
        osc::log::error("error detected while trying to lock a coordinate: %s", ex.what());
        model.rollback();
        return false;
    }
}

// set the value of a coordinate, but don't save it to the model (yet)
bool osc::ActionSetCoordinateValue(UndoableModelStatePair& model, OpenSim::Coordinate const& coord, double v)
{
    auto coordPath = coord.getAbsolutePath();
    UID oldVersion = model.getModelVersion();

    try
    {
        OpenSim::Model& mutModel = model.updModel();
        OpenSim::Coordinate* mutCoord = osc::FindComponentMut<OpenSim::Coordinate>(mutModel, coordPath);

        if (!mutCoord)
        {
            // can't find the coordinate within the provided model
            model.setModelVersion(oldVersion);
            return false;
        }

        double rangeMin = std::min(mutCoord->getRangeMin(), mutCoord->getRangeMax());
        double rangeMax = std::max(mutCoord->getRangeMin(), mutCoord->getRangeMax());

        if (!(rangeMin <= v && v <= rangeMax))
        {
            // the requested edit is outside the coordinate's allowed range
            model.setModelVersion(oldVersion);
            return false;
        }

        // PERF HACK: don't do a full model+state re-realization here: only do it
        //            when the caller wants to save the coordinate change
        mutCoord->setDefaultValue(v);
        mutCoord->setValue(mutModel.updWorkingState(), v);
        mutModel.equilibrateMuscles(mutModel.updWorkingState());
        mutModel.realizeDynamics(mutModel.updWorkingState());

        return true;
    }
    catch (std::exception const& ex)
    {
        osc::log::error("error detected while trying to set a coordinate's value: %s", ex.what());
        model.rollback();
        return false;
    }
}

// set the value of a coordinate and ensure it is saved into the model
bool osc::ActionSetCoordinateValueAndSave(UndoableModelStatePair& model, OpenSim::Coordinate const& coord, double v)
{
    if (ActionSetCoordinateValue(model, coord, v))
    {
        OpenSim::Model& mutModel = model.updModel();

        // CAREFUL: ensure that *all* coordinate's default values are updated to reflect
        //          the current state
        //
        // You might be thinking "but, the caller only wanted to set one coordinate". You're
        // right, but OpenSim models can contain constraints where editing one coordinate causes
        // a bunch of other coordinates to change.
        //
        // See #345 for a longer explanation
        for (OpenSim::Coordinate& c : mutModel.updComponentList<OpenSim::Coordinate>())
        {
            c.setDefaultValue(c.getValue(model.getState()));
        }

        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);

        std::stringstream ss;
        ss << "set " << coord.getName() << " to " << osc::ConvertCoordValueToDisplayValue(coord, v);
        model.commit(std::move(ss).str());

        return true;
    }
    else
    {
        // edit wasn't made
        return false;
    }
}

bool osc::ActionSetComponentAndAllChildrensIsVisibleTo(UndoableModelStatePair& model, OpenSim::ComponentPath const& path, bool visible)
{
    if (!FindComponent(model.getModel(), path))
    {
        return false;  // root component does not exist
    }

    // else: it does exist and we should start a mutation
    try
    {
        OpenSim::Model& mutModel = model.updModel();
        OpenSim::Component* mutComponent = osc::FindComponentMut(mutModel, path);
        OSC_ASSERT(mutComponent != nullptr && "the check above this line should already guarantee this");

        TrySetAppearancePropertyIsVisibleTo(*mutComponent, visible);
        for (OpenSim::Component& c : mutComponent->updComponentList())
        {
            TrySetAppearancePropertyIsVisibleTo(c, visible);
        }

        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);

        std::stringstream ss;
        ss << "set " << path.getComponentName() << " visibility to " << visible;
        model.commit(std::move(ss).str());
        return true;
    }
    catch (std::exception const& ex)
    {
        osc::log::error("error detected while trying to hide a component: %s", ex.what());
        model.rollback();
        return false;
    }
}

bool osc::ActionShowOnlyComponentAndAllChildren(UndoableModelStatePair& model, OpenSim::ComponentPath const& path)
{
    if (!FindComponent(model.getModel(), path))
    {
        return false;  // root component does not exist
    }

    // else: it does exist and we should start a mutation
    try
    {
        OpenSim::Model& mutModel = model.updModel();
        OpenSim::Component* mutComponent = osc::FindComponentMut(mutModel, path);
        OSC_ASSERT(mutComponent != nullptr && "the check above this line should already guarantee this");

        // first, hide everything in the model
        for (OpenSim::Component& c : mutModel.updComponentList())
        {
            TrySetAppearancePropertyIsVisibleTo(c, false);
        }

        // then show the intended component and its children
        TrySetAppearancePropertyIsVisibleTo(*mutComponent, true);
        for (OpenSim::Component& c : mutComponent->updComponentList())
        {
            TrySetAppearancePropertyIsVisibleTo(c, true);
        }

        // reinitialize etc.
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);

        std::stringstream ss;
        ss << "showing only " << path.getComponentName();
        model.commit(std::move(ss).str());
        return true;
    }
    catch (std::exception const& ex)
    {
        osc::log::error("error detected while trying to hide a component: %s", ex.what());
        model.rollback();
        return false;
    }
}