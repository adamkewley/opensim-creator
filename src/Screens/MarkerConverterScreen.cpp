#include "MarkerConverterScreen.hpp"

#include "src/Platform/App.hpp"
#include "src/Platform/Screen.hpp"
#include "src/Utils/Assertions.hpp"

#include <imgui.h>

#include <memory>
#include <utility>

namespace
{
    class MarkerConverterState;

    class StateHolder {
    public:
        virtual ~StateHolder() noexcept = default;
        virtual void requestTransition(std::unique_ptr<MarkerConverterState>) = 0;
    };

    class MarkerConverterState {
    public:
        MarkerConverterState(StateHolder* stateHolder) : m_StateHolder{stateHolder}
        {
            OSC_ASSERT_ALWAYS(stateHolder != nullptr);
        }

        virtual ~MarkerConverterState() noexcept = default;
        virtual void onEvent(SDL_Event const&) {}
        virtual void tick(float) {}
        virtual void draw() = 0;

    protected:
        template<typename TState, typename... Args>
        void requestTransition(Args... args)
        {
            m_StateHolder->requestTransition(std::make_unique<TState>(m_StateHolder, std::forward(args)...));
        }

    private:
        StateHolder* m_StateHolder;
    };

    class MarkerConverterGetSecondModelState final : public MarkerConverterState {
        using MarkerConverterState::MarkerConverterState;

        void draw() override
        {
            ImGui::Begin("cookiecutter panel");
            ImGui::Text("second state");
            if (ImGui::Button("next"))
            {
                requestTransition<MarkerConverterGetSecondModelState>();
            }
            ImGui::End();
        }
    };

    class MarkerConverterGetFirstModelState final : public MarkerConverterState {
        using MarkerConverterState::MarkerConverterState;

        void draw() override
        {
            ImGui::Begin("cookiecutter panel");
            ImGui::Text("first state");
            if (ImGui::Button("next"))
            {
                requestTransition<MarkerConverterGetSecondModelState>();
            }
            ImGui::End();
        }
    };
}

struct osc::MarkerConverterScreen::Impl final : public StateHolder {

    void onMount()
    {
        osc::ImGuiInit();
    }

    void onUnmount()
    {
        osc::ImGuiShutdown();
    }

    void onEvent(SDL_Event const& e)
    {
        // called when the app receives an event from the operating system

        if (e.type == SDL_QUIT)
        {
            App::cur().requestQuit();
            return;
        }
        else if (osc::ImGuiOnEvent(e))
        {
            return;  // ImGui handled this particular event
        }
        else
        {
            m_CurrentState->onEvent(e);
            return;
        }
    }

    void tick(float dt)
    {
        m_CurrentState->tick(dt);
    }

    void draw()
    {
        if (m_NextState)
        {
            m_CurrentState = std::move(m_NextState);
        }

        // called once per frame. Code in here should use drawing primitives, OpenGL, ImGui,
        // etc. to draw things into the screen. The application does not clear the screen
        // buffer between frames (it's assumed that your code does this when it needs to)

        osc::ImGuiNewFrame();  // tell ImGui you're about to start drawing a new frame

        App::cur().clearScreen({0.0f, 0.0f, 0.0f, 0.0f});  // set app window bg color

        m_CurrentState->draw();

        osc::ImGuiRender();  // tell ImGui to render any ImGui widgets since calling ImGuiNewFrame();
    }

    void requestTransition(std::unique_ptr<MarkerConverterState> st) override
    {
        m_NextState = std::move(st);
    }

private:
    std::unique_ptr<MarkerConverterState> m_CurrentState{new MarkerConverterGetFirstModelState{this}};
    std::unique_ptr<MarkerConverterState> m_NextState = nullptr;
};

// public API

osc::MarkerConverterScreen::MarkerConverterScreen() :
    m_Impl{new Impl{}}
{
}

osc::MarkerConverterScreen::~MarkerConverterScreen() noexcept = default;

void osc::MarkerConverterScreen::onMount()
{
    m_Impl->onMount();
}

void osc::MarkerConverterScreen::onUnmount()
{
    m_Impl->onUnmount();
}

void osc::MarkerConverterScreen::onEvent(SDL_Event const& e)
{
    m_Impl->onEvent(e);
}

void osc::MarkerConverterScreen::tick(float dt)
{
    m_Impl->tick(dt);
}

void osc::MarkerConverterScreen::draw()
{
    m_Impl->draw();
}
