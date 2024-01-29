
#include "pch.h"
#include "ConsolePanel.h"

#include "imgui/imgui_utilities.h"
#include "imgui/scoped_helpers.h"
#include "imgui/fonts/IconsFontAwesome.h"
#include "meshgen/Application.h"
#include "meshgen/Logging.h"

#include <fmt/chrono.h>
#include <spdlog/spdlog.h>

//----------------------------------------------------------------------------

static constexpr ImVec4 s_levelColors[spdlog::level::n_levels] = {
	ImVec4(0.3f, 0.3f, 0.3f, 1.0f),                     // trace
	ImVec4(0.6f, 0.6f, 0.6f, 1.0f),                     // debug
	ImVec4(0.0f, 0.44f, 1.0f, 1.0f),                    // info
	ImVec4(1.0f, 0.90f, 0.06f, 1.0f),                   // warning
	ImVec4(1.0f, 0.31f, 0.31f, 1.0f),                   // error
	ImVec4(1.0f, 0.0f, 0.0f, 1.0f),                     // critical
};

static constexpr ImVec4 s_levelColorsForText[spdlog::level::n_levels] = {
	ImVec4(0.3f, 0.3f, 0.3f, 1.0f),                     // trace
	ImVec4(0.6f, 0.6f, 0.6f, 1.0f),                     // debug
	ImVec4(1.0f, 1.0, 1.0f, 1.0f),                      // info
	ImVec4(1.0f, 0.90f, 0.06f, 1.0f),                   // warning
	ImVec4(1.0f, 0.31f, 0.31f, 1.0f),                   // error
	ImVec4(1.0f, 0.0f, 0.0f, 1.0f),                     // critical
};

static constexpr float s_toolbarHeight = 22.0f;

//----------------------------------------------------------------------------

ConsoleLogSink::ConsoleLogSink()
{
	set_level(spdlog::level::trace);
}

void ConsoleLogSink::sink_it_(const spdlog::details::log_msg& message)
{
	if (m_console)
		m_console->AddLog(message);
	else
	{
		queuedMessages.emplace_back(
			message.time,
			Logging::GetLoggingCategory(
				std::string_view(message.logger_name.data(), message.logger_name.size())),
			message.level,
			std::string_view(message.payload.data(), message.payload.size())
		);
	}
}

std::shared_ptr<ConsoleLogSink> g_consoleSink;

//----------------------------------------------------------------------------

ConsolePanel::ConsolePanel()
	: PanelWindow("Console Log", "ConsolePanel")
{
	m_sink = g_consoleSink;
	m_sink->SetConsolePanel(this);

	std::vector<ConsoleMessage> messages = std::move(m_sink->queuedMessages);
	for (auto&& message : messages)
		AddLog(std::move(message));
}

ConsolePanel::~ConsolePanel()
{
	m_sink->SetConsolePanel(nullptr);
}

void ConsolePanel::AddLog(const spdlog::details::log_msg& message)
{
	m_messages.emplace_back(
		message.time,
		Logging::GetLoggingCategory(
			std::string_view(message.logger_name.data(), message.logger_name.size())),
		message.level,
		std::string_view(message.payload.data(), message.payload.size())
	);

	const ConsoleMessage& newMsg = m_messages.back();
	if (MessageMatchesFilter(newMsg))
		m_filteredMessages.push_back(m_messages.size() - 1);
}

void ConsolePanel::AddLog(ConsoleMessage&& message)
{
	m_messages.push_back(std::move(message));

	const ConsoleMessage& newMsg = m_messages.back();
	if (MessageMatchesFilter(newMsg))
		m_filteredMessages.push_back(m_messages.size() - 1);
}

void ConsolePanel::OnImGuiRender(bool* p_open)
{
	ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

	if (ImGui::Begin(panelName.c_str(), p_open))
	{
		ImVec2 consoleSize = ImGui::GetContentRegionAvail();
		consoleSize.y -= 26.0f;

		RenderToolbar({ consoleSize.x, s_toolbarHeight });

		RenderLogs(consoleSize);
	}
	ImGui::End();
}

void ConsolePanel::RenderToolbar(const ImVec2& size)
{
	{
		mq::imgui::ScopedStyleStack styleScope(
		   ImGuiStyleVar_FrameBorderSize, 0.0f,
		   ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f)
	   );

		ImGui::BeginChild("Toolbar", size);
	}

	if (ImGui::Button("Clear", { 75.0f, s_toolbarHeight }))
	{
		m_messages.clear();
		m_filteredMessages.clear();
	}

	const auto& style = ImGui::GetStyle();
	ImGui::SameLine(0.0f, 16.0f);

	bool filterChanged = false;

	constexpr ImVec4 disabledButtonColor(0.1f, 0.1f, 0.1f, 1.0f);
	constexpr ImVec4 enabledButtonColor(0.2f, 0.2f, 0.2f, 1.0f);

	// Category filters
	for (int i = 0; i <= static_cast<int>(LoggingCategory::Other); ++i)
	{
		if (i != 0)
			ImGui::SameLine();

		LoggingCategory category = static_cast<LoggingCategory>(i);
		const char* name = Logging::GetLoggingCategoryName(category);
		bool enabled = (m_categoryMask & (1 << i)) != 0;

		mq::imgui::ScopedColorStack buttonColorScope(
			ImGuiCol_Button, enabled ? enabledButtonColor : disabledButtonColor
		);

		if (ImGui::Button(name, { 0.0f, s_toolbarHeight }))
		{
			m_categoryMask ^= (1 << i);
			filterChanged = true;
		}
	}

	ImGui::SameLine(0.0f, 16.0f);

	// Level filters
	{
		static constexpr ImVec2 buttonSize(s_toolbarHeight, s_toolbarHeight);

		for (int i = 0; i < spdlog::level::off; ++i)
		{
			if (i != 0)
				ImGui::SameLine();

			bool enabled = m_levelMask & (1 << i);
			const char* name = spdlog::level::short_level_names[i];

			{
				mq::imgui::ScopedColorStack buttonColorScope(
					ImGuiCol_Text, enabled ? s_levelColors[i] : style.Colors[ImGuiCol_TextDisabled],
					ImGuiCol_Button, enabled ? enabledButtonColor : disabledButtonColor
				);

				if (ImGui::Button(name, buttonSize))
				{
					m_levelMask ^= (1 << i);
					filterChanged = true;
				}
			}

			if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
			{
				char label[32] = { 0 };
				sprintf_s(label, "%.*s", static_cast<int>(spdlog::level::level_string_views[i].size()),
					spdlog::level::level_string_views[i].data());
				label[0] = ::toupper(label[0]);

				ImGui::SetTooltip(label);
			}
		}

		ImGui::SameLine(0.0f, 16.0f);

		if (m_filter.Draw("Filter", -60.0f))
		{
			filterChanged = true;
		}
	}

	ImGui::EndChild();

	if (filterChanged)
	{
		RefilterMessages();
	}
}

void ConsolePanel::RenderLogs(const ImVec2& size)
{
	mq::imgui::ScopedStyleStack tableStyle(
		ImGuiStyleVar_CellPadding, ImVec2(4.0f, 4.0f)
	);
	constexpr ImColor backgroundColor = IM_COL32(36, 36, 36, 255);
	mq::imgui::ScopedColorStack tableColor(
		ImGuiCol_TableRowBg, backgroundColor,
		ImGuiCol_TableRowBgAlt, mq::imgui::ColorWithMultipliedValue(backgroundColor, 1.2f),
		ImGuiCol_ChildBg, backgroundColor
	);

	ImGuiTableFlags flags = ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_BordersInnerV
		| ImGuiTableFlags_Reorderable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg;

	bool atBottom = false;

	if (ImGui::BeginTable("##Log", 4, flags, size))
	{
		const float cursorX = ImGui::GetCursorScreenPos().x;

		ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableSetupColumn("Timestamp", ImGuiTableColumnFlags_WidthFixed, 90.0f);
		ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 75.0f);
		ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);

		{
			// Headers

			const ImColor activeColor = mq::imgui::ColorWithMultipliedValue(backgroundColor, 1.3f);
			mq::imgui::ScopedColorStack headerColors(
				ImGuiCol_HeaderHovered, activeColor,
				ImGuiCol_HeaderActive, activeColor
			);

			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableNextRow(ImGuiTableRowFlags_Headers, 22.0f);

			for (int i = 0; i < 4; ++i)
			{
				constexpr float edgeOffset = 4.0f;
				ImGui::TableSetColumnIndex(i);
				const char* columnName = ImGui::TableGetColumnName(i);
				mq::imgui::ShiftCursor(edgeOffset * 3.0f, edgeOffset * 2.0f);
				ImGui::TableHeader(columnName);
				mq::imgui::ShiftCursor(-edgeOffset * 3.0f, -edgeOffset * 2.0f);
			}

			ImGui::SetCursorScreenPos(ImVec2(cursorX, ImGui::GetCursorScreenPos().y));
			mq::imgui::DrawUnderline(true, 0.0f, 9.0f);
		}

		// Draw contents
		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(m_filteredMessages.size()));

		while (clipper.Step())
		{
			for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
			{
				size_t pos = m_filteredMessages[index];
				const ConsoleMessage& message = m_messages[pos];

				ImGui::PushID(&message);

				{
					mq::imgui::ScopedColorStack rowColor(
						ImGuiCol_Text, s_levelColorsForText[message.level]
					);

					ImGui::TableNextRow();

					// Level
					ImGui::TableNextColumn();
					mq::imgui::ShiftCursorX(4.0f);

					auto levelSV = spdlog::level::level_string_views[message.level];
					ImGui::Text("%c%.*s", ::toupper(levelSV[0]), levelSV.size() - 1, levelSV.data() + 1);

					// Timestamp
					ImGui::TableNextColumn();
					mq::imgui::ShiftCursorX(4.0f);
					ImGui::Text("%s", message.timestampStr);

					// Category
					ImGui::TableNextColumn();
					mq::imgui::ShiftCursorX(4.0f);

					ImGui::Text("%s", Logging::GetLoggingCategoryName(message.category));

					// Message
					ImGui::TableNextColumn();
					mq::imgui::ShiftCursorX(4.0f);
					ImGui::Text("%s", message.message.c_str());
				}

				ImGui::PopID();
			}
		}

		if (m_scrollToBottom)
		{
			ImGui::SetScrollHereY(1.0f);
			m_scrollToBottom = false;
		}

		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
		{
			ImGui::SetScrollHereY(1.0f);
			atBottom = true;
		}

		ImGui::EndTable();
	}

	if (!atBottom)
	{
		ImGui::SetCursorPos({ImGui::GetContentRegionMax().x - 48.0f, ImGui::GetContentRegionMax().y - 36.0f});
		ImGui::BeginChild("ButtonContainer", ImVec2(28.0f, 28.0f));
		if (ImGui::Button((const char*)ICON_FA_ARROW_DOWN, { 28.0f, 28.0f }))
		{
			m_scrollToBottom = true;
		}
		ImGui::EndChild();
	}
}

void ConsolePanel::RefilterMessages()
{
	m_filteredMessages.clear();
	m_filteredMessages.reserve(m_messages.size());

	for (size_t i = 0; i < m_messages.size(); ++i)
	{
		const ConsoleMessage& message = m_messages[i];

		if (!MessageMatchesFilter(message))
			continue;

		m_filteredMessages.push_back(i);
	}
}
