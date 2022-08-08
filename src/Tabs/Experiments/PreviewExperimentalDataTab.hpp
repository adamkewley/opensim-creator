#pragma once

#include "src/Tabs/Tab.hpp"
#include "src/Utils/CStringView.hpp"
#include "src/Utils/UID.hpp"

#include <SDL_events.h>

namespace osc
{
	class TabHost;
}

namespace osc
{
	class PreviewExperimentalDataTab final : public Tab {
	public:
		PreviewExperimentalDataTab(TabHost*);
		PreviewExperimentalDataTab(PreviewExperimentalDataTab const&) = delete;
		PreviewExperimentalDataTab(PreviewExperimentalDataTab&&) noexcept;
		PreviewExperimentalDataTab& operator=(PreviewExperimentalDataTab const&) = delete;
		PreviewExperimentalDataTab& operator=(PreviewExperimentalDataTab&&) noexcept;
		~PreviewExperimentalDataTab() noexcept override;

	private:
		UID implGetID() const override;
		CStringView implGetName() const override;
		TabHost* implParent() const override;
		void implOnMount() override;
		void implOnUnmount() override;
		bool implOnEvent(SDL_Event const&) override;
		void implOnTick() override;
		void implOnDrawMainMenu() override;
		void implOnDraw() override;

		class Impl;
		Impl* m_Impl;
	};
}