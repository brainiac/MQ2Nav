//
// NavMeshRenderer.cpp
//

#include "NavMeshRenderer.h"
#include "ImGuiRenderer.h"
#include "MQ2Navigation.h"
#include "MeshLoader.h"

#include <cassert>
#include <DetourDebugDraw.h>
#include <RecastDebugDraw.h>
#include <DetourNavMesh.h>
#include <DebugDraw.h>

#include <imgui.h>
#include "imgui_memory_editor.h"

#define GLM_FORCE_RADIANS
#include <glm.hpp>

//MemoryEditor memory_editor;

std::shared_ptr<NavMeshRenderer> g_navMeshRenderer;

class DebugDrawDX : public duDebugDraw
{
	NavMeshRenderer& m_renderer;
	RenderList* m_rl = nullptr;

public:
	DebugDrawDX(NavMeshRenderer& rl_)
		: m_renderer(rl_)
	{
	}

	virtual void end() override
	{
		m_rl->End();
	}

	virtual void depthMask(bool state) override
	{
		//m_pDevice->SetRenderState(D3DRS_ZENABLE, state ? D3DZB_TRUE : D3DZB_FALSE);
	}

	virtual void texture(bool state) override
	{
	}

	virtual void begin(duDebugDrawPrimitives prim, float size = 1.0f) override
	{
		RenderList::PrimitiveType type = RenderList::Prim_Points;

		switch (prim)
		{
		case DU_DRAW_POINTS: type = RenderList::Prim_Points; break;
		case DU_DRAW_LINES: type = RenderList::Prim_Lines; break;
		case DU_DRAW_TRIS: type = RenderList::Prim_Triangles; break;
		case DU_DRAW_QUADS: type = RenderList::Prim_Quads; break;
		}

		m_rl = m_renderer.GetRenderList(type);
		m_rl->Begin(size);
	}

	virtual void vertex(const float* pos, unsigned int color) override
	{
		vertex(pos[0], pos[1], pos[2], color, 0.0f, 0.0f);
	}

	virtual void vertex(const float x, const float y, const float z, unsigned int color) override
	{
		vertex(x, y, z, color, 0.0f, 0.0f);
	}

	virtual void vertex(const float* pos, unsigned int color, const float* uv) override
	{
		vertex(pos[0], pos[1], pos[2], color, uv[0], uv[1]);
	}

	virtual void vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v) override
	{
		m_rl->AddVertex(x, y, z, color, u, v);
	}
};

//----------------------------------------------------------------------------

NavMeshRenderer::NavMeshRenderer(MeshLoader* loader, IDirect3DDevice9* device)
	: m_pDevice(device)
	, m_meshLoader(loader)
	, m_state(new ConfigurableRenderState)
{
	m_uiConn = g_imguiRenderer->OnUpdateUI.Connect([this]() { OnUpdateUI(); });

	auto conn = [this](dtNavMesh* m) { m_navMesh = m; UpdateNavMesh(); };
	m_meshConn = loader->OnNavMeshChanged.Connect(conn);

	for (int i = 0; i < RenderList::Prim_Count; ++i)
	{
		m_primLists[i].reset(new RenderList(device, static_cast<RenderList::PrimitiveType>(i)));
		m_primsEnabled[i] = true;
	}
}

NavMeshRenderer::~NavMeshRenderer()
{
	CleanupObjects();
}

void NavMeshRenderer::InvalidateDeviceObjects()
{
	CleanupObjects();
}

void NavMeshRenderer::CleanupObjects()
{
	StopLoad();

	for (int i = 0; i < RenderList::Prim_Count; ++i)
		m_primLists[i]->InvalidateDeviceObjects();
}

bool NavMeshRenderer::CreateDeviceObjects()
{
	for (int i = 0; i < RenderList::Prim_Count; ++i)
		m_primLists[i]->CreateDeviceObjects();

	return true;
}

void NavMeshRenderer::Render(Renderable::RenderPhase phase)
{
	if (phase == Renderable::Render_Geometry)
	{
		if (m_enabled != m_loaded)
		{
			if (m_enabled)
			{
				if (m_navMesh)
				{
					UpdateNavMesh();
				}
			}
			else
			{
				StopLoad();
				InvalidateDeviceObjects();
			}

			m_loaded = m_enabled;
		}

		if (!m_loaded || m_loading)
			return;

		m_pDevice->SetPixelShader(nullptr);
		m_pDevice->SetVertexShader(nullptr);

		m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
		m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);

		if (m_useStateEditor)
		{
			m_state->ApplyState(m_pDevice);
		}
		else
		{
			m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
			m_pDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
			m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

			m_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, true);
			m_pDevice->SetRenderState(D3DRS_ALPHAREF, 0);
			m_pDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_ALWAYS);

			m_pDevice->SetRenderState(D3DRS_ZENABLE, true);
			m_pDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
			m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, true);

			m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
			m_pDevice->SetRenderState(D3DRS_LIGHTING, false);
			m_pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, true);

			m_pDevice->SetRenderState(D3DRS_STENCILENABLE, false);

			// disable the rest of the texture stages
			for (int i = 0; i < 8; i ++)
			{
				m_pDevice->SetTextureStageState(i, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
				m_pDevice->SetTextureStageState(i, D3DTSS_COLOROP, D3DTOP_DISABLE);
			}
		}

		for (int i = 0; i < RenderList::Prim_Count; ++i)
		{
			if (m_primsEnabled[i])
				m_primLists[i]->Render(phase);
		}
	}
}

//----------------------------------------------------------------------------

void NavMeshRenderer::StopLoad()
{
	if (m_loadThread.joinable())
	{
		m_stopLoading = true;
		m_loadThread.join();
	}
}

void NavMeshRenderer::StartLoad()
{
	m_stopLoading = false;
	m_loading = true;
	int tilesToLoad = 0;

	std::shared_ptr<DebugDrawDX> dd = std::make_shared<DebugDrawDX>(*this);

	for (int i = 0; i < m_navMesh->getMaxTiles(); ++i)
	{
		const dtMeshTile* tile = m_navMesh->getTile(i);
		if (!tile->header) continue;

		tilesToLoad++;
	}

	auto loadingThread = [this, dd, tilesToLoad]() mutable
	{
		int currentTile = 0;

		for (int i = 0; i < m_navMesh->getMaxTiles() && !m_stopLoading; ++i)
		{
			const dtMeshTile* tile = m_navMesh->getTile(i);
			if (!tile->header) continue;

			drawMeshTile(dd.get(), *m_navMesh, 0, tile, DU_DRAWNAVMESH_OFFMESHCONS | DU_DRAWNAVMESH_CLOSEDLIST
				/* | DU_DRAWNAVMESH_COLOR_TILES*/);

			++currentTile;
			m_progress = static_cast<float>(currentTile) / static_cast<float>(tilesToLoad) * 100;
		}

		m_loading = false;
	};

	m_loadThread = std::thread(loadingThread);
}

void NavMeshRenderer::UpdateNavMesh()
{
	// Tradeskill_objects.eqg
	// caveswitches.eqg
	EQEmu::EQGLoader loader;
	if (loader.Load("Z:\\EverQuest\\tutorialb", eqg_models, eqg_placeables, eqg_regions, eqg_lights))
	{
		std::sort(eqg_models.begin(), eqg_models.end(),
			[](const std::shared_ptr<EQEmu::EQG::Geometry>& geo1, const std::shared_ptr<EQEmu::EQG::Geometry>& geo2)
		{
			return geo1->GetName() < geo2->GetName();
		});

		std::sort(eqg_placeables.begin(), eqg_placeables.end(),
			[](const std::shared_ptr<EQEmu::Placeable>& geo1, const std::shared_ptr<EQEmu::Placeable>& geo2)
		{
			return geo1->GetName() < geo2->GetName();
		});
	}

	EQEmu::PFS::Archive archive;
	if (archive.Open("Z:\\EverQuest\\tutorialb_obj.s3d"))
	{
		archive.GetFilenames("*", m_files);
	}

	// if not enabled, don't do any work
	if (!m_enabled)
		return;

	StopLoad();

	for (int i = 0; i < RenderList::Prim_Count; ++i)
		m_primLists[i]->Reset();

	// if we don't have a navmesh, don't build the geometry
	if (!m_navMesh)
	{
		InvalidateDeviceObjects();
		return;
	}

	StartLoad();
}

void NavMeshRenderer::OnUpdateUI()
{
	if (!GetCharInfo())
		return;
	PCHARINFO pCharInfo = GetCharInfo();

	//bool valid_address = false;
	//static DWORD address = 0;
	//ImGui::Begin("Memory Editor");
	//{
	//	ImGui::InputInt("Address", (int*)&address, 1, 100, ImGuiInputTextFlags_CharsHexadecimal);

	//	if (address != 0)
	//	{
	//		__try {
	//			byte* value = (byte*)address;
	//			byte forceRead = *value;
	//			byte forceRead2 = *(value + 1024);

	//			valid_address = true;
	//		}
	//		__except (EXCEPTION_EXECUTE_HANDLER) {
	//			ImGui::TextColored(ImColor(255, 0, 0), "Invalid Address");
	//		}
	//	}
	//}
	//ImGui::End();

	//if (valid_address)
	//{
	//	memory_editor.Draw("Memory Editor", (unsigned char*)address, 1024);
	//}

	ImGui::Begin("NavMesh");

	DWORD zoneId = pCharInfo->zoneId & 0x7fff;
	if (zoneId < MAX_ZONES)
	{
		PZONELIST pZone = ((PWORLDDATA)pWorldData)->ZoneArray[zoneId];
		ImGui::TextColored(ImColor(255, 255, 0), "%s (%s)", pZone->LongName, pZone->ShortName);

		if (m_files.size() > 0)
		{
			if (ImGui::TreeNode("Files"))
			{
				for (auto iter = m_files.begin(); iter != m_files.end(); ++iter)
				{
					//const auto& model = *iter;

					ImGui::Text("%s", iter->c_str());
				}

				ImGui::TreePop();
			}
		}

		if (eqg_models.size() > 0)
		{
			if (ImGui::TreeNode("Models"))
			{
				for (auto iter = eqg_models.begin(); iter != eqg_models.end(); ++iter)
				{
					const auto& model = *iter;

					ImGui::Text("%s", model->GetName().c_str());
				}

				ImGui::TreePop();
			}
		}

		if (eqg_placeables.size() > 0)
		{
			if (ImGui::TreeNode("Placeables"))
			{
				for (auto iter = eqg_placeables.begin(); iter != eqg_placeables.end(); ++iter)
				{
					const auto& placeable = *iter;

					ImGui::Text("%s (%s)", placeable->GetName().c_str(),
						placeable->GetFileName().c_str());
				}

				ImGui::TreePop();
			}
		}

	}

	ImGui::Checkbox("Render Navmesh", &m_enabled);

	if (m_enabled)
	{
		ImGui::Separator();
		if (m_loading)
		{
			ImGui::LabelText("Progress", "%.2f", m_progress);
		}

#if 0
		ImGui::Columns(2);
		ImGui::Checkbox("Points", &m_primsEnabled[RenderList::Prim_Points]); ImGui::NextColumn();
		ImGui::Checkbox("Lines", &m_primsEnabled[RenderList::Prim_Lines]); ImGui::NextColumn();
		ImGui::Checkbox("Triangles", &m_primsEnabled[RenderList::Prim_Triangles]); ImGui::NextColumn();
		ImGui::Checkbox("Quads", &m_primsEnabled[RenderList::Prim_Quads]); ImGui::NextColumn();
		ImGui::Columns(1);
#endif
		ImGui::Checkbox("Modify State", &m_useStateEditor);

		if (m_useStateEditor)
		{
			m_state->RenderDebugUI();
		}
	}

	if (ImGui::CollapsingHeader("Doors"))
	{
		// temp for now, until we figure out where to put this...
		PDOORTABLE pDoorTable = (PDOORTABLE)pSwitchMgr;
		for (DWORD count = 0; count < pDoorTable->NumEntries; count++)
		{
			PDOOR door = pDoorTable->pDoor[count];
			ImGui::PushID(count);

			if (ImGui::TreeNode(door->Name, "%s (%d)", door->Name, door->ID))
			{
				ImGui::LabelText("ID", "%d", door->ID);
				ImGui::LabelText("Type", "%d", door->Type);
				ImGui::LabelText("State", "%d", door->State);

				ImGui::DragFloat3("Position", &door->Y);
				ImGui::DragFloat("Heading", &door->Heading);
				ImGui::DragFloat("Angle", &door->DoorAngle);
				ImGui::DragFloat2("Top Speed", &door->TopSpeed1);

				ImGui::DragFloat3("Default Position", &door->DefaultY);
				ImGui::DragFloat("Default Heading", &door->DefaultHeading);
				ImGui::DragFloat("Default Angle", &door->DefaultDoorAngle);

				ImGui::LabelText("Zone Point", "%d", door->ZonePoint);
				ImGui::LabelText("Switch?", "%s", door->pSwitch ? "true" : "false");
				ImGui::LabelText("Timestamp", "%d", door->TimeStamp);

				if (ImGui::Button("Reset Position"))
				{
					door->X = door->DefaultX;
					door->Y = door->DefaultY;
					door->Z = door->DefaultZ;
					door->Heading = door->DefaultHeading;
					door->DoorAngle = door->DefaultDoorAngle;
				}

				if (ImGui::TreeNode("Door Dump"))
				{
					ImGui::Columns(4);

					for (int i = 0; i < 55; ++i)
					{
						float* p = (float*)door;
						p += i;

						ImGui::Text("0x%08x (+%04x)", p, (i * 4)); ImGui::NextColumn();
						
						BYTE* b = (BYTE*)p;
						ImGui::Text("%02x %02x %02x %02x", b[0], b[1], b[2], b[3]); ImGui::NextColumn();
						ImGui::Text("0x%08x", *(DWORD*)p); ImGui::NextColumn();
						ImGui::Text("%f", *p); ImGui::NextColumn();
					}

					ImGui::Columns(1);

					ImGui::TreePop();
				}

				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}

	ImGui::End();
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

struct ComboItem { const char* name; DWORD value; };
#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

ComboItem cull_mode_items[] = {
	"None", D3DCULL_NONE,
	"CW",   D3DCULL_CW,
	"CCW",  D3DCULL_CCW,
};

ComboItem alpha_blend_ops[] = {
	"Add", D3DBLENDOP_ADD,
	"Subtract", D3DBLENDOP_SUBTRACT,
	"Reverse Subtract", D3DBLENDOP_REVSUBTRACT,
	"Minimum", D3DBLENDOP_MIN,
	"Maximum", D3DBLENDOP_MAX,
};

ComboItem blend_ops[] = {
	"Zero", D3DBLEND_ZERO,
	"One", D3DBLEND_ONE,
	"Source Color", D3DBLEND_SRCCOLOR,
	"Inverse Source Color", D3DBLEND_DESTCOLOR,
	"Source Alpha", D3DBLEND_SRCALPHA,
	"Inverse Source Alpha", D3DBLEND_INVSRCALPHA,
	"Dest Alpha", D3DBLEND_DESTALPHA,
	"Inverse Dest Alpha", D3DBLEND_INVDESTALPHA,
	"Dest Color", D3DBLEND_DESTCOLOR,
	"Inverse Dest Color", D3DBLEND_INVDESTCOLOR,
	"Source Alpha Saturation", D3DBLEND_SRCALPHASAT,
	"Both Source Alpha", D3DBLEND_BOTHSRCALPHA,
	"Both Inverse Source Alpha", D3DBLEND_BOTHINVSRCALPHA,
	"Blend Factor", D3DBLEND_BLENDFACTOR,
	"Inverse Blend Factor", D3DBLEND_INVBLENDFACTOR,
	"Source Color 2", D3DBLEND_SRCCOLOR2,
	"Inverse Source Color 2", D3DBLEND_INVSRCCOLOR2,
};

ComboItem cmp_func[] = {
	"Never", D3DCMP_NEVER,
	"Less", D3DCMP_LESS,
	"Equal", D3DCMP_EQUAL,
	"Less Equal", D3DCMP_LESSEQUAL,
	"Greater", D3DCMP_GREATER,
	"Not Equal", D3DCMP_NOTEQUAL,
	"Greater Equal", D3DCMP_GREATEREQUAL,
	"Always", D3DCMP_ALWAYS,
};

ComboItem stencil_op[] = {
	"Keep", D3DSTENCILOP_KEEP,
	"Zero", D3DSTENCILOP_ZERO,
	"Replace", D3DSTENCILOP_REPLACE,
	"Incrsat", D3DSTENCILOP_INCRSAT,
	"Decrsat", D3DSTENCILOP_DECRSAT,
	"Invert", D3DSTENCILOP_INVERT,
	"Increase", D3DSTENCILOP_INCR,
	"Decrease", D3DSTENCILOP_DECR,
};

#define ITEM(x) #x, x
ComboItem texture_op[] = {
	ITEM(D3DTOP_DISABLE),
	ITEM(D3DTOP_SELECTARG1),
	ITEM(D3DTOP_SELECTARG2),
	ITEM(D3DTOP_MODULATE),
	ITEM(D3DTOP_MODULATE2X),
	ITEM(D3DTOP_MODULATE4X),
	ITEM(D3DTOP_ADD),
	ITEM(D3DTOP_ADDSIGNED),
	ITEM(D3DTOP_ADDSIGNED2X),
	ITEM(D3DTOP_SUBTRACT),
	ITEM(D3DTOP_ADDSMOOTH),
	ITEM(D3DTOP_BLENDDIFFUSEALPHA),
	ITEM(D3DTOP_BLENDTEXTUREALPHA),
	ITEM(D3DTOP_BLENDFACTORALPHA),
	ITEM(D3DTOP_BLENDTEXTUREALPHAPM),
	ITEM(D3DTOP_BLENDCURRENTALPHA),
	ITEM(D3DTOP_PREMODULATE),
	ITEM(D3DTOP_MODULATEALPHA_ADDCOLOR),
	ITEM(D3DTOP_MODULATECOLOR_ADDALPHA),
	ITEM(D3DTOP_MODULATEINVALPHA_ADDCOLOR),
	ITEM(D3DTOP_MODULATEINVCOLOR_ADDALPHA),
	ITEM(D3DTOP_BUMPENVMAP),
	ITEM(D3DTOP_BUMPENVMAPLUMINANCE),
	ITEM(D3DTOP_DOTPRODUCT3),
	ITEM(D3DTOP_MULTIPLYADD),
	ITEM(D3DTOP_LERP),
};

ComboItem color_arg[] = {
	ITEM(D3DTA_CONSTANT),
	ITEM(D3DTA_CURRENT),
	ITEM(D3DTA_DIFFUSE),
	ITEM(D3DTA_SELECTMASK),
	ITEM(D3DTA_SPECULAR),
	ITEM(D3DTA_TEMP),
	ITEM(D3DTA_TEXTURE),
	ITEM(D3DTA_TFACTOR)
};

#undef ITEM

//bool  Combo(const char* label, int* current_item, <cb>, void* data, int items_count, int height_in_items = -1);
static bool ComboGetter(void* data, int idx, const char** out_text)
{
	ComboItem* items = (ComboItem*)data;
	*out_text = items[idx].name;
	return true;
}

template <typename T, int Size>
static void TypedCombo(const char* label, int& index, T& value, ComboItem(&items)[Size])
{
	if (ImGui::Combo(label, &index, ComboGetter, items, Size))
	{
		value = (T)items[index].value;
	}
}

void ConfigurableRenderState::RenderDebugUI()
{
	if (ImGui::CollapsingHeader("Render States"))
	{
		// alpha blending
		if (ImGui::TreeNode("Alpha Blending"))
		{
			ImGui::Checkbox("Alpha Blend", &m_alphaBlend);

			TypedCombo("Alpha Blend Op", m_blendOpIndex, m_blendOp, alpha_blend_ops);
			TypedCombo("Source Blend", m_sourceBlendIndex, m_sourceBlend, blend_ops);
			TypedCombo("Dest Blend", m_destBlendIndex, m_destBlend, blend_ops);

			ImGui::TreePop();
		}

		// alpha testing
		if (ImGui::TreeNode("Alpha Testing"))
		{
			ImGui::Checkbox("Enabled", &m_alphaTestEnable);
			ImGui::SliderInt("Alpha Ref", &m_alphaRef, 0, 0xff);
			TypedCombo("Alpha Func", m_alphaFuncIndex, m_alphaFunc, cmp_func);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Depth Buffer"))
		{
			ImGui::Columns(2);
			ImGui::Checkbox("Depth Buffering", &m_depthBuffer); ImGui::NextColumn();
			ImGui::Checkbox("Writeable", &m_dephtBufferWriteable); ImGui::NextColumn();
			ImGui::Columns(1);
			TypedCombo("Func", m_zfuncIndex, m_zFunc, cmp_func);

			ImGui::TreePop();
		}

		// misc stuff
		if (ImGui::TreeNode("Misc"))
		{
			TypedCombo("Cull Mode", m_cullModeIndex, m_cullMode, cull_mode_items);
			ImGui::Checkbox("Lighting", &m_lighting);
			ImGui::Checkbox("Scissor Test", &m_scissorTestEnable);
			ImGui::Checkbox("Clipping", &m_clipping);

			ImGui::TreePop();
		}


		if (ImGui::TreeNode("Stencil Buffer"))
		{
			ImGui::Checkbox("Enabled", &m_stencilBuffer);

			TypedCombo("Fail", m_stencilFailIndex, m_stencilFail, stencil_op);
			TypedCombo("Z Fail", m_stencilZFailIndex, m_stencilZFail, stencil_op);
			TypedCombo("Pass", m_stencilPassIndex, m_stencilPass, stencil_op);
			TypedCombo("Function", m_stencilFuncIndex, m_stencilFunc, cmp_func);
			ImGui::SliderInt("Reference", &m_stencilRef, 0, 0xff);
			ImGui::InputInt("Mask", (int*)&m_stencilMask, 1, 100, ImGuiInputTextFlags_CharsHexadecimal);
			ImGui::InputInt("Write Mask", (int*)&m_stencilWriteMask, 1, 100, ImGuiInputTextFlags_CharsHexadecimal);

			ImGui::TreePop();
		}
	}

	if (ImGui::CollapsingHeader("Texture Stage State"))
	{
		//ImGui::Columns(2);
		// can have up to 8 textures, so we will loop 8 times!
		for (int stage = 0; stage < 8; stage++)
		{
			TextureStageData& Tex = m_textureData[stage];
			ImGui::Separator();
			ImGui::PushID(stage);

			std::string label = "Stage " + std::to_string(stage);
			ImGui::Text("Stage %d", stage);
			//ImGui::BeginChild(label.c_str(), ImVec2(0, 200), true);

			TypedCombo("Color Operation", Tex.m_colorOpIndex, Tex.m_colorOp, texture_op);
			TypedCombo("Color Arg 1", Tex.m_colorArg1Index, Tex.m_colorArg1, color_arg);
			TypedCombo("Color Arg 2", Tex.m_colorArg2Index, Tex.m_colorArg2, color_arg);

			TypedCombo("Alpha Operation", Tex.m_alphaOpIndex, Tex.m_alphaOp, texture_op);
			TypedCombo("Alpha Arg 1", Tex.m_alphaArg1Index, Tex.m_alphaArg1, color_arg);
			TypedCombo("Alpha Arg 2", Tex.m_alphaArg2Index, Tex.m_alphaArg2, color_arg);

			ImVec4 color = ImGui::ColorConvertU32ToFloat4(Tex.m_colorConstant);
			ImGui::ColorEdit4("Constant Color", (float*)&color);
			Tex.m_colorConstant = ImGui::ColorConvertFloat4ToU32(color);

			//m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
			//m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
			//m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
			//m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
			//m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);


			ImGui::PopID();
			//ImGui::EndChild();
			//ImGui::NextColumn();
		}
		//ImGui::Columns(1);
	}

	if (ImGui::CollapsingHeader("Capabilities"))
	{
		ImGui::Text("Supports Constant: %s", (m_caps.PrimitiveMiscCaps & D3DPMISCCAPS_PERSTAGECONSTANT) ? "true" : "false");
	}
}

void ConfigurableRenderState::ApplyState(IDirect3DDevice9* device)
{
	static bool only_once = false;
	if (!only_once)
	{
		only_once = true;
		device->GetDeviceCaps(&m_caps);
	}

	device->SetRenderState(D3DRS_CULLMODE, m_cullMode);
	device->SetRenderState(D3DRS_LIGHTING, m_lighting);
	device->SetRenderState(D3DRS_ZENABLE, m_depthBuffer);
	device->SetRenderState(D3DRS_ZWRITEENABLE, m_dephtBufferWriteable);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, m_alphaBlend);
	device->SetRenderState(D3DRS_BLENDOP, m_blendOp);
	device->SetRenderState(D3DRS_SRCBLEND, m_sourceBlend);
	device->SetRenderState(D3DRS_DESTBLEND, m_destBlend);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, m_scissorTestEnable);
	device->SetRenderState(D3DRS_ZFUNC, m_zFunc);
	device->SetRenderState(D3DRS_CLIPPING, m_clipping);

	device->SetRenderState(D3DRS_ALPHATESTENABLE, m_alphaTestEnable);
	device->SetRenderState(D3DRS_ALPHAFUNC, m_alphaFunc);
	device->SetRenderState(D3DRS_ALPHAREF, (DWORD)m_alphaRef);

	device->SetRenderState(D3DRS_STENCILENABLE, m_stencilBuffer);
	device->SetRenderState(D3DRS_STENCILFAIL, m_stencilFail);
	device->SetRenderState(D3DRS_STENCILZFAIL, m_stencilZFail);
	device->SetRenderState(D3DRS_STENCILPASS, m_stencilPass);
	device->SetRenderState(D3DRS_STENCILFUNC, m_stencilFunc);
	device->SetRenderState(D3DRS_STENCILREF, m_stencilRef);
	device->SetRenderState(D3DRS_STENCILMASK, m_stencilMask);
	device->SetRenderState(D3DRS_STENCILWRITEMASK, m_stencilWriteMask);

	for (int i = 0; i < 8; i++)
	{
		device->SetTextureStageState(i, D3DTSS_COLOROP, m_textureData[i].m_colorOp);
		device->SetTextureStageState(i, D3DTSS_COLORARG1, m_textureData[i].m_colorArg1);
		device->SetTextureStageState(i, D3DTSS_COLORARG2, m_textureData[i].m_colorArg2);
		device->SetTextureStageState(i, D3DTSS_ALPHAOP, m_textureData[i].m_alphaOp);
		device->SetTextureStageState(i, D3DTSS_ALPHAARG1, m_textureData[i].m_alphaArg1);
		device->SetTextureStageState(i, D3DTSS_ALPHAARG2, m_textureData[i].m_alphaArg2);
	}
}