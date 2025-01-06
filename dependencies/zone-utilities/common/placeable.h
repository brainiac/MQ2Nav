
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <memory>
#include <vector>

namespace EQEmu {

class Placeable
{
public:
	void SetPosition(float nx, float ny, float nz) { pos.x = nx; pos.y = ny; pos.z = nz; }
	void SetPosition(const glm::vec3& pos_) { pos = pos_; }

	float GetX() const { return pos.x; }
	float GetY() const { return pos.y; }
	float GetZ() const { return pos.z; }
	glm::vec3 GetPosition() const { return pos; }

	void SetRotation(float nx, float ny, float nz) { rotate.x = nx; rotate.y = ny; rotate.z = nz; }
	void SetRotation(const glm::vec3& rot_) { rotate = rot_; }

	float GetRotateX() const { return rotate.x; }
	float GetRotateY() const { return rotate.y; }
	float GetRotateZ() const { return rotate.z; }
	glm::vec3 GetRotation() const { return rotate; }

	void SetScale(float nx, float ny, float nz) { scale.x = nx; scale.y = ny; scale.z = nz; }
	void SetScale(float nx) { scale.x = nx; scale.y = nx; scale.z = nx; }
	void SetScale(const glm::vec3& scale_) { scale = scale_; }

	float GetScaleX() const { return scale.x; }
	float GetScaleY() const { return scale.y; }
	float GetScaleZ() const { return scale.z; }
	glm::vec3 GetScale() const { return scale; }

	void SetName(const std::string& name) { tag = name; }
	const std::string& GetName() const { return tag; }

	void SetFileName(const std::string& name) { model_file = name; }
	const std::string& GetFileName() const { return model_file; }

	glm::mat4 GetTransform()
	{
		glm::mat4x4 mtx;
		mtx = glm::translate(mtx, pos);
		mtx = glm::scale(mtx, scale);
		mtx *= glm::mat4_cast(glm::quat(rotate));

		return mtx;
	}

	std::string tag;
	glm::vec3 pos;
	glm::vec3 rotate;
	glm::vec3 scale;
	std::string model_file;
};

class PlaceableGroup
{
public:
	void SetFromTOG(bool v) { from_tog = v; }
	bool GetFromTOG() const { return from_tog; }

	void SetPosition(float nx, float ny, float nz) { pos.x = nx; pos.y = ny; pos.z = nz; }
	void SetPosition(const glm::vec3& pos_) { pos = pos_; }
	float GetX() const { return pos.x; }
	float GetY() const { return pos.y; }
	float GetZ() const { return pos.z; }
	glm::vec3 GetPosition() const { return pos; }

	void SetTilePosition(float nx, float ny, float nz) { tile_pos.x = nx; tile_pos.y = ny; tile_pos.z = nz; }
	void SetTilePosition(glm::vec3& tile_pos_) { tile_pos = tile_pos_; }
	float GetTileX() const { return tile_pos.x; }
	float GetTileY() const { return tile_pos.y; }
	float GetTileZ() const { return tile_pos.z; }
	glm::vec3 GetTilePosition() const { return tile_pos; }

	void SetRotation(float nx, float ny, float nz) { rotate.x = nx; rotate.y = ny; rotate.z = nz; }
	void SetRotation(const glm::vec3& rotate_) { rotate = rotate_; }
	float GetRotationX() const { return rotate.x; }
	float GetRotationY() const { return rotate.y; }
	float GetRotationZ() const { return rotate.z; }
	glm::vec3 GetRotation() const { return rotate; }

	void SetScale(float nx, float ny, float nz) { scale.x = nx; scale.y = ny; scale.z = nz; }
	void SetScale(const glm::vec3& scale_) { scale = scale_; }
	float GetScaleX() const { return scale.x; }
	float GetScaleY() const { return scale.y; }
	float GetScaleZ() const { return scale.z; }
	glm::vec3 GetScale() const { return scale; }

	void AddPlaceable(std::shared_ptr<Placeable> p) { placeables.push_back(p); }
	std::vector<std::shared_ptr<Placeable>>& GetPlaceables() { return placeables; }

	glm::mat4 GetTransform()
	{
		glm::mat4x4 grp_mat;

		grp_mat = glm::translate(grp_mat, tile_pos);
		grp_mat = glm::translate(grp_mat, pos);
		grp_mat = glm::scale(grp_mat, scale);
		grp_mat *= glm::mat4_cast(glm::quat{glm::radians(rotate)});

		return grp_mat;
	}

	bool from_tog;
	glm::vec3 pos;
	glm::vec3 tile_pos;
	glm::vec3 rotate;
	glm::vec3 scale;

	std::vector<std::shared_ptr<Placeable>> placeables;
};


} // namespace EQEmu
