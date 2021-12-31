//
// UiController.h
//

#pragma once

#include "common/NavModule.h"

#include <mq/base/Signal.h>

#include <memory>

enum class TabPage {
	Navigation,
	Waypoints,
	Settings,
	Tools,
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
	mq::Signal<> OnUpdateUI;

	// signal for when a tab page is updated
	mq::Signal<TabPage> OnTabUpdate;

	bool IsUiOn() const;

	void PerformUpdateUI();
	void DrawNavSettingsPanel();

	virtual void OnBeginZone() override;
	virtual void OnEndZone() override;

private:
	void PerformUpdateTab(TabPage page);

	void DrawSettingsUI(bool fromSettingsPanel);

	int m_selectedTab = static_cast<int>(TabPage::Navigation);

	enum Section { Navigation, Mesh, Display };
	int m_settingsSection = Navigation;

	mq::Signal<>::ScopedConnection m_uiConn;
};
