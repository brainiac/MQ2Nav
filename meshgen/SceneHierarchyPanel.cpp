
#include "pch.h"
#include "SceneHierarchyPanel.h"

#include "meshgen/Editor.h"
#include "meshgen/Scene.h"

SceneHierarchyPanel::SceneHierarchyPanel(Editor* app)
	: PanelWindow("Scene Hierarchy", "SceneHierarchyPanel")
	, m_editor(app)
{
}

SceneHierarchyPanel::~SceneHierarchyPanel()
{
}

void SceneHierarchyPanel::OnImGuiRender(bool* p_open)
{
}

void SceneHierarchyPanel::SetScene(const std::shared_ptr<Scene>& scene)
{
	m_scene = scene;
}
