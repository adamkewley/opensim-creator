#include "App.hpp"

#include "osc_config.hpp"

#include "src/Bindings/SDL2Helpers.hpp"
#include "src/Graphics/Gl.hpp"
#include "src/Graphics/ShaderCache.hpp"
#include "src/Graphics/MeshCache.hpp"
#include "src/Platform/Config.hpp"
#include "src/Platform/Log.hpp"
#include "src/Platform/os.hpp"
#include "src/Platform/Screen.hpp"
#include "src/Platform/Styling.hpp"
#include "src/Screens/ErrorScreen.hpp"
#include "src/Utils/Algorithms.hpp"
#include "src/Utils/FilesystemHelpers.hpp"
#include "src/Utils/ScopeGuard.hpp"

#include <GL/glew.h>
#include <OpenSim/Common/Logger.h>
#include <OpenSim/Actuators/RegisterTypes_osimActuators.h>
#include <OpenSim/Analyses/RegisterTypes_osimAnalyses.h>
#include <OpenSim/Common/RegisterTypes_osimCommon.h>
#include <OpenSim/Simulation/Model/ModelVisualizer.h>
#include <OpenSim/Simulation/RegisterTypes_osimSimulation.h>
#include <OpenSim/Tools/RegisterTypes_osimTools.h>
#include <OpenSim/Common/LogSink.h>
#include <OpenSim/Common/Logger.h>
#include <imgui.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <imgui/backends/imgui_impl_sdl.h>

#include <fstream>
#include <locale>


// install backtrace dumper
//
// useful if the application fails in prod: can provide some basic backtrace
// info that users can paste into an issue or something, which is *a lot* more
// information than "yeah, it's broke"
static bool EnsureBacktraceHandlerEnabled()
{
    static bool enabledOnceGlobally = []()
    {
        osc::log::info("enabling backtrace handler");
        osc::InstallBacktraceHandler();
        return true;
    }();

    return enabledOnceGlobally;
}

// returns a resource from the config-provided `resources/` dir
static std::filesystem::path GetResource(osc::Config const& c, std::string_view p)
{
    return c.resourceDir / p;
}

namespace
{
    // an OpenSim log sink that sinks into OSC's main log
    class OpenSimLogSink final : public OpenSim::LogSink {
        void sinkImpl(std::string const& msg) override
        {
            osc::log::info("%s", msg.c_str());
        }
    };
}

// initialize OpenSim for osc
//
// this involves setting up OpenSim's log, registering types, dirs, etc.
static bool EnsureOpenSimInitialized(osc::Config const& config)
{
    static bool initializeOnceGlobally = [&config]()
    {
        // these are because OpenSim is inconsistient about handling locales
        //
        // it *writes* OSIM files using the locale, so you can end up with entries like:
        //
        //     <PathPoint_X>0,1323</PathPoint_X>
        //
        // but it *reads* OSIM files with the assumption that numbers will be in the format 'x.y'
        osc::log::info("setting locale to US (so that numbers are always in the format '0.x'");
        char const* locale = "C";
        osc::SetEnv("LANG", locale, 1);
        osc::SetEnv("LC_CTYPE", locale, 1);
        osc::SetEnv("LC_NUMERIC", locale, 1);
        osc::SetEnv("LC_TIME", locale, 1);
        osc::SetEnv("LC_COLLATE", locale, 1);
        osc::SetEnv("LC_MONETARY", locale, 1);
        osc::SetEnv("LC_MESSAGES", locale, 1);
        osc::SetEnv("LC_ALL", locale, 1);
#ifdef LC_CTYPE
        setlocale(LC_CTYPE, locale);
#endif
#ifdef LC_NUMERIC
        setlocale(LC_NUMERIC, locale);
#endif
#ifdef LC_TIME
        setlocale(LC_TIME, locale);
#endif
#ifdef LC_COLLATE
        setlocale(LC_COLLATE, locale);
#endif
#ifdef LC_MONETARY
        setlocale(LC_MONETARY, locale);
#endif
#ifdef LC_MESSAGES
        setlocale(LC_MESSAGES, locale);
#endif
#ifdef LC_ALL
        setlocale(LC_ALL, locale);
#endif
        std::locale::global(std::locale{locale});

        // disable OpenSim's `opensim.log` default
        //
        // by default, OpenSim creates an `opensim.log` file in the process's working
        // directory. This should be disabled because it screws with running multiple
        // instances of the UI on filesystems that use locking (e.g. Windows) and
        // because it's incredibly obnoxious to have `opensim.log` appear in every
        // working directory from which osc is ran
        osc::log::info("removing OpenSim's default log (opensim.log)");
        OpenSim::Logger::removeFileSink();

        // add OSC in-memory logger
        //
        // this logger collects the logs into a global mutex-protected in-memory structure
        // that the UI can can trivially render (w/o reading files etc.)
        osc::log::info("attaching OpenSim to this log");
        OpenSim::Logger::addSink(std::make_shared<OpenSimLogSink>());

        // explicitly load OpenSim libs
        //
        // this is necessary because some compilers will refuse to link a library
        // unless symbols from that library are directly used.
        //
        // Unfortunately, OpenSim relies on weak linkage *and* static library-loading
        // side-effects. This means that (e.g.) the loading of muscles into the runtime
        // happens in a static initializer *in the library*.
        //
        // osc may not link that library, though, because the source code in OSC may
        // not *directly* use a symbol exported by the library (e.g. the code might use
        // OpenSim::Muscle references, but not actually concretely refer to a muscle
        // implementation method (e.g. a ctor)
        osc::log::info("registering OpenSim types");
        RegisterTypes_osimCommon();
        RegisterTypes_osimSimulation();
        RegisterTypes_osimActuators();
        RegisterTypes_osimAnalyses();
        RegisterTypes_osimTools();

        // globally set OpenSim's geometry search path
        //
        // when an osim file contains relative geometry path (e.g. "sphere.vtp"), the
        // OpenSim implementation will look in these directories for that file
        osc::log::info("registering OpenSim geometry search path to use osc resources");
        std::filesystem::path applicationWideGeometryDir = GetResource(config, "geometry");
        OpenSim::ModelVisualizer::addDirToGeometrySearchPaths(applicationWideGeometryDir.string());
        osc::log::info("added geometry search path entry: %s", applicationWideGeometryDir.string().c_str());

        return true;
    }();

    return initializeOnceGlobally;
}

// handy macro for calling SDL_GL_SetAttribute with error checking
#define OSC_SDL_GL_SetAttribute_CHECK(attr, value)                                                                     \
    {                                                                                                                  \
        int rv = SDL_GL_SetAttribute((attr), (value));                                                                 \
        if (rv != 0) {                                                                                                 \
            throw std::runtime_error{std::string{"SDL_GL_SetAttribute failed when setting " #attr " = " #value " : "} +            \
                                     SDL_GetError()};                                                                  \
        }                                                                                                              \
    }

static char const* g_BaseWindowTitle = "OpenSim Creator v" OSC_VERSION_STRING;

// initialize the main application window
static sdl::Window CreateMainAppWindow()
{
    osc::log::info("initializing main application (OpenGL 3.3) window");

    OSC_SDL_GL_SetAttribute_CHECK(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    OSC_SDL_GL_SetAttribute_CHECK(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    OSC_SDL_GL_SetAttribute_CHECK(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    OSC_SDL_GL_SetAttribute_CHECK(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

    // careful about setting resolution, position, etc. - some people have *very* shitty
    // screens on their laptop (e.g. ultrawide, sub-HD, minus space for the start bar, can
    // be <700 px high)
    constexpr int x = SDL_WINDOWPOS_CENTERED;
    constexpr int y = SDL_WINDOWPOS_CENTERED;
    constexpr int width = 800;
    constexpr int height = 600;
    constexpr Uint32 flags =
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED;

    return sdl::CreateWindoww(g_BaseWindowTitle, x, y, width, height, flags);
}

// create an OpenGL context for an application window
static sdl::GLContext CreateOpenGLContext(SDL_Window* window)
{
    osc::log::info("initializing application OpenGL context");

    sdl::GLContext ctx = sdl::GL_CreateContext(window);

    // enable the context
    if (SDL_GL_MakeCurrent(window, ctx) != 0)
    {
        throw std::runtime_error{std::string{"SDL_GL_MakeCurrent failed: "} + SDL_GetError()};
    }

    // enable vsync by default
    //
    // vsync can feel a little laggy on some systems, but vsync reduces CPU usage
    // on *constrained* systems (e.g. laptops, which the majority of users are using)
    if (SDL_GL_SetSwapInterval(-1) != 0)
    {
        SDL_GL_SetSwapInterval(1);
    }

    // initialize GLEW
    //
    // effectively, enables the OpenGL API used by this application
    if (auto err = glewInit(); err != GLEW_OK)
    {
        std::stringstream ss;
        ss << "glewInit() failed: ";
        ss << glewGetErrorString(err);
        throw std::runtime_error{ss.str()};
    }

    // depth testing used to ensure geometry overlaps correctly
    glEnable(GL_DEPTH_TEST);

    // MSXAA is used to smooth out the model
    glEnable(GL_MULTISAMPLE);

    // all vertices in the render are backface-culled
    glEnable(GL_CULL_FACE);

    // print OpenGL information if in debug mode
    osc::log::info(
        "OpenGL initialized: info: %s, %s, (%s), GLSL %s",
        glGetString(GL_VENDOR),
        glGetString(GL_RENDERER),
        glGetString(GL_VERSION),
        glGetString(GL_SHADING_LANGUAGE_VERSION));

    return ctx;
}

// returns the maximum numbers of MSXAA samples the active OpenGL context supports
static GLint GetOpenGLMaxMSXAASamples(sdl::GLContext const&)
{
    GLint v = 1;
    glGetIntegerv(GL_MAX_SAMPLES, &v);

    // OpenGL spec: "the value must be at least 4"
    // see: https://www.khronos.org/registry/OpenGL-Refpages/es3.0/html/glGet.xhtml
    if (v < 4)
    {
        static bool warnOnce = [&]()
        {
            osc::log::warn("the current OpenGl backend only supports %i samples. Technically, this is invalid (4 *should* be the minimum)", v);
            return true;
        }();
        (void)warnOnce;
    }
    OSC_ASSERT(v < 1<<16 && "number of samples is greater than the maximum supported by the application");

    return v;
}

// maps an OpenGL debug message severity level to a log level
static constexpr osc::log::level::LevelEnum OpenGLDebugSevToLogLvl(GLenum sev) noexcept
{
    switch (sev) {
    case GL_DEBUG_SEVERITY_HIGH: return osc::log::level::err;
    case GL_DEBUG_SEVERITY_MEDIUM: return osc::log::level::warn;
    case GL_DEBUG_SEVERITY_LOW: return osc::log::level::debug;
    case GL_DEBUG_SEVERITY_NOTIFICATION: return osc::log::level::trace;
    default: return osc::log::level::info;
    }
}

// returns a string representation of an OpenGL debug message severity level
static constexpr char const* OpenGLDebugSevToCStr(GLenum sev) noexcept
{
    switch (sev) {
    case GL_DEBUG_SEVERITY_HIGH: return "GL_DEBUG_SEVERITY_HIGH";
    case GL_DEBUG_SEVERITY_MEDIUM: return "GL_DEBUG_SEVERITY_MEDIUM";
    case GL_DEBUG_SEVERITY_LOW: return "GL_DEBUG_SEVERITY_LOW";
    case GL_DEBUG_SEVERITY_NOTIFICATION: return "GL_DEBUG_SEVERITY_NOTIFICATION";
    default: return "GL_DEBUG_SEVERITY_UNKNOWN";
    }
}

// returns a string representation of an OpenGL debug message source
static constexpr char const* OpenGLDebugSrcToCStr(GLenum src) noexcept
{
    switch (src) {
    case GL_DEBUG_SOURCE_API: return "GL_DEBUG_SOURCE_API";
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM: return "GL_DEBUG_SOURCE_WINDOW_SYSTEM";
    case GL_DEBUG_SOURCE_SHADER_COMPILER: return "GL_DEBUG_SOURCE_SHADER_COMPILER";
    case GL_DEBUG_SOURCE_THIRD_PARTY: return "GL_DEBUG_SOURCE_THIRD_PARTY";
    case GL_DEBUG_SOURCE_APPLICATION: return "GL_DEBUG_SOURCE_APPLICATION";
    case GL_DEBUG_SOURCE_OTHER: return "GL_DEBUG_SOURCE_OTHER";
    default: return "GL_DEBUG_SOURCE_UNKNOWN";
    }
}

// returns a string representation of an OpenGL debug message type
static constexpr char const* OpenGLDebugTypeToCStr(GLenum type) noexcept
{
    switch (type) {
    case GL_DEBUG_TYPE_ERROR: return "GL_DEBUG_TYPE_ERROR";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR";
    case GL_DEBUG_TYPE_PORTABILITY: return "GL_DEBUG_TYPE_PORTABILITY";
    case GL_DEBUG_TYPE_PERFORMANCE: return "GL_DEBUG_TYPE_PERFORMANCE";
    case GL_DEBUG_TYPE_MARKER: return "GL_DEBUG_TYPE_MARKER";
    case GL_DEBUG_TYPE_PUSH_GROUP: return "GL_DEBUG_TYPE_PUSH_GROUP";
    case GL_DEBUG_TYPE_POP_GROUP: return "GL_DEBUG_TYPE_POP_GROUP";
    case GL_DEBUG_TYPE_OTHER: return "GL_DEBUG_TYPE_OTHER";
    default: return "GL_DEBUG_TYPE_UNKNOWN";
    }
}

// returns `true` if current OpenGL context is in debug mode
static bool IsOpenGLInDebugMode()
{
    // if context is not debug-mode, then some of the glGet*s below can fail
    // (e.g. GL_DEBUG_OUTPUT_SYNCHRONOUS on apple).
    {
        GLint flags;
        glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
        if (!(flags & GL_CONTEXT_FLAG_DEBUG_BIT))
        {
            return false;
        }
    }

    {
        GLboolean b = false;
        glGetBooleanv(GL_DEBUG_OUTPUT, &b);
        if (!b)
        {
            return false;
        }
    }

    {
        GLboolean b = false;
        glGetBooleanv(GL_DEBUG_OUTPUT_SYNCHRONOUS, &b);
        if (!b)
        {
            return false;
        }
    }

    return true;
}

// raw handler function that can be used with `glDebugMessageCallback`
static void OpenGLDebugMessageHandler(
    GLenum source,
    GLenum type,
    unsigned int id,
    GLenum severity,
    GLsizei,
    const char* message,
    void const*)
{
    osc::log::level::LevelEnum lvl = OpenGLDebugSevToLogLvl(severity);
    char const* sourceCStr = OpenGLDebugSrcToCStr(source);
    char const* typeCStr = OpenGLDebugTypeToCStr(type);
    char const* severityCStr = OpenGLDebugSevToCStr(severity);

    osc::log::log(lvl,
R"(OpenGL Debug message:
id = %u
message = %s
source = %s
type = %s
severity = %s
)", id, message, sourceCStr, typeCStr, severityCStr);
}

// enable OpenGL API debugging
static void EnableOpenGLDebugMessages()
{
    if (IsOpenGLInDebugMode())
    {
        osc::log::info("application appears to already be in OpenGL debug mode: skipping enabling it");
        return;
    }

    int flags;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT)
    {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(OpenGLDebugMessageHandler, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        osc::log::info("enabled OpenGL debug mode");
    }
    else
    {
        osc::log::error("cannot enable OpenGL debug mode: the context does not have GL_CONTEXT_FLAG_DEBUG_BIT set");
    }
}

// disable OpenGL API debugging
static void DisableOpenGLDebugMessages()
{
    if (!IsOpenGLInDebugMode())
    {
        osc::log::info("application does not need to disable OpenGL debug mode: already in it: skipping");
        return;
    }

    int flags;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT)
    {
        glDisable(GL_DEBUG_OUTPUT);
        osc::log::info("disabled OpenGL debug mode");
    }
    else
    {
        osc::log::error("cannot disable OpenGL debug mode: the context does not have a GL_CONTEXT_FLAG_DEBUG_BIT set");
    }
}

// returns refresh rate of highest refresh rate display on the computer
static int GetHighestRefreshRateDisplay()
{
    int numDisplays = SDL_GetNumVideoDisplays();

    if (numDisplays < 1)
    {
        return 60;  // this should be impossible but, you know, coding.
    }

    int highestRefreshRate = 30;
    SDL_DisplayMode modeStruct{};
    for (int display = 0; display < numDisplays; ++display)
    {
        int numModes = SDL_GetNumDisplayModes(display);
        for (int mode = 0; mode < numModes; ++mode)
        {
            SDL_GetDisplayMode(display, mode, &modeStruct);
            highestRefreshRate = std::max(highestRefreshRate, modeStruct.refresh_rate);
        }
    }
    return highestRefreshRate;
}

static void ImGuiApplyDarkTheme()
{
    // see: https://github.com/ocornut/imgui/issues/707
    // this one: https://github.com/ocornut/imgui/issues/707#issuecomment-512669512

    ImGui::GetStyle().FrameRounding = 2.0f;
    ImGui::GetStyle().GrabRounding = 20.0f;
    ImGui::GetStyle().GrabMinSize = 10.0f;

    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.20f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.09f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.12f, 0.14f, 0.65f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.39f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18f, 0.22f, 0.25f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.09f, 0.21f, 0.31f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.29f, 0.55f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_Tab] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}

// load the "recent files" file that osc persists to disk
static std::vector<osc::RecentFile> LoadRecentFilesFile(std::filesystem::path const& p)
{
    std::ifstream fd{p, std::ios::in};

    if (!fd)
    {
        // do not throw, because it probably shouldn't crash the application if this
        // is an issue
        osc::log::error("%s: could not be opened for reading: cannot load recent files list", p.string().c_str());
        return {};
    }

    std::vector<osc::RecentFile> rv;
    std::string line;

    while (std::getline(fd, line))
    {
        std::istringstream ss{line};

        // read line content
        uint64_t timestamp;
        std::filesystem::path path;
        ss >> timestamp;
        ss >> path;

        // calc tertiary data
        bool exists = std::filesystem::exists(path);
        std::chrono::seconds timestampSecs{timestamp};

        rv.push_back(osc::RecentFile{exists, std::move(timestampSecs), std::move(path)});
    }

    return rv;
}

// returns the filesystem path to the "recent files" file
static std::filesystem::path GetRecentFilesFilePath()
{
    return osc::GetUserDataDir() / "recent_files.txt";
}

// returns a unix timestamp in seconds since the epoch
static std::chrono::seconds GetCurrentTimeAsUnixTimestamp()
{
    return std::chrono::seconds(std::time(nullptr));
}

static osc::App::Clock::duration ConvertPerfTicksToFClockDuration(Uint64 ticks, Uint64 frequency)
{
    double dticks = static_cast<double>(ticks);
    double fq = static_cast<double>(frequency);
    float dur = static_cast<float>(dticks/fq);
    return osc::App::Clock::duration{dur};
}

static osc::App::Clock::time_point ConvertPerfCounterToFClock(Uint64 ticks, Uint64 frequency)
{
    return osc::App::Clock::time_point{ConvertPerfTicksToFClockDuration(ticks, frequency)};
}

// main application state
//
// this is what "booting the application" actually initializes
class osc::App::Impl final {
public:
    void show(std::unique_ptr<Screen> s)
    {
        log::info("showing screen %s", s->name());

        if (m_CurrentScreen)
        {
            throw std::runtime_error{"tried to call App::show when a screen is already being shown: you should use `requestTransition` instead"};
        }

        m_CurrentScreen = std::move(s);
        m_NextScreen.reset();

        // ensure retained screens are destroyed when exiting this guarded path
        //
        // this means callers can call .show multiple times on the same app
        OSC_SCOPE_GUARD({ m_CurrentScreen.reset(); });
        OSC_SCOPE_GUARD({ m_NextScreen.reset(); });

        // keep looping until `break` is hit, because the implementation may swap in
        // an error screen
        while (m_CurrentScreen)
        {
            try
            {
                mainLoopUnguarded();
                break;
            }
            catch (std::exception const& ex)
            {
                // if a screen was open when the exception was thrown, and that screen was not
                // an error screen, then transition to an error screen so that the user has a
                // chance to see the error
                if (m_CurrentScreen && !dynamic_cast<ErrorScreen*>(m_CurrentScreen.get()))
                {
                    m_CurrentScreen = std::make_unique<ErrorScreen>(ex);
                    m_NextScreen.reset();
                    // go to top of loop
                }
                else
                {
                    log::error("unhandled exception thrown in main render loop: %s", ex.what());
                    throw;
                }
            }
        }
    }

    void requestTransition(std::unique_ptr<Screen> s)
    {
        m_NextScreen = std::move(s);
    }

    void requestQuit()
    {
        m_QuitRequested = true;
    }

    glm::ivec2 idims() const
    {
        auto [w, h] = sdl::GetWindowSize(m_MainWindow);
        return glm::ivec2{w, h};
    }

    glm::vec2 dims() const
    {
        auto [w, h] = sdl::GetWindowSize(m_MainWindow);
        return glm::vec2{static_cast<float>(w), static_cast<float>(h)};
    }

    float aspectRatio() const
    {
        glm::vec2 v = dims();
        return v.x / v.y;
    }

    void setShowCursor(bool v)
    {
        SDL_ShowCursor(v ? SDL_ENABLE : SDL_DISABLE);
    }


    bool isWindowFocused() const
    {
        return SDL_GetWindowFlags(m_MainWindow) & SDL_WINDOW_INPUT_FOCUS;
    }

    void makeFullscreen()
    {
        SDL_SetWindowFullscreen(m_MainWindow, SDL_WINDOW_FULLSCREEN);
    }

    void makeWindowedFullscreen()
    {
        SDL_SetWindowFullscreen(m_MainWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }

    void makeWindowed()
    {
        SDL_SetWindowFullscreen(m_MainWindow, 0);
    }

    int getMSXAASamplesRecommended() const
    {
        return m_CurrentMSXAASamples;
    }

    void setMSXAASamplesRecommended(int s)
    {
        if (s <= 0)
        {
            throw std::runtime_error{"tried to set number of samples to <= 0"};
        }

        if (s > getMSXAASamplesMax())
        {
            throw std::runtime_error{"tried to set number of multisamples higher than supported by hardware"};
        }

        if (NumBitsSetIn(s) != 1)
        {
            throw std::runtime_error{"tried to set number of multisamples to an invalid value. Must be 1, or a multiple of 2 (1x, 2x, 4x, 8x...)"};
        }

        m_CurrentMSXAASamples = s;
    }

    int getMSXAASamplesMax() const
    {
        return m_MaxMSXAASamples;
    }

    bool isInDebugMode() const
    {
        return m_DebugModeEnabled;
    }

    void enableDebugMode()
    {
        if (IsOpenGLInDebugMode())
        {
            return;  // already in debug mode
        }

        log::info("enabling debug mode");
        EnableOpenGLDebugMessages();
        m_DebugModeEnabled = true;
    }

    void disableDebugMode()
    {
        if (!IsOpenGLInDebugMode())
        {
            return;  // already not in debug mode
        }

        log::info("disabling debug mode");
        DisableOpenGLDebugMessages();
        m_DebugModeEnabled = false;
    }

    bool isVsyncEnabled() const
    {
        // adaptive vsync (-1) and vsync (1) are treated as "vsync is enabled"
        return SDL_GL_GetSwapInterval() != 0;
    }

    void setVsync(bool v)
    {
        if (v)
        {
            enableVsync();
        }
        else
        {
            disableVsync();
        }
    }

    void enableVsync()
    {
        // try using adaptive vsync
        if (SDL_GL_SetSwapInterval(-1) == 0)
        {
            return;
        }

        // if adaptive vsync doesn't work, then try normal vsync
        if (SDL_GL_SetSwapInterval(1) == 0)
        {
            return;
        }

        // otherwise, setting vsync isn't supported by the system
    }

    void disableVsync()
    {
        SDL_GL_SetSwapInterval(0);
    }

    uint64_t getFrameCount() const
    {
        return m_FrameCounter;
    }

    uint64_t getTicks() const
    {
        return SDL_GetPerformanceCounter();
    }

    uint64_t getTickFrequency() const
    {
        return SDL_GetPerformanceFrequency();
    }

    osc::App::Clock::time_point getCurrentTime() const
    {
        return ConvertPerfCounterToFClock(SDL_GetPerformanceCounter(), m_AppCounterFq);
    }

    osc::App::Clock::time_point getAppStartupTime() const
    {
        return m_AppStartupTime;
    }

    osc::App::Clock::time_point getFrameStartTime() const
    {
        return m_FrameStartTime;
    }

    osc::App::Clock::duration getDeltaSinceLastFrame() const
    {
        return m_TimeSinceLastFrame;
    }

    bool isMainLoopWaiting() const
    {
        return m_InWaitMode;
    }

    void setMainLoopWaiting(bool v)
    {
        m_InWaitMode = v;
        requestRedraw();
    }

    void makeMainEventLoopWaiting()
    {
        setMainLoopWaiting(true);
    }

    void makeMainEventLoopPolling()
    {
        setMainLoopWaiting(false);
    }

    void requestRedraw()
    {
        SDL_Event e{};
        e.type = SDL_USEREVENT;
        m_NumFramesToPoll += 2;  // HACK: some parts of ImGui require rendering 2 frames before it shows something
        SDL_PushEvent(&e);
    }

    void clearScreen(glm::vec4 const& color)
    {
        gl::ClearColor(color.r, color.g, color.b, color.a);
        gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }


    osc::App::MouseState getMouseState() const
    {
        MouseState rv;

        glm::ivec2 mouseLocal;
        Uint32 ms = SDL_GetMouseState(&mouseLocal.x, &mouseLocal.y);
        rv.LeftDown = ms & SDL_BUTTON(SDL_BUTTON_LEFT);
        rv.RightDown = ms & SDL_BUTTON(SDL_BUTTON_RIGHT);
        rv.MiddleDown = ms & SDL_BUTTON(SDL_BUTTON_MIDDLE);
        rv.X1Down = ms & SDL_BUTTON(SDL_BUTTON_X1);
        rv.X2Down = ms & SDL_BUTTON(SDL_BUTTON_X2);

        if (isWindowFocused())
        {
            static bool canUseGlobalMouseState = strncmp(SDL_GetCurrentVideoDriver(), "wayland", 7) != 0;

            if (canUseGlobalMouseState)
            {
                glm::ivec2 mouseGlobal;
                SDL_GetGlobalMouseState(&mouseGlobal.x, &mouseGlobal.y);
                glm::ivec2 mouseWindow;
                SDL_GetWindowPosition(m_MainWindow, &mouseWindow.x, &mouseWindow.y);

                rv.pos = mouseGlobal - mouseWindow;
            }
            else
            {
                rv.pos = mouseLocal;
            }
        }

        return rv;
    }

    void warpMouseInWindow(glm::vec2 v) const
    {
        SDL_WarpMouseInWindow(m_MainWindow, static_cast<int>(v.x), static_cast<int>(v.y));
    }

    bool isShiftPressed() const
    {
        return SDL_GetModState() & KMOD_SHIFT;
    }

    bool isCtrlPressed() const
    {
        return SDL_GetModState() & KMOD_CTRL;
    }

    bool isAltPressed() const
    {
        return SDL_GetModState() & KMOD_ALT;
    }

    void setMainWindowSubTitle(std::string_view sv)
    {
        // use global + mutex to prevent hopping into the OS too much
        static std::string g_CurSubtitle = "";
        static std::mutex g_SubtitleMutex;

        std::lock_guard lock{g_SubtitleMutex};

        if (sv == g_CurSubtitle)
        {
            return;
        }

        g_CurSubtitle = sv;

        std::string newTitle = sv.empty() ? g_BaseWindowTitle : (std::string{sv} + " - " + g_BaseWindowTitle);
        SDL_SetWindowTitle(m_MainWindow, newTitle.c_str());
    }

    void unsetMainWindowSubTitle()
    {
        setMainWindowSubTitle("");
    }

    osc::Config const& getConfig() const
    {
        return *m_ApplicationConfig;
    }

    std::filesystem::path getResource(std::string_view p) const
    {
        return ::GetResource(*m_ApplicationConfig, p);
    }

    std::string slurpResource(std::string_view p) const
    {
        std::filesystem::path path = getResource(p);
        return SlurpFileIntoString(path);
    }

    std::vector<osc::RecentFile> getRecentFiles() const
    {
        std::filesystem::path p = GetRecentFilesFilePath();

        if (!std::filesystem::exists(p))
        {
            return {};
        }

        return LoadRecentFilesFile(p);
    }

    void addRecentFile(std::filesystem::path const& p)
    {
        std::filesystem::path recentFilesPath = GetRecentFilesFilePath();

        // load existing list
        std::vector<RecentFile> rfs;
        if (std::filesystem::exists(recentFilesPath))
        {
            rfs = LoadRecentFilesFile(recentFilesPath);
        }

        // clear potentially duplicate entries from existing list
        osc::RemoveErase(rfs, [&p](RecentFile const& rf) { return rf.path == p; });

        // write by truncating existing list file
        std::ofstream fd{recentFilesPath, std::ios::trunc};

        if (!fd)
        {
            osc::log::error("%s: could not be opened for writing: cannot update recent files list", recentFilesPath.string().c_str());
        }

        // re-serialize the n newest entries (the loaded list is sorted oldest -> newest)
        auto begin = rfs.end() - (rfs.size() < 10 ? static_cast<int>(rfs.size()) : 10);
        for (auto it = begin; it != rfs.end(); ++it)
        {
            fd << it->lastOpenedUnixTimestamp.count() << ' ' << it->path << std::endl;
        }

        // append the new entry
        fd << GetCurrentTimeAsUnixTimestamp().count() << ' ' << std::filesystem::absolute(p) << std::endl;
    }

    osc::ShaderCache& getShaderCache()
    {
        return m_ShaderCache;
    }

    osc::MeshCache& getMeshCache()
    {
        return m_MeshCache;
    }

    // used by ImGui backends

    sdl::Window& updWindow()
    {
        return m_MainWindow;
    }

    sdl::GLContext& updGLContext()
    {
        return m_OpenGLContext;
    }

private:
    // perform a screen transntion between two top-level `osc::Screen`s
    void transitionToNextScreen()
    {
        if (!m_NextScreen)
        {
            return;
        }

        log::info("unmounting screen %s", m_CurrentScreen->name());

        try
        {
            m_CurrentScreen->onUnmount();
        }
        catch (std::exception const& ex)
        {
            log::error("error unmounting screen %s: %s", m_CurrentScreen->name(), ex.what());
            m_CurrentScreen.reset();
            throw;
        }

        m_CurrentScreen.reset();
        m_CurrentScreen = std::move(m_NextScreen);

        // the next screen might need to draw a couple of frames
        // to "warm up" (e.g. because it's using ImGui)
        m_NumFramesToPoll = 2;

        log::info("mounting screen %s", m_CurrentScreen->name());
        m_CurrentScreen->onMount();
        log::info("transitioned main screen to %s", m_CurrentScreen->name());
    }

    // the main application loop
    //
    // this is what he application enters when it `show`s the first screen
    void mainLoopUnguarded()
    {
        // perform initial screen mount
        m_CurrentScreen->onMount();

        // ensure current screen is unmounted when exiting the main loop
        OSC_SCOPE_GUARD_IF(m_CurrentScreen, { m_CurrentScreen->onUnmount(); });

        // reset counters
        m_AppCounter = SDL_GetPerformanceCounter();
        m_FrameCounter = 0;
        m_FrameStartTime = ConvertPerfCounterToFClock(m_AppCounter, m_AppCounterFq);
        m_TimeSinceLastFrame = osc::App::Clock::duration{1.0f/60.0f};  // hack, for first frame

        while (true)  // gameloop
        {
            // pump events
            bool shouldWait = m_InWaitMode && m_NumFramesToPoll <= 0;
            m_NumFramesToPoll = std::max(0, m_NumFramesToPoll - 1);

            for (SDL_Event e; shouldWait ? SDL_WaitEventTimeout(&e, 1000) : SDL_PollEvent(&e);)
            {
                shouldWait = false;

                if (e.type == SDL_WINDOWEVENT)
                {
                    // window was resized and should be drawn a couple of times quickly
                    // to ensure any datastructures in the screens (namely: imgui) are
                    // updated
                    m_NumFramesToPoll = 2;
                }

                // let screen handle the event
                m_CurrentScreen->onEvent(e);

                if (m_QuitRequested)
                {
                    // screen requested application quit, so exit this function
                    return;
                }

                if (m_NextScreen)
                {
                    // screen requested a new screen, so perform the transition
                    transitionToNextScreen();
                }
            }

            // update clocks
            {
                auto counter = SDL_GetPerformanceCounter();

                Uint64 deltaTicks = counter - m_AppCounter;

                m_AppCounter = counter;
                m_FrameStartTime = ConvertPerfCounterToFClock(counter, m_AppCounterFq);
                m_TimeSinceLastFrame = ConvertPerfTicksToFClockDuration(deltaTicks, m_AppCounterFq);
            }

            // "tick" the screen
            m_CurrentScreen->tick(m_TimeSinceLastFrame.count());
            ++m_FrameCounter;

            if (m_QuitRequested)
            {
                // screen requested application quit, so exit this function
                return;
            }

            if (m_NextScreen)
            {
                // screen requested a new screen, so perform the transition
                transitionToNextScreen();
                continue;
            }

            // "draw" the screen into the window framebuffer
            m_CurrentScreen->draw();

            // "present" the rendered screen to the user (can block on VSYNC)
            SDL_GL_SwapWindow(m_MainWindow);

            if (m_QuitRequested)
            {
                // screen requested application quit, so exit this function
                return;
            }

            if (m_NextScreen)
            {
                // screen requested a new screen, so perform the transition
                transitionToNextScreen();
                continue;
            }
        }
    }

    // init/load the application config first
    std::unique_ptr<Config> m_ApplicationConfig = Config::load();

    // install the backtrace handler (if necessary - once per process)
    bool m_IsBacktraceHandlerInstalled = EnsureBacktraceHandlerEnabled();

    // init SDL context (windowing, etc.)
    sdl::Context m_SDLContext{SDL_INIT_VIDEO};

    // init main application window
    sdl::Window m_MainWindow = CreateMainAppWindow();

    // get performance counter frequency (for the delta clocks)
    Uint64 m_AppCounterFq = SDL_GetPerformanceFrequency();

    // current performance counter value (recorded once per frame)
    Uint64 m_AppCounter = 0;

    // number of frames the application has drawn
    uint64_t m_FrameCounter = 0;

    // when the application started up (set now)
    App::Clock::time_point m_AppStartupTime = ConvertPerfCounterToFClock(SDL_GetPerformanceCounter(), m_AppCounterFq);

    // when the current frame started (set each frame)
    App::Clock::time_point m_FrameStartTime = m_AppStartupTime;

    // time since the frame before the current frame (set each frame)
    App::Clock::duration m_TimeSinceLastFrame = {};

    // init OpenGL (globally)
    sdl::GLContext m_OpenGLContext = CreateOpenGLContext(m_MainWindow);

    // init global shader cache
    ShaderCache m_ShaderCache{};

    // init global mesh cache
    MeshCache m_MeshCache{};

    // figure out maximum number of samples supported by the OpenGL backend
    GLint m_MaxMSXAASamples = GetOpenGLMaxMSXAASamples(m_OpenGLContext);

    // how many samples the implementation should actually use
    GLint m_CurrentMSXAASamples = std::min(m_MaxMSXAASamples, m_ApplicationConfig->numMSXAASamples);

    // ensure OpenSim is initialized (logs, etc.)
    bool m_OpenSimInitialized = EnsureOpenSimInitialized(*m_ApplicationConfig);

    // set to true if the application should quit
    bool m_QuitRequested = false;

    // set to true if application is in debug mode
    bool m_DebugModeEnabled = false;

    // set to true if the main loop should pause on events
    //
    // CAREFUL: this makes the app event-driven
    bool m_InWaitMode = false;

    // set >0 to force that `n` frames are polling-driven: even in waiting mode
    int m_NumFramesToPoll = 0;

    // current screen being shown (if any)
    std::unique_ptr<Screen> m_CurrentScreen = nullptr;

    // the *next* screen the application should show
    std::unique_ptr<Screen> m_NextScreen = nullptr;
};

// public API

osc::App* osc::App::g_Current = nullptr;

osc::Config const& osc::App::config()
{
    return cur().getConfig();
}

osc::ShaderCache& osc::App::shaders()
{
    return cur().getShaderCache();
}

osc::MeshCache& osc::App::meshes()
{
    return cur().getMeshCache();
}

std::filesystem::path osc::App::resource(std::string_view s)
{
    return cur().getResource(s);
}

osc::App::App() : m_Impl{new Impl{}}
{
    g_Current = this;
}

osc::App::App(App&&) noexcept = default;

osc::App& osc::App::operator=(App&&) noexcept = default;

osc::App::~App() noexcept
{
    g_Current = nullptr;
}

void osc::App::show(std::unique_ptr<Screen> s)
{
    m_Impl->show(std::move(s));
}

void osc::App::requestTransition(std::unique_ptr<Screen> s)
{
    m_Impl->requestTransition(std::move(s));
}

void osc::App::requestQuit()
{
    m_Impl->requestQuit();
}

glm::ivec2 osc::App::idims() const
{
    return m_Impl->idims();
}

glm::vec2 osc::App::dims() const
{
    return m_Impl->dims();
}

float osc::App::aspectRatio() const
{
    return m_Impl->aspectRatio();
}

void osc::App::setShowCursor(bool v)
{
    m_Impl->setShowCursor(std::move(v));
}

bool osc::App::isWindowFocused() const
{
    return m_Impl->isWindowFocused();
}

void osc::App::makeFullscreen()
{
    m_Impl->makeFullscreen();
}

void osc::App::makeWindowedFullscreen()
{
    m_Impl->makeWindowedFullscreen();
}

void osc::App::makeWindowed()
{
    m_Impl->makeWindowed();
}

int osc::App::getMSXAASamplesRecommended() const
{
    return m_Impl->getMSXAASamplesRecommended();
}

void osc::App::setMSXAASamplesRecommended(int s)
{
    m_Impl->setMSXAASamplesRecommended(std::move(s));
}

int osc::App::getMSXAASamplesMax() const
{
    return m_Impl->getMSXAASamplesMax();
}

bool osc::App::isInDebugMode() const
{
    return m_Impl->isInDebugMode();
}

void osc::App::enableDebugMode()
{
    m_Impl->enableDebugMode();
}

void osc::App::disableDebugMode()
{
    m_Impl->disableDebugMode();
}

bool osc::App::isVsyncEnabled() const
{
    return m_Impl->isVsyncEnabled();
}

void osc::App::setVsync(bool v)
{
    m_Impl->setVsync(std::move(v));
}

void osc::App::enableVsync()
{
    m_Impl->enableVsync();
}

void osc::App::disableVsync()
{
    m_Impl->disableVsync();
}

uint64_t osc::App::getFrameCount() const
{
    return m_Impl->getFrameCount();
}

uint64_t osc::App::getTicks() const
{
    return m_Impl->getTicks();
}

uint64_t osc::App::getTickFrequency() const
{
    return m_Impl->getTickFrequency();
}

osc::App::Clock::time_point osc::App::getCurrentTime() const
{
    return m_Impl->getCurrentTime();
}

osc::App::Clock::time_point osc::App::getAppStartupTime() const
{
    return m_Impl->getAppStartupTime();
}

osc::App::Clock::time_point osc::App::getFrameStartTime() const
{
    return m_Impl->getFrameStartTime();
}

osc::App::Clock::duration osc::App::getDeltaSinceLastFrame() const
{
    return m_Impl->getDeltaSinceLastFrame();
}

bool osc::App::isMainLoopWaiting() const
{
    return m_Impl->isMainLoopWaiting();
}

void osc::App::setMainLoopWaiting(bool v)
{
    m_Impl->setMainLoopWaiting(std::move(v));
}

void osc::App::makeMainEventLoopWaiting()
{
    m_Impl->makeMainEventLoopWaiting();
}

void osc::App::makeMainEventLoopPolling()
{
    m_Impl->makeMainEventLoopPolling();
}

void osc::App::requestRedraw()
{
    m_Impl->requestRedraw();
}

void osc::App::clearScreen(glm::vec4 const& color)
{
    m_Impl->clearScreen(color);
}

osc::App::MouseState osc::App::getMouseState() const
{
    return m_Impl->getMouseState();
}

void osc::App::warpMouseInWindow(glm::vec2 v) const
{
    m_Impl->warpMouseInWindow(std::move(v));
}

bool osc::App::isShiftPressed() const
{
    return m_Impl->isShiftPressed();
}

bool osc::App::isCtrlPressed() const
{
    return m_Impl->isCtrlPressed();
}

bool osc::App::isAltPressed() const
{
    return m_Impl->isAltPressed();
}

void osc::App::setMainWindowSubTitle(std::string_view sv)
{
    m_Impl->setMainWindowSubTitle(std::move(sv));
}

void osc::App::unsetMainWindowSubTitle()
{
    m_Impl->unsetMainWindowSubTitle();
}

osc::Config const& osc::App::getConfig() const
{
    return m_Impl->getConfig();
}

std::filesystem::path osc::App::getResource(std::string_view p) const
{
    return m_Impl->getResource(std::move(p));
}

std::string osc::App::slurpResource(std::string_view p) const
{
    return m_Impl->slurpResource(std::move(p));
}

std::vector<osc::RecentFile> osc::App::getRecentFiles() const
{
    return m_Impl->getRecentFiles();
}

void osc::App::addRecentFile(std::filesystem::path const& p)
{
    m_Impl->addRecentFile(p);
}

osc::ShaderCache& osc::App::getShaderCache()
{
    return m_Impl->getShaderCache();
}

osc::MeshCache& osc::App::getMeshCache()
{
    return m_Impl->getMeshCache();
}

void osc::ImGuiInit()
{
    // init ImGui top-level context
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();

    // configure ImGui from OSC's (toml) configuration
    {
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        if (App::config().useMultiViewport)
        {
            io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        }
    }

    // make it so that windows can only ever be moved from the title bar
    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;

    // load application-level ImGui config, then the user one,
    // so that the user config takes precedence
    {
        std::string defaultIni = App::resource("imgui_base_config.ini").string();
        ImGui::LoadIniSettingsFromDisk(defaultIni.c_str());
        static std::string userIni = (osc::GetUserDataDir() / "imgui.ini").string();
        ImGui::LoadIniSettingsFromDisk(userIni.c_str());
        io.IniFilename = userIni.c_str();  // care: string has to outlive ImGui context
    }

    ImFontConfig baseConfig;
    baseConfig.SizePixels = 16.0f;
    baseConfig.PixelSnapH = true;
    baseConfig.OversampleH = 3;
    baseConfig.OversampleV = 2;
    std::string baseFontFile = App::resource("Ruda-Bold.ttf").string();
    io.Fonts->AddFontFromFileTTF(baseFontFile.c_str(), baseConfig.SizePixels, &baseConfig);

    // add FontAwesome icon support
    {
        ImFontConfig config = baseConfig;
        config.MergeMode = true;
        config.GlyphMinAdvanceX = std::floor(1.5f * config.SizePixels);
        config.GlyphMaxAdvanceX = std::floor(1.5f * config.SizePixels);
        static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        char const* file = "fa-solid-900.ttf";
        std::string fontFile = App::resource(file).string();
        io.Fonts->AddFontFromFileTTF(fontFile.c_str(), config.SizePixels, &config, icon_ranges);
    }

    // init ImGui for SDL2 /w OpenGL
    App::Impl& impl = *App::cur().m_Impl;
    ImGui_ImplSDL2_InitForOpenGL(impl.updWindow(), impl.updGLContext());

    // init ImGui for OpenGL
    ImGui_ImplOpenGL3_Init(OSC_GLSL_VERSION);

    ImGuiApplyDarkTheme();
}

void osc::ImGuiShutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

bool osc::ImGuiOnEvent(SDL_Event const& e)
{
    ImGui_ImplSDL2_ProcessEvent(&e);

    ImGuiIO const& io  = ImGui::GetIO();

    bool handledByImgui = false;

    if (io.WantCaptureKeyboard && (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP))
    {
        handledByImgui = true;
    }

    if (io.WantCaptureMouse && (e.type == SDL_MOUSEWHEEL || e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONUP || e.type == SDL_MOUSEBUTTONDOWN))
    {
        handledByImgui = true;
    }

    return handledByImgui;
}

void osc::ImGuiNewFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(App::cur().m_Impl->updWindow());
    ImGui::NewFrame();
}

void osc::ImGuiRender()
{
    gl::UseProgram();  // bound program can sometimes cause issues

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // ImGui: handle multi-viewports if the user has requested them
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        SDL_Window* backupCurrentWindow = SDL_GL_GetCurrentWindow();
        SDL_GLContext backupCurrentContext = SDL_GL_GetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent(backupCurrentWindow, backupCurrentContext);
    }
}
