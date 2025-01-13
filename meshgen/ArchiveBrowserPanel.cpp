
#include "pch.h"
#include "ArchiveBrowserPanel.h"

#include "meshgen/Editor.h"
#include "meshgen/ZoneProject.h"
#include "meshgen/ZoneResourceManager.h"
#include "imgui/fonts/IconsFontAwesome.h"


ArchiveBrowserPanel::ArchiveBrowserPanel(Editor* editor)
	: PanelWindow("Archive Browser", "ArchivePane")
	, m_editor(editor)
{
}

ArchiveBrowserPanel::~ArchiveBrowserPanel()
{
}

void ArchiveBrowserPanel::DrawArchiveTable(ZoneResourceManager* resourceMgr)
{
	int archiveCount = resourceMgr->GetNumArchives();

	if (ImGui::BeginTable("##Archives", 2, ImGuiTableFlags_ScrollY))
	{
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80.0f);
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableHeadersRow();

		// TODO: Optimize by using a clipper (it will make this more complicated since we are using a tree structure...)

		for (int i = 0; i < archiveCount; ++i)
		{
			auto archive = resourceMgr->GetArchive(i);

			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			bool open = ImGui::TreeNodeEx(archive->GetFileName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
			ImGui::TableNextColumn();
			ImGui::Text("%d Files", archive->GetFileCount());

			if (open)
			{
				ImGui::PushID(archive);

				const auto& files = archive->GetSortedEntries();
				for (const auto& [fileName, fileEntry] : files)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("%.*s", (int)fileName.size(), fileName.data());

					ImGui::TableNextColumn();
					uint32_t size = fileEntry->uncompressedSize;
					if (size > 1024 * 1024)
					{
						ImGui::Text("%.2f MB", static_cast<float>(size) / (1024 * 1024));
					}
					else if (size > 1024)
					{
						ImGui::Text("%.2f KB", static_cast<float>(size) / 1024);
					}
					else
					{
						ImGui::Text("%d Bytes", size);
					}
				}

				ImGui::PopID();
				ImGui::TreePop();
			}
		}

		ImGui::EndTable();
	}
}

void ArchiveBrowserPanel::OnImGuiRender(bool* p_open)
{
	if (ImGui::Begin(panelName.c_str(), p_open))
	{
		if (!m_project)
		{
			ImGui::Text("No zone loaded");
		}
		else
		{
			auto resourceMgr = m_project->GetResourceManager();
			size_t archiveCount = resourceMgr->GetNumArchives();

			if (archiveCount == 0)
			{
				ImGui::Text("No open archives");
			}
			else
			{
				ImGui::Text("%d open archives", archiveCount);
				ImGui::Separator();

				DrawArchiveTable(resourceMgr);
			}
		}
	}

	ImGui::End();
}

void ArchiveBrowserPanel::OnProjectChanged(const std::shared_ptr<ZoneProject>& zoneProject)
{
	m_project = zoneProject;
}
