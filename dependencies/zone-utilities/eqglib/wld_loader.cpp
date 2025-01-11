#include "buffer_reader.h"
#include "log_macros.h"
#include "pfs.h"
#include "wld_loader.h"
#include "wld_structs.h"

namespace EQEmu::S3D {

static const uint8_t decoder_array[] = { 0x95, 0x3A, 0xC5, 0x2A, 0x95, 0x7A, 0x95, 0x6A };

void decode_s3d_string(char* str, size_t len)
{
	for (size_t i = 0; i < len; ++i)
	{
		str[i] ^= decoder_array[i % 8];
	}
}

WLDLoader::WLDLoader()
{
}

WLDLoader::~WLDLoader()
{
	Reset();
}

void WLDLoader::Reset()
{
	if (m_stringPool)
	{
		delete[] m_stringPool;
	}

	for (auto& obj : m_objects)
	{
		if (obj.parsed_data != nullptr)
		{
			delete obj.parsed_data;
			obj.parsed_data = nullptr;
		}
	}

	m_objects.clear();
}

bool WLDLoader::Init(PFS::Archive* archive, const std::string& wld_name)
{
	if (archive == nullptr || wld_name.empty())
	{
		return false;
	}

	m_archive = archive;
	m_fileName = wld_name;

	if (!archive->Exists(wld_name))
		return false; // no error log if file doesn't exist.

	// This will whole the entire contents of the .wld file
	m_wldFileContents = m_archive->Get(wld_name, m_wldFileSize);
	if (!m_wldFileContents)
	{
		eqLogMessage(LogWarn, "Unable to open wld file %s.", wld_name.c_str());
		return false;
	}

	BufferReader reader(m_wldFileContents, m_wldFileSize);

	wld_header header;
	if (!reader.read(header))
	{
		eqLogMessage(LogWarn, "Unable to read wld header.");
		return false;
	}

	if (header.magic != '\x02\x3dPT')
	{
		eqLogMessage(LogError, "Header magic of %x did not match expected", header.magic);
		return false;
	}

	m_version = header.version;
	m_numObjects = header.fragments;
	m_numRegions = header.unk1;
	m_maxObjectSize = header.unk2;
	m_stringPoolSize = header.hash_length;
	m_numStrings = header.unk3;
	m_oldVersion = header.version == 0x00015500;

	if (m_stringPoolSize)
	{
		m_stringPool = new char[m_stringPoolSize];
		if (!reader.read(m_stringPool, m_stringPoolSize))
		{
			eqLogMessage(LogWarn, "Unable to read string pool.");
			return false;
		}

		decode_s3d_string(m_stringPool, m_stringPoolSize);
	}

	eqLogMessage(LogTrace, "Parsing WLD objects.");

	m_objects.resize(m_numObjects + 1);
	memset(m_objects.data(), 0, sizeof(S3DFileObject) * (m_numObjects + 1));

	uint32_t index = 0;
	for (; index < m_numObjects; ++index)
	{
		uint32_t objectSize, type;
		if (!reader.read(objectSize))
		{
			eqLogMessage(LogWarn, "Unable to read object size (%d).", index);
			return false;
		}

		if (!reader.read(type))
		{
			eqLogMessage(LogWarn, "Unable to read object type (%d).", index);
			return false;
		}

		auto& obj = m_objects[index + 1];
		obj.size = objectSize;
		obj.type = static_cast<S3DObjectType>(type);

		// Read the raw data from the buffer
		obj.data = m_wldFileContents.get() + reader.pos();
		if (!reader.skip(objectSize))
		{
			eqLogMessage(LogWarn, "Unable to read object data (%d).", index);
			return false;
		}

		// Read the tag
		if (type == WLD_OBJ_DEFAULTPALETTEFILE_TYPE || type == WLD_OBJ_WORLD_USERDATA_TYPE)
		{
			eqLogMessage(LogWarn, "Not going to read tag ID (%d) type (%d).", index, type);
		}
		else if (type != WLD_OBJ_CONSTANTAMBIENT_TYPE)
		{
			BufferReader tagBuffer(obj.data, obj.size);

			int tagID;
			if (!tagBuffer.read(tagID))
			{
				eqLogMessage(LogWarn, "Unable to read tag ID (%d).", index);
				return false;
			}

			if (tagID < 0)
			{
				obj.tag = &m_stringPool[-tagID];
			}
		}
	}

	if (index < m_numObjects)
	{
		eqLogMessage(LogWarn, "Failed to read all objects (%d < %d).", index, m_numObjects);
		return false;
	}

	if (!ParseWLDObjects())
		return false;

	m_valid = true;
	return true;
}

uint32_t WLDLoader::GetObjectIndexFromID(int nID, uint32_t currID)
{
	if (nID >= 0)
	{
		// positive IDs are object IDs.
		return static_cast<uint32_t>(nID);
	}

	// negative IDs mean string tag ids.
	std::string_view tag = GetString(nID);

	// if a current Id is provided, scan backwards from that index.
	if (currID != (uint32_t)-1)
	{
		for (uint32_t i = currID; i > 0; --i)
		{
			if (m_objects[i].tag == tag)
			{
				return i;
			}
		}

		return 0;
	}

	// otherwise, scan the entire list.
	for (uint32_t i = 1; i <= m_numObjects; ++i)
	{
		if (m_objects[i].tag == tag)
		{
			return i;
		}
	}

	return 0;
}

bool WLDLoader::ParseWLDFile(WLDLoader& loader,
	const std::string& file_name, const std::string& wld_name)
{
	EQEmu::PFS::Archive archive;
	if (!archive.Open(file_name))
	{
		eqLogMessage(LogWarn, "Unable to open file %s.", file_name.c_str());
		return false;
	}

	if (!loader.Init(&archive, wld_name))
	{
		eqLogMessage(LogWarn, "Unable to open wld file %s.", wld_name.c_str());
		return false;
	}

	return true;
}

bool WLDLoader::ParseWLDObjects()
{
	for (uint32_t i = 0; i <= m_numObjects; ++i)
	{
		S3DFileObject& obj = m_objects[i];
		eqLogMessage(LogTrace, "Dispatching WLD object of type %x", obj.type);

		switch (obj.type)
		{
		case WLD_OBJ_BMINFO_TYPE: {
			obj.parsed_data = new WLDFragment03(this, &obj);
			break;
		}
		case WLD_OBJ_SIMPLESPRITEDEFINITION_TYPE: {
			obj.parsed_data = new WLDFragment04(this, &obj);
			break;
		}
		case WLD_OBJ_SIMPLESPRITEINSTANCE_TYPE: {
			obj.parsed_data = new WLDFragment05(this, &obj);
			break;
		}
		case WLD_OBJ_HIERARCHICALSPRITEDEFINITION_TYPE: {
			obj.parsed_data = new WLDFragment10(this, &obj);
			break;
		}
		case WLD_OBJ_HIERARCHICALSPRITEINSTANCE_TYPE: {
			obj.parsed_data = new WLDFragment11(this, &obj);
			break;
		}
		case WLD_OBJ_TRACKDEFINITION_TYPE: {
			obj.parsed_data = new WLDFragment12(this, &obj);
			break;
		}
		case WLD_OBJ_TRACKINSTANCE_TYPE: {
			obj.parsed_data = new WLDFragment13(this, &obj);
			break;
		}
		case WLD_OBJ_ACTORDEFINITION_TYPE: {
			obj.parsed_data = new WLDFragment14(this, &obj);
			break;
		}
		case WLD_OBJ_ACTORINSTANCE_TYPE: {
			obj.parsed_data = new WLDFragment15(this, &obj);
			break;
		}
		case WLD_OBJ_LIGHTDEFINITION_TYPE: {
			obj.parsed_data = new WLDFragment1B(this, &obj);
			break;
		}
		case WLD_OBJ_LIGHTINSTANCE_TYPE: {
			obj.parsed_data = new WLDFragment1C(this, &obj);
			break;
		}
		case WLD_OBJ_WORLDTREE_TYPE: {
			obj.parsed_data = new WLDFragment21(this, &obj);
			break;
		}
		case WLD_OBJ_REGION_TYPE: {
			obj.parsed_data = new WLDFragment22(this, &obj);
			break;
		}
		case WLD_OBJ_POINTLIGHT_TYPE: {
			obj.parsed_data = new WLDFragment28(this, &obj);
			break;
		}
		case WLD_OBJ_ZONE_TYPE: {
			obj.parsed_data = new WLDFragment29(this, &obj);
			break;
		}
		case WLD_OBJ_DMSPRITEINSTANCE_TYPE: {
			obj.parsed_data = new WLDFragment2D(this, &obj);
			break;
		}
		case WLD_OBJ_MATERIALDEFINITION_TYPE: {
			obj.parsed_data = new WLDFragment30(this, &obj);
			break;
		}
		case WLD_OBJ_MATERIALPALETTE_TYPE: {
			obj.parsed_data = new WLDFragment31(this, &obj);
			break;
		}
		case WLD_OBJ_DMSPRITEDEFINITION2_TYPE: {
			obj.parsed_data = new WLDFragment36(this, &obj);
			break;
		}
		default:
			obj.parsed_data = new WLDFragment(&obj);
			break;
		}
	}

	return true;
}

} //  namespace EQEmu
