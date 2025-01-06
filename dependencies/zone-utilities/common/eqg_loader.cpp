
#include "eqg_loader.h"
#include "eqg_structs.h"
#include "safe_alloc.h"
#include "log_macros.h"

namespace EQEmu {
	
bool EQGLoader::Load(PFS::Archive* archive, const std::string& file)
{
	// find zon file
	m_archive = archive;

	std::vector<char> zon;
	bool zon_found = false;
	std::vector<std::string> files;
	m_archive->GetFilenames("zon", files);

	if (files.size() == 0)
	{
		if (GetZon(file + ".zon", zon))
		{
			zon_found = true;
		}
	}
	else
	{
		for (auto& f : files)
		{
			if (m_archive->Get(f, zon))
			{
				if (zon[0] == 'E' && zon[1] == 'Q' && zon[2] == 'T' && zon[3] == 'Z' && zon[4] == 'P')
				{
					eqLogMessage(LogWarn, "Unable to parse the zone file, is a eqgv4.");
					return false;
				}

				zon_found = true;
			}
		}
	}

	if (!zon_found)
	{
		eqLogMessage(LogError, "Failed to open %s.eqg because the %s.zon file could not be found.", file.c_str(), file.c_str());
		return false;
	}

	eqLogMessage(LogTrace, "Parsing zone file.");

	if (!ParseZon(zon))
	{
		// if we couldn't parse the zon file then it's probably eqg4
		eqLogMessage(LogWarn, "Unable to parse the zone file, probably eqgv4 style file.");
		return false;
	}

	return true;
}

bool EQGLoader::GetZon(std::string file, std::vector<char>& buffer)
{
	buffer.clear();

	FILE* f = _fsopen(file.c_str(), "rb", _SH_DENYNO);
	if (f)
	{
		fseek(f, 0, SEEK_END);
		size_t sz = ftell(f);
		rewind(f);

		if (sz != 0)
		{
			buffer.resize(sz);

			size_t bytes_read = fread(&buffer[0], 1, sz, f);

			if (bytes_read != sz)
			{
				fclose(f);
				return false;
			}
			fclose(f);
		}
		else
		{
			fclose(f);
			return false;
		}
		return true;
	}
	return false;
}

bool EQGLoader::ParseZon(std::vector<char>& buffer)
{
	uint32_t idx = 0;
	SafeStructAllocParse(zon_header, header);

	if (header->magic[0] != 'E' || header->magic[1] != 'Q' || header->magic[2] != 'G' || header->magic[3] != 'Z')
	{
		return false;
	}

	idx += header->list_length;

	eqLogMessage(LogTrace, "Parsing zone models.");

	std::vector<std::string> model_names;
	for (uint32_t i = 0; i < header->model_count; ++i)
	{
		SafeVarAllocParse(uint32_t, model);

		std::string mod = &buffer[sizeof(zon_header) + model];
		for (size_t j = 0; j < mod.length(); ++j)
		{
			if (mod[j] == ')')
				mod[j] = '_';
		}
		model_names.push_back(mod);
	}

	// Need to load all the models
	eqLogMessage(LogTrace, "Loading zone models.");

	for (size_t i = 0; i < model_names.size(); ++i)
	{
		std::string mod = model_names[i];
		std::shared_ptr<EQG::Geometry> m(new EQG::Geometry());
		m->SetName(mod);

		if (LoadEQGModel(*m_archive, mod, m))
		{
			models.push_back(m);
		}
		else
		{
			m->GetMaterials().clear();
			m->GetPolygons().clear();
			m->GetVertices().clear();
			models.push_back(m);
		}
	}

	// load placables
	eqLogMessage(LogTrace, "Parsing zone placeables.");

	constexpr float rot_change = 180.0f / 3.14159f;

	for (uint32_t i = 0; i < header->object_count; ++i)
	{
		SafeStructAllocParse(zon_placable, plac);

		std::shared_ptr<Placeable> p = std::make_shared<Placeable>();
		p->SetName(&buffer[sizeof(zon_header) + plac->loc]);

		if (plac->id >= 0 && plac->id < static_cast<int32_t>(models.size()))
		{
			p->SetFileName(model_names[plac->id]);
		}

		p->SetPosition(plac->x, plac->y, plac->z);
		p->SetRotation(plac->rz * rot_change, plac->ry * rot_change, plac->rx * rot_change);
		p->SetScale(plac->scale, plac->scale, plac->scale);

		if (header->version > 1)
		{
			//don't know what this is but it relates to the underlying model
			SafeVarAllocParse(uint32_t, unk_size);
			idx += (unk_size) * sizeof(uint32_t);
		}

		placeables.push_back(p);
	}

	eqLogMessage(LogTrace, "Parsing zone regions.");

	for (uint32_t i = 0; i < header->region_count; ++i)
	{
		SafeStructAllocParse(zon_region, reg);

		std::shared_ptr<EQG::Region> region(new EQG::Region());
		region->SetName(&buffer[sizeof(zon_header) + reg->loc]);
		region->SetLocation(reg->center_x, reg->center_y, reg->center_z);
		region->SetRotation(0.0f, 0.0f, 0.0f);
		region->SetScale(1.0f, 1.0f, 1.0f);
		region->SetExtents(reg->extend_x, reg->extend_y, reg->extend_z);
		region->SetFlags(reg->flag_unknown020, reg->flag_unknown024);
		regions.push_back(region);
	}

	eqLogMessage(LogTrace, "Parsing zone lights.");

	for (uint32_t i = 0; i < header->light_count; ++i)
	{
		SafeStructAllocParse(zon_light, light);
		std::shared_ptr<Light> l(new Light());
		l->tag = std::string(&buffer[sizeof(zon_header) + light->loc]);
		l->pos = glm::vec3(light->x, light->y, light->z);
		l->color.push_back(glm::vec3(light->r, light->g, light->b));
		l->radius = light->radius;

		lights.push_back(l);
	}

	return true;
}

} // namespace EQEmu
