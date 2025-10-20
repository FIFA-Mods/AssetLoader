#pragma once
#include "plugin.h"
#include <vector>

class Settings {
public:
	bool ENABLE_LOG;
	unsigned int FAT_MAX_ENTRIES; // 22500 original, 50000 new
	unsigned int SGR_TEXTURE_POOL_SIZE_BE; // 15.7/60 MB original, 140 MB new
	unsigned int SGR_TEXTURE_POOL_SIZE_FE; // 2/10 MB original, 20 MB new
	unsigned int REAL_MEMORY_HEAP_SIZE; // 75/216 MB original, 350 MB new
	unsigned int TEXTURE_MEMORY_SIZE; // 18/72 MB original, 140 MB new

	Settings();
};

Settings &settings();
