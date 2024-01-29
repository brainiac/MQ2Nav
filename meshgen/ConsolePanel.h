
#pragma once

#include "meshgen/Logging.h"
#include "meshgen/PanelManager.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <deque>

class Application;
class ConsolePanel;

struct ConsoleMessage
{
	ConsoleMessage(const std::chrono::system_clock::time_point& timestamp, LoggingCategory category,
		spdlog::level::level_enum level, std::string_view message)
		: timestamp(timestamp),
		category(category),
		level(level),
		message(message)
	{
		std::time_t t = std::chrono::system_clock::to_time_t(timestamp);
		std::tm* lt = std::localtime(&t);

		strftime(timestampStr, sizeof(timestampStr), "%H:%M:%S", lt);

		auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()) % 1000;

		char temp[6];
		sprintf_s(temp, ".%03d", static_cast<int>(milliseconds.count()));
		strcat_s(timestampStr, temp);
	}

	std::chrono::system_clock::time_point timestamp;
	char timestampStr[13]; // HH:MM:SS.mmm
	LoggingCategory category;
	spdlog::level::level_enum level;
	std::string message;
};


class ConsoleLogSink : public spdlog::sinks::base_sink<std::mutex>
{
public:
	ConsoleLogSink();

	std::vector<ConsoleMessage> queuedMessages;

	void SetConsolePanel(ConsolePanel* console) { m_console = console; }

protected:
	void sink_it_(const spdlog::details::log_msg& msg) override;
	void flush_() override {}

private:
	ConsolePanel* m_console = nullptr;
};

extern std::shared_ptr<ConsoleLogSink> g_consoleSink;

class ConsolePanel : public PanelWindow
{
public:
	ConsolePanel();
	virtual ~ConsolePanel() override;

	virtual void OnImGuiRender(bool* p_open) override;

	void AddLog(const spdlog::details::log_msg& message);
	void AddLog(ConsoleMessage&& message);

private:
	void RenderToolbar(const ImVec2& size);
	void RenderLogs(const ImVec2& size);

	void RefilterMessages();

	bool MessageMatchesFilter(const ConsoleMessage& message)
	{
		if (m_categoryMask != 0xff && (m_categoryMask & (1 << static_cast<int>(message.category))) == 0)
			return false;

		if (m_levelMask != 0xff && (m_levelMask & (1 << static_cast<int>(message.level))) == 0)
			return false;

		if (!m_filter.PassFilter(message.message.c_str()))
			return false;

		return true;
	}

	std::shared_ptr<ConsoleLogSink> m_sink;
	std::deque<ConsoleMessage> m_messages;
	ImGuiTextFilter m_filter;
	uint8_t m_categoryMask = 0xf7; // bgfx off by default
	uint8_t m_levelMask = 0xfe;  // trace off by default
	std::vector<size_t> m_filteredMessages;
	bool m_scrollToBottom = false;
};
