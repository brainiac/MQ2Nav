#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <algorithm>
#include <functional>
#include <cctype>

#include "water_map.h"
#include "water_map_v1.h"
#include "water_map_v2.h"

WaterMap* WaterMap::LoadWaterMapfile(std::string zone_name) {
	std::transform(zone_name.begin(), zone_name.end(), zone_name.begin(), ::tolower);
		
	std::string file_path = "maps" + std::string("/") + zone_name + std::string(".wtr");
	FILE *f = _fsopen(file_path.c_str(), "rb", _SH_DENYNO);
	if(f) {
		char magic[10];
		uint32_t version;
		if(fread(magic, 10, 1, f) != 1) {
			fclose(f);
			return nullptr;
		}
		
		if(strncmp(magic, "EQEMUWATER", 10)) {
			fclose(f);
			return nullptr;
		}
		
		if(fread(&version, sizeof(version), 1, f) != 1) {
			fclose(f);
			return nullptr;
		}
		
		if(version == 1) {
			WaterMapV1 *wm = new WaterMapV1();
			if(!wm->Load(f)) {
				delete wm;
				wm = nullptr;
			}
			
			fclose(f);
			return wm;
		} else if(version == 2) {
			WaterMapV2 *wm = new WaterMapV2();
			if(!wm->Load(f)) {
				delete wm;
				wm = nullptr;
			}
			
			fclose(f);
			return wm;
		} else {
			fclose(f);
			return nullptr;
		}
	}
	
	return nullptr;
}