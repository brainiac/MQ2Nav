#pragma once

#include <entt/entity/entity.hpp>
#include <vector>

class SelectionManager
{
public:
	SelectionManager();
	~SelectionManager();

	void Select(entt::entity entity);
	void Deselect(entt::entity entity);
	void Clear();
	bool IsSelected(entt::entity entity) const;

	const std::vector<entt::entity>& GetSelection() const { return m_selection; }

private:
	std::vector<entt::entity> m_selection;
};
