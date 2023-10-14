//
// ModelLoader.cpp
//

#include "pch.h"
#include "ModelLoader.h"

#include "plugin/DebugDrawDX.h"
#include "plugin/PluginSettings.h"
#include "plugin/Renderable.h"
#include "plugin/RenderHandler.h"
#include "plugin/Utilities.h"
#include "imgui/ImGuiUtils.h"

#include <imgui.h>
#include <rapidjson/prettywriter.h>

#include <glm/glm.hpp>

#include <DebugDraw.h>
#include <d3d9types.h>

#include <filesystem>
#include <fstream>

float GetDoorScale(EQSwitch* theSwitch)
{
	return (float)theSwitch->ScaleFactor / 100.0f;
}

#define RENDER_MODELS  0

#pragma region Object Model Debug Rendering

bool s_visibleOverride = false;
bool s_drawBoundingBoxes = false;

class ModelData : public Renderable
{
public:
	ModelData(int doorId, const std::shared_ptr<ModelInfo>& modelInfo, eqlib::Direct3DDevice9* device)
		: m_switchID(doorId)
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
		// find the switch
		if (!pSwitchMgr) return;

		if (EQSwitch* pSwitch = pSwitchMgr->GetSwitchById(m_switchID))
		{
			if (m_visible && pSwitch->pActor)
			{
				bool zDisabled = m_targetted || m_highlight;

				// create a matrix to scale the object by the scale amount
				float scaleFactor = GetDoorScale(pSwitch);

				//D3DXMatrixScaling(&scale, scaleFactor, scaleFactor, scaleFactor);

				// multiply the transform matrix by the scale matrix to produce new matrix
				glm::mat4* mtx = pSwitch->pActor->GetObjectToWorldMatrix();
				glm::mat4 scale = glm::scale(*mtx, glm::vec3(scaleFactor));

#if RENDER_MODELS
				if (m_targetted)
				{
					m_grpModel->SetTransform(&scale);
					m_grpModel->Render();
				}
#endif

#if HAS_DIRECTX_9
				if (m_targetted || m_highlight || s_drawBoundingBoxes)
				{
					gpD3D9Device->SetRenderState(D3DRS_ZENABLE, !zDisabled && !s_visibleOverride);
					m_grpBB->SetTransform(&scale);
					m_grpBB->Render();
				}
#endif
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
	int m_switchID = 0;
	bool m_highlight = false;
	bool m_targetted = false;
};

#pragma endregion

//----------------------------------------------------------------------------

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

	float GetDistance(EQSwitch* door);

	int m_doorsSortColumn = 0;
	bool m_sortReverse = false;
	bool m_showDoorsUI = false;
	int m_lastDoorTargetId = -1;

	std::map<int, float> m_distanceCache;
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
	if (pSwitchTarget)
		doorTargetId = pSwitchTarget->ID;
	m_lastDoorTargetId = doorTargetId;

	// Don't render doortarget if setting is disabled.
	bool showDoorTarget = nav::GetSettings().render_doortarget;
	if (!showDoorTarget)
		doorTargetId = -1;

	if (doorTargetId != m_lastDoorTargetRenderId)
	{
		if (m_lastDoorTargetRenderId >= 0)
		{
			if (auto model = m_modelData[m_lastDoorTargetRenderId])
			{
				model->SetTargetted(false);
			}
		}

		m_lastDoorTargetRenderId = doorTargetId;
		m_doorsUI->m_lastDoorTargetId = doorTargetId;

		if (m_lastDoorTargetRenderId >= 0)
		{
			if (auto model = m_modelData[m_lastDoorTargetRenderId])
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

	if (gbDeviceAcquired)
	{
		const char* zoneName = GetShortZone(m_zoneId);
		const std::string pathEQ = std::filesystem::absolute(".").string();

		// this uses a lot of cpu, spin it off into its own thread so it
		// doesn't block themain thread.
		auto zoneData = std::make_unique<ZoneData>(pathEQ, zoneName);

		if (!zoneData->IsLoaded())
		{
			return;
		}

		for (int index = 0; index < pSwitchMgr->GetCount(); index++)
		{
			EQSwitch* pSwitch = pSwitchMgr->GetSwitch(index);
			std::shared_ptr<ModelInfo> modelInfo;

			// might end up moving the renderable generating into m_zoneData
			if (modelInfo = zoneData->GetModelInfo(pSwitch->Name))
			{
				// Create new model object
				std::shared_ptr<ModelData> md = std::make_shared<ModelData>(pSwitch->ID, modelInfo, gpD3D9Device);
				m_modelData[pSwitch->ID] = md;
			}
		}
	}

	m_loadedDoorCount = pSwitchMgr->GetCount();

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

	for (int count = 0; count < pSwitchMgr->NumEntries; count++)
	{
		if (EQSwitch* pSwitch = pSwitchMgr->GetSwitch(count))
		{
			writer.StartObject();

			writer.String("ID"); writer.Int(pSwitch->ID);
			writer.String("Name"); writer.String(pSwitch->Name);
			writer.String("Type"); writer.Int(pSwitch->Type);
			writer.String("Scale"); writer.Double((float)pSwitch->ScaleFactor / 100.0f);

			if (pSwitch->pActor)
			{
				writer.String("Transform");
				writer.StartArray();
				for (int i = 0; i < 4; i++)
				{
					writer.StartArray();
					for (int j = 0; j < 4; j++)
					{
						writer.Double((*pSwitch->pActor->GetObjectToWorldMatrix())[i][j]);
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
	m_lastDoorTargetRenderId = -1;
	m_loadedDoorCount = 0;
	m_modelData.clear();
}

bool IsSwitchStationary(EQSwitch* pSwitch)
{
	DWORD type = pSwitch->Type;

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

	ImGui::Text("%d door objects", pSwitchMgr->NumEntries);

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

	std::vector<EQSwitch*> switches(pSwitchMgr->NumEntries);
	for (int count = 0; count < pSwitchMgr->NumEntries; count++)
	{
		switches[count] = pSwitchMgr->GetSwitch(count);
	}

	std::sort(switches.begin(), switches.end(),
		[this](EQSwitch* a, EQSwitch* b) -> bool
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
		std::reverse(switches.begin(), switches.end());

	for (auto iter = switches.begin(); iter != switches.end(); ++iter)
	{
		EQSwitch* door = *iter;

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

float DoorsDebugUI::GetDistance(EQSwitch* theSwitch)
{
	if (!pLocalPlayer)
		return 0;

	glm::vec3 myPos(pLocalPlayer->X, pLocalPlayer->Y, pLocalPlayer->Z);
	if (myPos != m_lastPos)
		m_distanceCache.clear();
	m_lastPos = myPos;

	auto it = m_distanceCache.find(theSwitch->ID);
	if (it != m_distanceCache.end())
		return it->second;

	glm::vec3 doorPos(theSwitch->DefaultX, theSwitch->DefaultY, theSwitch->DefaultZ);
	float distance = glm::distance(myPos, doorPos);

	m_distanceCache[theSwitch->ID] = distance;
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
			if (m_lastDoorTargetId >= 0)
			{
				if (EQSwitch* pSwitch = pSwitchMgr->GetSwitchById(m_lastDoorTargetId))
				{
					ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(0, 255, 0));

					if (ImGui::TreeNode("Target Object", "Door Target: %s (%d)", pSwitch->Name, pSwitch->ID))
					{
						ImGui::PopStyleColor();

						RenderDoorObjectUI(pSwitch, true);

						ImGui::TreePop();
					}
					else
					{
						ImGui::PopStyleColor();
					}
				}
			}

			// temp for now, until we figure out where to put this...
			for (int count = 0; count < pSwitchMgr->NumEntries; count++)
			{
				EQSwitch* pSwitch = pSwitchMgr->GetSwitch(count);
				ImGui::PushID(count);

				if (ImGui::TreeNode(pSwitch->Name, "%s (%d)", pSwitch->Name, pSwitch->ID))
				{
					RenderDoorObjectUI(pSwitch);

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
					ImGui::LabelText("DropId", "%d", pItem->DropID);
					ImGui::LabelText("SubId", "%d", pItem->DropSubID);
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

void ModelLoader::RenderDoorObjectUI(EQSwitch* door, bool target)
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
