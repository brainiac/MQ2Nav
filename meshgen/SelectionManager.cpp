
#include "pch.h"
#include "SelectionManager.h"

#include <ranges>

SelectionManager::SelectionManager()
{
}

SelectionManager::~SelectionManager()
{
}

void SelectionManager::Select(entt::entity entity)
{
	if (!IsSelected(entity))
	{
		m_selection.push_back(entity);
	}
}

void SelectionManager::Deselect(entt::entity entity)
{
	auto it = std::ranges::find(m_selection, entity);
	if (it != end(m_selection))
	{
		m_selection.erase(it);
	}
}

void SelectionManager::Clear()
{
	m_selection.clear();
}

bool SelectionManager::IsSelected(entt::entity entity) const
{
	return std::ranges::find(m_selection, entity) != end(m_selection);
}
