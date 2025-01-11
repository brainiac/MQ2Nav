
#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace eqg {

class Material
{
public:
	struct Property
	{
		std::string name;
		uint32_t type;
		int32_t value_i;
		float value_f;
		std::string value_s;
	};

	void SetName(std::string n) { name = n; }
	void SetShader(std::string s) { shader = s; }

	std::string& GetName() { return name; }
	std::string& GetShader() { return shader; }
	std::vector<Property>& GetProperties() { return properties; }

	std::string name;
	std::string shader;
	std::vector<Property> properties;
};

} // namespace eqg
