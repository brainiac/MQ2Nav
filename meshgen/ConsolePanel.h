
#pragma once

#include "meshgen/PanelManager.h"

class Application;
class ConsoleLogSink;

class ConsolePanel : public PanelWindow
{
public:
	ConsolePanel();
	virtual ~ConsolePanel() override;

	virtual void OnImGuiRender(bool* p_open) override;

	void Clear();
	void AddLog(const char* str_begin, const char* str_end);

private:
	std::shared_ptr<ConsoleLogSink> m_sink;
	ImGuiTextBuffer m_buffer;
	ImGuiTextFilter m_filter;
	ImVector<int> m_offsets;
	bool m_scrollToBottom = false;
};

