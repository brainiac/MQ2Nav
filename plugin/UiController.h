//
// UiController.h
//

#pragma once

#include "common/NavModule.h"
#include "common/Signal.h"

#include <memory>

enum class TabPage {
	Navigation,
	Waypoints,
	Settings,
	Tools,
#if defined(_DEBUG)
	Theme,
#endif

	Max
};

class UiController : public NavModule
{
public:
	UiController() {}
	virtual ~UiController() {}

	virtual void Initialize() override;
	virtual void Shutdown() override;

	// general signal for ui update.
	Signal<> OnUpdateUI;

	// signal for when a tab page is updated
	Signal<TabPage> OnTabUpdate;

	bool IsUiOn() const;

	virtual void OnBeginZone() override;
	virtual void OnEndZone() override;

private:
	void PerformUpdateUI();
	void PerformUpdateTab(TabPage page);

	int m_selectedTab = static_cast<int>(TabPage::Navigation);

	enum Section { Navigation, Mesh, Display };
	int m_settingsSection = Navigation;

	Signal<>::ScopedConnection m_uiConn;
};
