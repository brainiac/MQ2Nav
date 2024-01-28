
#include "pch.h"
#include "ConsolePanel.h"

#include "meshgen/Application.h"
#include "meshgen/Logging.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <mutex>

//----------------------------------------------------------------------------

class ConsoleLogSink : public spdlog::sinks::base_sink<std::mutex>
{
public:
	ConsoleLogSink(ConsolePanel* panel)
		: m_panel(panel)
	{}

protected:
	void sink_it_(const spdlog::details::log_msg& msg) override;

	void flush_() override {}

private:
	ConsolePanel* m_panel;
};

void ConsoleLogSink::sink_it_(const spdlog::details::log_msg& msg)
{
	spdlog::memory_buf_t formatted;
	this->formatter_->format(msg, formatted);

	m_panel->AddLog(formatted.data(), formatted.data() + formatted.size());
}

static ConsoleLogSink* s_logSink = nullptr;

//----------------------------------------------------------------------------

ConsolePanel::ConsolePanel()
	: PanelWindow("Console Log", "ConsolePanel")
{
	m_sink = std::make_shared<ConsoleLogSink>(this);
	m_sink->set_level(spdlog::level::debug);

	g_logger->sinks().push_back(m_sink);
}

ConsolePanel::~ConsolePanel()
{
	std::erase(g_logger->sinks(), m_sink);
}

void ConsolePanel::Clear()
{
	m_buffer.clear();
	m_offsets.clear();
}

void ConsolePanel::AddLog(const char* str_begin, const char* str_end)
{
	int old_size = m_buffer.size();
	m_buffer.append(str_begin, str_end);
	for (int new_size = m_buffer.size(); old_size < new_size; old_size++)
	{
		if (m_buffer[old_size] == '\n')
			m_offsets.push_back(old_size);
	}
	m_scrollToBottom = true;
}

void ConsolePanel::OnImGuiRender(bool* p_open)
{
	ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

	if (ImGui::Begin(panelName.c_str(), p_open))
	{
		if (ImGui::Button("Clear"))
			Clear();
		ImGui::SameLine();
		bool copy = ImGui::Button("Copy");
		ImGui::SameLine();
		m_filter.Draw("Filter", -100.0f);
		ImGui::Separator();

		if (ImGui::BeginChild("scrolling"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));
			if (copy)
				ImGui::LogToClipboard();
			ImGui::PushFont(mq::imgui::ConsoleFont);

			if (m_filter.IsActive())
			{
				const char* buf_begin = m_buffer.begin();
				const char* line = buf_begin;
				for (int line_no = 0; line != nullptr; line_no++)
				{
					const char* line_end = (line_no < m_offsets.Size) ? buf_begin + m_offsets[line_no] : nullptr;
					if (m_filter.PassFilter(line, line_end))
						ImGui::TextUnformatted(line, line_end);
					line = line_end && line_end[1] ? line_end + 1 : nullptr;
				}
			}
			else
			{
				ImGui::TextUnformatted(m_buffer.begin());
			}

			ImGui::PopFont();
			if (m_scrollToBottom)
				ImGui::SetScrollHereY(1.0f);
			m_scrollToBottom = false;

			ImGui::PopStyleVar();
		}
		ImGui::EndChild();
	}
	ImGui::End();
}
