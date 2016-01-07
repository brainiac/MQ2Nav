//
// ModelLoader.cpp
//

#include "ModelLoader.h"
#include "ImGuiRenderer.h"
#include "DebugDrawDX.h"
#include "Renderable.h"
#include "RenderHandler.h"
#include "MQ2Nav_Util.h"

#include <imgui.h>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

#include <rapidjson/prettywriter.h>

#define GLM_FORCE_RADIANS
#include <glm.hpp>

#include <DebugDraw.h>
#include <d3d9types.h>

float GetDoorScale(PDOOR door)
{
	return (float)door->ScaleFactor / 100.0f;
}

#pragma region Object Model Debug Rendering

bool s_visibleOverride = false;
bool s_drawBoundingBoxes = false;

class ModelData : public Renderable
{
public:
	ModelData(int doorId, ModelInfo& bb, IDirect3DDevice9* device)
		: m_doorId(doorId)
		, m_bb(bb)
	{
		m_grpBB = std::make_unique<RenderGroup>(device);
#if RENDER_MODELS
		m_grpModel = std::make_unique<RenderGroup>(device);
#endif

		RegenerateMesh();
	}

	~ModelData()
	{
		InvalidateDeviceObjects();
	}

	bool SetColor(DWORD color)
	{
		if (color != m_color)
		{
			m_color = color;
			Rebuild();
			return true;
		}

		return false;
	}

	void SetTargetted(bool targetted)
	{
		if (m_targetted == targetted)
			return;

		m_targetted = targetted;

		if (!UpdateColor())
			Rebuild();
	}
	bool IsTargetted() const { return m_targetted; }

	void SetHighlight(bool highlight)
	{
		m_highlight = highlight;
		UpdateColor();
	}
	bool IsHighlighted() const { return m_highlight; }

	bool UpdateColor()
	{
		DWORD color = D3DCOLOR_RGBA(255, 255, 255, 255);

		if (m_targetted)
			color = D3DCOLOR_RGBA(0, 255, 0, 255);
		else if (m_highlight)
			color = D3DCOLOR_RGBA(0, 255, 255, 255);

		return SetColor(color);
	}

	void RegenerateMesh()
	{
		ModelInfo& bb = m_bb;
	
		{
			DebugDrawDX dd(m_grpBB.get());

			duDebugDrawBoxWire(&dd, bb.min.x, bb.min.z, bb.min.y,
				bb.max.x, bb.max.z, bb.max.y, m_color, 1.0);
		}

#if RENDER_MODELS
		if (!m_targetted)
			return;

		DebugDrawDX dd(m_grpModel.get());

		// iterate over all the things!
		if (bb.oldModel)
		{
			auto verts = bb.oldModel->GetVertices();
			auto polys = bb.oldModel->GetPolygons();

#define VERT(PX) { \
	glm::vec3 p0; const glm::vec3& p = verts[iter->verts[PX]].pos; \
	p0.x = p.y; \
	p0.y = p.z; \
	p0.z = p.x; \
	dd.vertex(&p0.x, 0xffffffff); \
}

			for (auto iter = polys.begin(); iter != polys.end(); ++iter)
			{
				//if (iter->flags & 0x11)
				//	continue;

				dd.begin(DU_DRAW_TRIS);

				VERT(0);
				VERT(1);
				VERT(2);

				dd.end();
			}
		}

		if (bb.newModel)
		{
			auto verts = bb.newModel->GetVertices();
			auto polys = bb.newModel->GetPolygons();

			for (auto iter = polys.begin(); iter != polys.end(); ++iter)
			{
				//if (iter->flags & 0x11)
				//	continue;
				dd.begin(DU_DRAW_TRIS);

				VERT(0);
				VERT(1);
				VERT(2);

				dd.end();
			}
		}
#endif
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

					g_pDevice->SetRenderState(D3DRS_FOGENABLE, false);

					D3DXMATRIX* mtx = (D3DXMATRIX*)(&door->pSwitch->transformMatrix);

					D3DXMATRIX scale;
					float scaleFactor = GetDoorScale(door);
					D3DXMatrixScaling(&scale, scaleFactor, scaleFactor, scaleFactor);

					scale = scale * *mtx;

#if RENDER_MODELS
					if (m_targetted)
					{
						m_grpModel->SetTransform(&scale);
						m_grpModel->Render(phase);
					}
#endif

					if (m_targetted || m_highlight || s_drawBoundingBoxes)
					{
						g_pDevice->SetRenderState(D3DRS_ZENABLE, !zDisabled && !s_visibleOverride);
						m_grpBB->SetTransform(&scale);
						m_grpBB->Render(phase);
					}
				}
			}
		}
	}

	virtual bool CreateDeviceObjects() override
	{
		m_grpBB->CreateDeviceObjects();
#if RENDER_MODELS
		m_grpModel->CreateDeviceObjects();
#endif

		return true;
	}

	virtual void InvalidateDeviceObjects() override
	{
		m_grpBB->InvalidateDeviceObjects();
#if RENDER_MODELS
		m_grpModel->InvalidateDeviceObjects();
#endif
	}

	void SetVisible(bool visible) { m_visible = visible; }
	bool IsVisible() const { return m_visible; }

	ModelInfo& GetBoundingBox() { return m_bb; }

private:
	void Rebuild()
	{
		m_grpBB->Reset();
#if RENDER_MODELS
		m_grpModel->Reset();
#endif
		RegenerateMesh();
	}

	ModelInfo m_bb;
	std::unique_ptr<RenderGroup> m_grpBB;
#if RENDER_MODELS
	std::unique_ptr<RenderGroup> m_grpModel;
#endif
	DWORD m_color = D3DCOLOR_RGBA(255, 255, 255, 255);

	glm::vec3 m_lastAdjust;

	bool m_visible = true;
	int m_doorId = 0;
	bool m_highlight = false;
	bool m_targetted = false;
};

#pragma endregion

//----------------------------------------------------------------------------

const char* GetTeleportName(DWORD id)
{
	return "UNKNOWN";
}

class DoorsDebugUI
{
public:
	enum DoorSortColumn {
		Sort_ID = 0,
		Sort_Name,
		Sort_Type,
		Sort_State,
		Sort_Distance
	};

	void Render();

	void Show()
	{
		m_showDoorsUI = true;
	}

	float GetDistance(PDOOR door);

	int m_doorsSortColumn = 0;
	bool m_sortReverse = false;
	bool m_showDoorsUI = false;
	int m_lastDoorTargetId = -1;

	std::map<DWORD, float> m_distanceCache;
	glm::vec3 m_lastPos;
};

ModelLoader::ModelLoader()
  : m_doorsUI(new DoorsDebugUI)
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

void ModelLoader::Process()
{
	if (m_zoneId == 0)
		return;
	if (gGameState != GAMESTATE_INGAME)
		return;

	DWORD doorTargetId = -1;
	if (pDoorTarget)
		doorTargetId = pDoorTarget->ID;
	if (doorTargetId != m_lastDoorTargetId)
	{
		if (m_lastDoorTargetId >= 0)
		{
			if (auto model = m_modelData[m_lastDoorTargetId])
			{
				model->SetTargetted(false);
			}
		}

		m_lastDoorTargetId = doorTargetId;
		m_doorsUI->m_lastDoorTargetId = doorTargetId;

		if (m_lastDoorTargetId >= 0)
		{
			if (auto model = m_modelData[m_lastDoorTargetId])
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
		ModelInfo bb;

		// might end up moving the renderable generating into m_zoneData
		if (m_zoneData->GetModelInfo(door->Name, bb))
		{
			// Create new model object
			std::shared_ptr<ModelData> md = std::make_shared<ModelData>(door->ID, bb, g_pDevice);
			m_modelData[door->ID] = md;

			g_renderHandler->AddRenderable(md);
		}
	}

	// dump the doors out to a config file
	DumpDoors();
}

void ModelLoader::DumpDoors()
{
	std::string filename = std::string(gszINIPath) + "\\MQ2Nav";

	// make sure directory exists so we can write to it!
	boost::system::error_code ec;
	boost::filesystem::create_directory(filename, ec);

	// filename for the door file
	const char* zoneName = GetShortZone(m_zoneId);
	filename = filename + "\\" + zoneName + "_doors.json";

	std::ofstream file(filename);
	if (!file.is_open())
		return;

	rapidjson::StringBuffer sb;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);

	writer.StartArray();

	PDOORTABLE pDoorTable = (PDOORTABLE)pSwitchMgr;
	for (DWORD count = 0; count < pDoorTable->NumEntries; count++)
	{
		if (PDOOR door = pDoorTable->pDoor[count])
		{
			writer.StartObject();

			writer.String("ID"); writer.Int(door->ID);
			writer.String("Name"); writer.String(door->Name);
			writer.String("Type"); writer.Int(door->Type);
			writer.String("Scale"); writer.Double((float)door->ScaleFactor / 100.0f);

			if (door->pSwitch)
			{
				writer.String("Transform");
				writer.StartArray();
				for (int i = 0; i < 4; i++)
				{
					writer.StartArray();
					for (int j = 0; j < 4; j++)
					{
						writer.Double(door->pSwitch->transformMatrix.m[i][j]);
					}
					writer.EndArray();
				}
				writer.EndArray();
			}

			writer.EndObject();
		}
	}

	writer.EndArray();

	file.write(sb.GetString(), sb.GetSize());
}

void ModelLoader::Reset()
{
	m_zoneId = 0;
	m_zoneFile.clear();
	m_zoneData.reset();
	m_models.clear();
	m_lastDoorTargetId = -1;

	for (auto iter = m_modelData.begin(); iter != m_modelData.end(); ++iter)
		g_renderHandler->RemoveRenderable(iter->second);
	m_modelData.clear();
}

bool IsSwitchStationary(PDOOR door)
{
	DWORD type = door->Type;

	return (type != 53 && type >= 50 && type < 59)
		|| (type >= 153 && type <= 155);
}

#pragma region ImGui Debug UI

void DoorsDebugUI::Render()
{
if (!m_showDoorsUI)
		return;
	if (!ImGui::Begin("EQ Doors", &m_showDoorsUI))
		return;

	PDOORTABLE pDoorTable = (PDOORTABLE)pSwitchMgr;

	ImGui::Text("%d door objects", pDoorTable->NumEntries);

	ImGui::Columns(6, "doorcolumns");
	ImGui::Separator();

	auto header = [this](const char* name, int id) {
		if (ImGui::Selectable(name, m_doorsSortColumn == id)) {
			m_sortReverse = (m_doorsSortColumn == id) ? !m_sortReverse : false;
			m_doorsSortColumn = id;
		}
		ImGui::NextColumn();
	};

	header("ID", Sort_ID);
	header("Name", Sort_Name);
	header("Type", Sort_Type);
	header("State", Sort_State);
	header("Distance", Sort_Distance);
	/* buttons */ ImGui::NextColumn();

	ImGui::Separator();

	std::vector<PDOOR> doors(pDoorTable->NumEntries);

	for (DWORD count = 0; count < pDoorTable->NumEntries; count++)
	{
		doors[count] = pDoorTable->pDoor[count];
	}

	std::sort(doors.begin(), doors.end(),
		[this](PDOOR a, PDOOR b) -> bool
	{
		bool less = false;
		if (m_doorsSortColumn == Sort_ID)
			less = a->ID < b->ID;
		else if (m_doorsSortColumn == Sort_Name)
			less = strcmp(a->Name, b->Name) < 0;
		else if (m_doorsSortColumn == Sort_Type)
			less = a->Type < b->Type;
		else if (m_doorsSortColumn == Sort_State)
			less = a->State < b->State;
		else if (m_doorsSortColumn == Sort_Distance)
			less = GetDistance(a) < GetDistance(b);
		
		return less;
	});

	if (m_sortReverse)
		std::reverse(doors.begin(), doors.end());

	for (auto iter = doors.begin(); iter != doors.end(); ++iter)
	{
		PDOOR door = *iter;

		ImGui::PushID(door->ID);

		bool targetted = (m_lastDoorTargetId == door->ID);
		if (targetted)
			ImGui::PushStyleColor(ImGuiCol_Text, ImColor(0, 255, 0));

		ImGui::Text("%d", door->ID); ImGui::NextColumn();

		if (ImGui::Selectable(door->Name, targetted))
		{
			CHAR temp[256];
			sprintf_s(temp, "/doortarget id %d", door->ID);

			// would be nice if there was an easier way to do this.
			EzCommand(temp);
		}
		ImGui::NextColumn();
		ImGui::Text("%d", door->Type); ImGui::NextColumn();
		ImGui::Text("%d", door->State); ImGui::NextColumn();
		ImGui::Text("%.2f", GetDistance(door)); ImGui::NextColumn();

		if (targetted)
			ImGui::PopStyleColor();

		if (ImGui::Button("Click"))
			ClickDoor(door);

		if (targetted)
		{
			ImGui::SameLine();
			ImGui::Text("<Target>");
		}

		if (IsSwitchStationary(door))
		{
			ImGui::SameLine();
			ImGui::Text("*stationary*");
		}

		ImGui::NextColumn();

		ImGui::PopID();
	}

	ImGui::Separator();
	ImGui::Columns(1);
	ImGui::Separator();

	ImGui::End();
}

float DoorsDebugUI::GetDistance(PDOOR door)
{
	PCHARINFO charInfo = GetCharInfo();
	if (!charInfo)
		return 0;

	glm::vec3 myPos(charInfo->pSpawn->X, charInfo->pSpawn->Y, charInfo->pSpawn->Z);
	if (myPos != m_lastPos)
		m_distanceCache.clear();
	m_lastPos = myPos;

	auto it = m_distanceCache.find(door->ID);
	if (it != m_distanceCache.end())
		return it->second;

	glm::vec3 doorPos(door->DefaultX, door->DefaultY, door->DefaultZ);
	float distance = glm::distance(myPos, doorPos);

	m_distanceCache[door->ID] = distance;
	return distance;
}


void ModelLoader::OnUpdateUI()
{
	m_doorsUI->Render();

	ImGui::Begin("Debug");

	if (ImGui::CollapsingHeader("Objects"))
	{
		//ImGui::LabelText("ZoneId", "%d", m_zoneId);
		//ImGui::LabelText("Models Loaded", "%d", m_models.size());
		//ImGui::LabelText("Filename", "%s", m_zoneFile.c_str());

		ImGui::Checkbox("Draw Bounding Boxes", &s_drawBoundingBoxes);
		ImGui::Checkbox("All doors visible", &s_visibleOverride);

		if (ImGui::Button("View Doors"))
		{
			m_doorsUI->Show();
		}

		if (ImGui::TreeNode("##SwitchTable", "Objects"))
		{
			PDOORTABLE pDoorTable = (PDOORTABLE)pSwitchMgr;
			
			if (m_lastDoorTargetId >= 0)
			{
				PDOOR door = nullptr;
				for (DWORD count = 0; count < pDoorTable->NumEntries; count++)
				{
					if (pDoorTable->pDoor[count]->ID == m_lastDoorTargetId)
						door = pDoorTable->pDoor[count];
				}

				if (door)
				{
					ImGui::PushStyleColor(ImGuiCol_Text, ImColor(0, 255, 0));

					if (ImGui::TreeNode("Target Object", "Door Target: %s (%d)", door->Name, door->ID))
					{
						ImGui::PopStyleColor();

						RenderDoorObjectUI(door, true);

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

void ModelLoader::RenderDoorObjectUI(PDOOR door, bool target)
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

	if (target)
	{
		if (ImGui::TreeNode("Door Data"))
		{
			DumpDataUI(door, sizeof(_DOOR));

			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Switch Data"))
		{
			DumpDataUI(door->pSwitch, sizeof(EQSWITCH));

			ImGui::TreePop();
		}
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
	ImGui::LabelText("Scale", "%.2f", GetDoorScale(door));
	
	ImGui::DragFloat2("Top Speed 1", &door->TopSpeed1);

	ImGui::DragFloat3("Default Position", &door->DefaultY);
	ImGui::DragFloat("Default Heading", &door->DefaultHeading);
	ImGui::DragFloat("Default Angle", &door->DefaultDoorAngle);


	if (ImGui::Button("Target"))
	{
		CHAR temp[256];
		sprintf_s(temp, "/doortarget id %d", door->ID);

		// would be nice if there was an easier way to do this.
		EzCommand(temp);
	}

	ImGui::SameLine();

	if (ImGui::Button("Reset Position"))
	{
		door->X = door->DefaultX;
		door->Y = door->DefaultY;
		door->Z = door->DefaultZ;
		door->Heading = door->DefaultHeading;
	}

	ImGui::SameLine();

	if (ImGui::Button("Click"))
	{
		ClickDoor(door);
	}
}

void DumpDataUI(void* ptr, DWORD length)
{
	ImGui::Columns(4);

	for (int i = 0; i < length / 4; ++i)
	{
		float* p = (float*)ptr;
		p += i;

		ImGui::Text("0x%08x (+%04x)", p, (i * 4)); ImGui::NextColumn();

		BYTE* b = (BYTE*)p;
		ImGui::Text("%02x %02x %02x %02x", b[0], b[1], b[2], b[3]); ImGui::NextColumn();
		ImGui::Text("0x%08x", *(DWORD*)p); ImGui::NextColumn();

		ImGui::PushID(i);
		ImGui::DragFloat("", p);
		ImGui::PopID();
		ImGui::NextColumn();
	}

	ImGui::Columns(1);
}

#pragma endregion
