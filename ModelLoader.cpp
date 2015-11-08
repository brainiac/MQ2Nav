//
// ModelLoader.cpp
//

#include "ModelLoader.h"
#include "ImGuiRenderer.h"
#include "DebugDrawDX.h"
#include "Renderable.h"
#include "RenderHandler.h"

#include <imgui.h>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

#define GLM_FORCE_RADIANS
#include <glm.hpp>

#include <DebugDraw.h>
#include <d3d9types.h>

int s_matrixOffset = 0xE4;

class ModelData : public Renderable
{
public:
	ModelData(int doorId, BoundingBox& bb, IDirect3DDevice9* device)
		: m_doorId(doorId)
		, m_bb(bb)
	{
		m_grp = std::make_unique<RenderGroup>(device);
		RegenerateMesh();
	}

	~ModelData()
	{
		InvalidateDeviceObjects();
	}

	void SetColor(DWORD color)
	{
		if (color != m_color)
		{
			m_color = color;
			RegenerateMesh();
			InvalidateDeviceObjects();
		}
	}

	void SetTargetted(bool targetted)
	{
		m_targetted = targetted;
		UpdateColor();
	}
	bool IsTargetted() const { return m_targetted; }

	void SetHighlight(bool highlight)
	{
		m_highlight = highlight;
		UpdateColor();
	}
	bool IsHighlighted() const { return m_highlight; }

	void UpdateColor()
	{
		DWORD color = D3DCOLOR_RGBA(255, 255, 255, 255);

		if (m_targetted)
			color = D3DCOLOR_RGBA(0, 255, 0, 255);
		else if (m_highlight)
			color = D3DCOLOR_RGBA(0, 255, 255, 255);

		SetColor(color);
	}

	void RegenerateMesh()
	{
		DebugDrawDX dd(m_grp.get());

		duDebugDrawBoxWire(&dd, m_bb.min.x, m_bb.min.z, m_bb.min.y,
			m_bb.max.x, m_bb.max.z, m_bb.max.y, m_color, 1.0);
	}

	virtual void Render(RenderPhase phase) override
	{
		// find the door
		PDOORTABLE pDoorTable = (PDOORTABLE)pSwitchMgr;
		if (!pDoorTable) return;
		for (DWORD count = 0; count < pDoorTable->NumEntries; count++)
		{
			PDOOR door = pDoorTable->pDoor[count];
			if (door && door->ID == m_doorId)
			{
				if (m_visible && door->pSwitch)
				{
					ResetDeviceState();

					bool zDisabled = m_targetted || m_highlight;

					g_pDevice->SetRenderState(D3DRS_ZENABLE, !zDisabled);
					g_pDevice->SetRenderState(D3DRS_FOGENABLE, false);

					BYTE* b = (BYTE*)door->pSwitch;
					b += s_matrixOffset;

					D3DXMATRIX* mtx = (D3DXMATRIX*)b;

					m_grp->SetTransform(mtx);
					m_grp->Render(phase);
				}
			}
		}
	}

	virtual bool CreateDeviceObjects() override
	{
		return m_grp->CreateDeviceObjects();
	}

	virtual void InvalidateDeviceObjects() override
	{
		m_grp->InvalidateDeviceObjects();
	}

	void SetVisible(bool visible) { m_visible = visible; }
	bool IsVisible() const { return m_visible; }

private:
	BoundingBox m_bb;
	std::unique_ptr<RenderGroup> m_grp;
	DWORD m_color = D3DCOLOR_RGBA(255, 255, 255, 255);

	bool m_visible = true;
	int m_doorId = 0;
	bool m_highlight = false;
	bool m_targetted = false;
};

//----------------------------------------------------------------------------

const char* GetTeleportName(DWORD id)
{
	return "UNKNOWN";
}

ModelLoader::ModelLoader()
{
}

ModelLoader::~ModelLoader()
{
}

void ModelLoader::Initialize()
{
	m_uiConn = g_imguiRenderer->OnUpdateUI.Connect([this]() { OnUpdateUI(); });
}

void ModelLoader::Shutdown()
{
	m_uiConn.Disconnect();
}

DWORD lastDoorTargetId = 0;

void ModelLoader::Process()
{
	DWORD doorTargetId = 0;
	if (pDoorTarget)
		doorTargetId = pDoorTarget->ID;
	if (doorTargetId != lastDoorTargetId)
	{
		if (lastDoorTargetId)
		{
			if (auto model = m_modelData[lastDoorTargetId])
			{
				model->SetTargetted(false);
			}
		}

		lastDoorTargetId = doorTargetId;

		if (lastDoorTargetId)
		{
			if (auto model = m_modelData[lastDoorTargetId])
			{
				model->SetTargetted(true);
			}
		}
	}
}

void ModelLoader::SetZoneId(int zoneId)
{
	if (zoneId == m_zoneId)
		return;

	Reset();

	DebugSpewAlways("Model Loader, zone set to: %d", zoneId);
	m_zoneId = zoneId;

	const char* zoneName = GetShortZone(m_zoneId);
	CHAR szEQPath[MAX_STRING];
	GetEQPath(szEQPath);

	auto zoneData = std::make_unique<ZoneData>(szEQPath, zoneName);

	if (!zoneData->IsLoaded())
	{
		return;
	}

	m_zoneData = std::move(zoneData);

	PDOORTABLE pDoorTable = (PDOORTABLE)pSwitchMgr;
	for (DWORD count = 0; count < pDoorTable->NumEntries; count++)
	{
		PDOOR door = pDoorTable->pDoor[count];
		BoundingBox bb;

		// might end up moving the renderable generating into m_zoneData
		if (m_zoneData->GetBoundingBox(door->Name, bb))
		{
			// Create new model object
			std::shared_ptr<ModelData> md = std::make_shared<ModelData>(door->ID, bb, g_pDevice);
			//md->SetVisible(boost::iequals(door->Name, "OBJ_TABLE_MARKET_A"));
			m_modelData[door->ID] = md;

			g_renderHandler->AddRenderable(md);
		}
	}
}

void ModelLoader::Reset()
{
	m_zoneId = 0;
	m_zoneFile.clear();
	m_zoneData.reset();
	m_models.clear();

	for (auto iter = m_modelData.begin(); iter != m_modelData.end(); ++iter)
		g_renderHandler->RemoveRenderable(iter->second);
	m_modelData.clear();
}

void ModelLoader::OnUpdateUI()
{
	ImGui::Begin("Debug");

	if (ImGui::CollapsingHeader("Objects"))
	{
		//ImGui::LabelText("ZoneId", "%d", m_zoneId);
		//ImGui::LabelText("Models Loaded", "%d", m_models.size());
		//ImGui::LabelText("Filename", "%s", m_zoneFile.c_str());

		if (ImGui::TreeNode("##SwitchTable", "Objects"))
		{
			PDOORTABLE pDoorTable = (PDOORTABLE)pSwitchMgr;
			
			if (lastDoorTargetId)
			{
				PDOOR door = nullptr;
				for (DWORD count = 0; count < pDoorTable->NumEntries; count++)
				{
					if (pDoorTable->pDoor[count]->ID == lastDoorTargetId)
						door = pDoorTable->pDoor[count];
				}

				if (door)
				{
					ImGui::PushStyleColor(ImGuiCol_Text, ImColor(0, 255, 0));

					if (ImGui::TreeNode("Target Object", "Door Target: %s (%d)", door->Name, door->ID))
					{
						ImGui::PopStyleColor();

						RenderDoorObjectUI(door);

						ImGui::TreePop();
					}
					else
					{
						ImGui::PopStyleColor();
					}
				}
			}

			// temp for now, until we figure out where to put this...
			for (DWORD count = 0; count < pDoorTable->NumEntries; count++)
			{
				PDOOR door = pDoorTable->pDoor[count];
				ImGui::PushID(count);

				if (ImGui::TreeNode(door->Name, "%s (%d)", door->Name, door->ID))
				{
					RenderDoorObjectUI(door);

					ImGui::TreePop();
				}
				ImGui::PopID();
			}

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("##ItemTable", "Ground Items"))
		{
			PGROUNDITEM pItem = *(PGROUNDITEM*)pItemList;
			while (pItem)
			{
				if (ImGui::TreeNode(pItem->Name, "%s (%d)", pItem->Name, pItem->DropID))
				{
					ImGui::LabelText("Id", "%d", pItem->ID);
					ImGui::LabelText("DropId", "%d", pItem->DropID);
					ImGui::LabelText("Name", "%s", pItem->Name);
					ImGui::LabelText("Heading", "%.2f", pItem->Heading);
					ImGui::DragFloat3("Position", &pItem->Y);

					ImGui::TreePop();
				}

				pItem = pItem->pNext;
			}

			ImGui::TreePop();
		}
	}

	ImGui::End();
}

void ModelLoader::RenderDoorObjectUI(PDOOR door)
{
	auto model = m_modelData[door->ID];

	if (model)
	{
		bool visible = model->IsVisible();
		ImGui::Checkbox("Render", &visible); ImGui::SameLine();
		model->SetVisible(visible);

		bool highlight = model->IsHighlighted();
		ImGui::Checkbox("Highlight", &highlight);
		model->SetHighlight(highlight);
	}
	else
	{
		ImGui::TextColored(ImColor(255, 0, 0), "No Model Data");
	}

	ImGui::Separator();

	ImGui::Text("ID: %d Type: %d State: %d", door->ID, door->Type, door->State);
	if (door->ZonePoint != -1)
	{
		const char* zone = GetTeleportName(door->ZonePoint);
		ImGui::TextColored(ImColor(255, 255, 0), "Zone Point: %s (%d)", zone, door->ZonePoint);
	}

	ImGui::DragFloat3("Position", &door->Y);
	ImGui::DragFloat("Heading", &door->Heading);
	ImGui::DragFloat("Angle", &door->DoorAngle);


#if 0

	ImGui::DragFloat2("Top Speed", &door->TopSpeed1);

	ImGui::DragFloat3("Default Position", &door->DefaultY);
	ImGui::DragFloat("Default Heading", &door->DefaultHeading);
	ImGui::DragFloat("Default Angle", &door->DefaultDoorAngle);

	ImGui::LabelText("Switch?", "%s", door->pSwitch ? "true" : "false");
	ImGui::LabelText("Timestamp", "%d", door->TimeStamp);
#endif

	if (ImGui::Button("Reset Position"))
	{
		door->X = door->DefaultX;
		door->Y = door->DefaultY;
		door->Z = door->DefaultZ;
		door->Heading = door->DefaultHeading;
	}
}
