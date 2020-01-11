//
// ModelLoader.cpp
//

#include "pch.h"
#include "ModelLoader.h"

#include "plugin/ImGuiRenderer.h"
#include "plugin/DebugDrawDX.h"
#include "plugin/Renderable.h"
#include "plugin/RenderHandler.h"
#include "plugin/Utilities.h"

#include <boost/algorithm/string.hpp>
#include <imgui.h>
#include <imgui/custom/imgui_column_headers.h>
#include <rapidjson/prettywriter.h>

#define GLM_FORCE_RADIANS
#include <glm.hpp>

#include <DebugDraw.h>
#include <d3d9types.h>

#include <filesystem>
#include <fstream>

float GetDoorScale(PDOOR door)
{
	return (float)door->ScaleFactor / 100.0f;
}

#if defined(Teleport_Table_x) && defined(Teleport_Table_Size_x)
#define USE_TP_COORDS
#endif


#pragma region Object Model Debug Rendering

bool s_visibleOverride = false;
bool s_drawBoundingBoxes = false;

class ModelData : public Renderable
{
public:
	ModelData(int doorId, const std::shared_ptr<ModelInfo>& modelInfo, IDirect3DDevice9* device)
		: m_doorId(doorId)
		, m_modelInfo(modelInfo)
	{
		m_grpBB = std::make_unique<RenderGroup>(device);
#if RENDER_MODELS
		m_grpModel = std::make_unique<RenderGroup>(device);
#endif
		RegenerateMesh();

		g_renderHandler->AddRenderable(this);
	}

	~ModelData()
	{
		g_renderHandler->RemoveRenderable(this);

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
		{
			DebugDrawDX dd(m_grpBB.get());

			duDebugDrawBoxWire(&dd,
				m_modelInfo->min.x, m_modelInfo->min.z, m_modelInfo->min.y,
				m_modelInfo->max.x, m_modelInfo->max.z, m_modelInfo->max.y,
				m_color, 1.0);
		}

#if RENDER_MODELS
		if (!m_targetted)
			return;

		DebugDrawDX dd(m_grpModel.get());

		// iterate over all the things!
		if (m_modelInfo->oldModel)
		{
			auto verts = m_modelInfo->oldModel->GetVertices();
			auto polys = m_modelInfo->oldModel->GetPolygons();

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

		if (m_modelInfo->newModel)
		{
			auto verts = m_modelInfo->newModel->GetVertices();
			auto polys = m_modelInfo->newModel->GetPolygons();

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

	virtual void Render() override
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
					bool zDisabled = m_targetted || m_highlight;

					// create a matrix to scale the object by the scale amount
					D3DXMATRIX scale;
					float scaleFactor = GetDoorScale(door);
					D3DXMatrixScaling(&scale, scaleFactor, scaleFactor, scaleFactor);

					// multiply the transform matrix by the scale matrix to produce new matrix
					D3DXMATRIX* mtx = (D3DXMATRIX*)(&door->pSwitch->transformMatrix); 
					scale = scale * *mtx;

#if 0
					// if attachment matrix exists, apply it next
					if (door->pSwitch->bbHasAttachSRT)
					{
						D3DXMATRIX* attach = (D3DXMATRIX*)(&door->pSwitch->attachSRT);

						scale = scale * *attach;
					}
#endif

#if RENDER_MODELS
					if (m_targetted)
					{
						m_grpModel->SetTransform(&scale);
						m_grpModel->Render(phase);
					}
#endif

					if (m_targetted || m_highlight || s_drawBoundingBoxes)
					{
						gpD3D9Device->SetRenderState(D3DRS_ZENABLE, !zDisabled && !s_visibleOverride);
						m_grpBB->SetTransform(&scale);
						m_grpBB->Render();
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

	const std::shared_ptr<ModelInfo>& GetModelInfo() { return m_modelInfo; }

private:
	void Rebuild()
	{
		m_grpBB->Reset();
#if RENDER_MODELS
		m_grpModel->Reset();
#endif
		RegenerateMesh();
	}

	std::shared_ptr<ModelInfo> m_modelInfo;
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

#define RENDER_MODELS  0

const char* GetTeleportName(DWORD id)
{
#if defined(USE_TP_COORDS)
	struct tp_coords
	{
		DWORD    Index;
		FLOAT    Y;
		FLOAT    X;
		FLOAT    Z;
		FLOAT    Heading;
		DWORD    ZoneId;
		DWORD    FilterId;
		DWORD    VehicalId;
	};
	DWORD TableSize = *(DWORD*)Teleport_Table_Size;
	tp_coords *tp = (tp_coords*)Teleport_Table;

	if (id < TableSize)
	{
		DWORD zoneId = tp[id].ZoneId & 0x7fff;

		return GetShortZone(zoneId);
	}
#endif

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
}

void ModelLoader::Shutdown()
{
}

void ModelLoader::OnPulse()
{
	if (m_zoneId == 0)
		return;
	if (gGameState != GAMESTATE_INGAME)
		return;

	PDOORTABLE pDoorTable = (PDOORTABLE)pSwitchMgr;
	if (m_loadedDoorCount != pDoorTable->NumEntries
		&& pDoorTable->NumEntries > 0)
	{
		UpdateModels();
	}

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

	if (zoneId != -1)
	{
		SPDLOG_DEBUG("Model Loader, zone set to: {}", zoneId);
		m_zoneId = zoneId;
	}
}

void ModelLoader::UpdateModels()
{
	m_modelData.clear();

	const char* zoneName = GetShortZone(m_zoneId);
	const std::string pathEQ = std::filesystem::absolute(".").string();

	// this uses a lot of cpu, spin it off into its own thread so it
	// doesn't block themain thread.
	auto zoneData = std::make_unique<ZoneData>(pathEQ, zoneName);

	if (!zoneData->IsLoaded())
	{
		return;
	}

	PDOORTABLE pDoorTable = (PDOORTABLE)pSwitchMgr;
	for (DWORD count = 0; count < pDoorTable->NumEntries; count++)
	{
		PDOOR door = pDoorTable->pDoor[count];
		std::shared_ptr<ModelInfo> modelInfo;

		// might end up moving the renderable generating into m_zoneData
		if (modelInfo = zoneData->GetModelInfo(door->Name))
		{
			// Create new model object
			std::shared_ptr<ModelData> md = std::make_shared<ModelData>(door->ID, modelInfo, gpD3D9Device);
			m_modelData[door->ID] = md;
		}
	}

	m_loadedDoorCount = pDoorTable->NumEntries;

	// dump the doors out to a config file
	DumpDoors();
}

void ModelLoader::DumpDoors()
{
	std::string filename = std::string(gPathResources) + "\\MQ2Nav";

	// make sure directory exists so we can write to it!
	std::error_code ec;
	std::filesystem::create_directory(filename, ec);

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
	m_models.clear();
	m_lastDoorTargetId = -1;
	m_loadedDoorCount = 0;
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

	ImGui::SetNextWindowSize(ImVec2(500, 120), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("EQ Doors", &m_showDoorsUI))
	{
		ImGui::End();
		return;
	}

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
			ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(0, 255, 0));

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

		ImGui::SameLine();
		if (ImGui::Button("Nav"))
		{
			std::string command = "door id " + std::to_string(door->ID) + " click";

			auto info = g_mq2Nav->ParseDestination(command.c_str(), spdlog::level::info);
			g_mq2Nav->BeginNavigation(info);
		}

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


void ModelLoader::OnUpdateUI(bool visible)
{
	m_doorsUI->Render();

	if (!visible)
		return;

	if (ImGui::CollapsingHeader("Objects"))
	{
		//ImGui::LabelText("ZoneId", "%d", m_zoneId);
		//ImGui::LabelText("Models Loaded", "%d", m_models.size());
		//ImGui::LabelText("Filename", "%s", m_zoneFile.c_str());

		ImGui::Checkbox("Draw Bounding Boxes", &s_drawBoundingBoxes);
		if (s_drawBoundingBoxes)
			ImGui::Checkbox("All doors visible", &s_visibleOverride);

		if (ImGui::Button("View Doors"))
		{
			m_doorsUI->Show();
		}

		if (ImGui::TreeNode("##SwitchTable", "Door Objects"))
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
					ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(0, 255, 0));

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
			EQGroundItem* pItem = pItemList->Top;
			while (pItem)
			{
				if (ImGui::TreeNode(pItem->Name, "%s (%d)", pItem->Name, pItem->DropID))
				{
					ImGui::LabelText("Id", "%d", pItem->pContents);
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
			DumpDataUI(door, sizeof(DOOR));

			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Switch Data"))
		{
			DumpDataUI(door->pSwitch, sizeof(EQSWITCH));

			ImGui::TreePop();
		}
	}

	ImGui::Separator();

	ImGui::Text("ID: %d Type: %d", door->ID, door->Type);
	ImGui::LabelText("Name", "%s", door->Name);
	if (door->SpellID != -1)
	{
		const char* zone = GetTeleportName(door->SpellID);
		ImGui::TextColored(ImColor(255, 255, 0), "Zone Point: %s (%d)", zone, door->SpellID);
	}

	ImGui::LabelText("State", "%d", door->State);
	ImGui::LabelText("Default State", "%d", door->DefaultState);

	ImGui::DragFloat3("Default Position", &door->DefaultY);
	ImGui::DragFloat3("Position", &door->Y);
	ImGui::DragFloat("Default Heading", &door->DefaultHeading);
	ImGui::DragFloat("Heading", &door->Heading);
	ImGui::DragFloat("Default Angle", &door->DefaultDoorAngle);
	ImGui::DragFloat("Angle", &door->DoorAngle);

	ImGui::LabelText("Scale", "%.2f", GetDoorScale(door));
	ImGui::DragFloat2("Top Speed", &door->TopSpeed1);

	ImGui::LabelText("Self Activated", "%d", door->SelfActivated);
	ImGui::LabelText("Dependent", "%d", door->Dependent);
	ImGui::LabelText("bTemplate", "%d", door->bTemplate);
	ImGui::LabelText("Difficulty", "%d", door->Difficulty);
	ImGui::LabelText("Key", "%d", door->Key);
	ImGui::LabelText("SpellID", "%d", door->SpellID);
	ImGui::LabelText("TargetID", "%hhX%hhX%hhX%hhX%hhX", door->TargetID[0], door->TargetID[1], door->TargetID[2], door->TargetID[3], door->TargetID[4]);
	ImGui::LabelText("Script", "%s", door->Script);
	ImGui::LabelText("TimeStamp", "%d", door->TimeStamp);
	ImGui::LabelText("AlwaysActive", "%d", door->AlwaysActive);
	ImGui::DragFloat("Return Pos", &door->ReturnY);
	ImGui::LabelText("DynDoorID", "%d", door->DynDoorID);
	ImGui::LabelText("bHasScript", "%d", door->bHasScript);
	ImGui::LabelText("SomeID", "%d", door->SomeID);
	ImGui::LabelText("bUsable", "%d", door->bUsable);
	ImGui::LabelText("bRemainOpen", "%d", door->bRemainOpen);
	ImGui::LabelText("bVisible", "%d", door->bVisible);
	ImGui::LabelText("bHeadingChanged", "%d", door->bHeadingChanged);
	ImGui::LabelText("bALlowCorpseDrag", "%d", door->bAllowCorpseDrag);
	ImGui::LabelText("RealEstateDoorID", "%d", door->RealEstateDoorID);

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

	for (int i = 0; i < static_cast<int>(length / 4); ++i)
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
