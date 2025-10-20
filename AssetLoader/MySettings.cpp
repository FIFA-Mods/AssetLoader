#include "MySettings.h"
#include "plugin.h"

Settings::Settings() {
	const unsigned int MB_TO_BYTES = 1024 * 1024; 
	const unsigned int LIMIT_FAT_MAX_ENTRIES[2] = { 22'500, 500'000 };
	const unsigned int LIMIT_REAL_MEMORY_HEAP_SIZE[2] = { 75, 2048 };
	const unsigned int LIMIT_SGR_TEXTURE_POOL_SIZE_BE[2] = { 16, 1536 };
	const unsigned int LIMIT_SGR_TEXTURE_POOL_SIZE_FE[2] = { 2, 512 };
	const unsigned int LIMIT_TEXTURE_MEMORY_SIZE[2] = { 6, 1536 };
	const unsigned int DEFAULT_FAT_MAX_ENTRIES = 50'000;
	const unsigned int DEFAULT_REAL_MEMORY_HEAP_SIZE = 350;
	const unsigned int DEFAULT_SGR_TEXTURE_POOL_SIZE_BE = 140;
	const unsigned int DEFAULT_SGR_TEXTURE_POOL_SIZE_FE = 20;
	const unsigned int DEFAULT_TEXTURE_MEMORY_SIZE = 140;
	auto filename = FIFA::GameDirPath(L"plugins\\AssetLoader.ini");
	wchar_t sEnableLog[32];
	GetPrivateProfileStringW(L"MAIN", L"ENABLE_LOG", L"0", sEnableLog, 32, filename.c_str());
	std::wstring strEnableLog = plugin::ToLower(sEnableLog);
	ENABLE_LOG = strEnableLog == L"true" || strEnableLog == L"1";
	FAT_MAX_ENTRIES = GetPrivateProfileIntW(L"MAIN", L"FAT_MAX_ENTRIES", DEFAULT_FAT_MAX_ENTRIES, filename.c_str());
	if (FAT_MAX_ENTRIES < LIMIT_FAT_MAX_ENTRIES[0] || FAT_MAX_ENTRIES > LIMIT_FAT_MAX_ENTRIES[1]) {
		FAT_MAX_ENTRIES = DEFAULT_FAT_MAX_ENTRIES;
		plugin::Warning("FAT_MAX_ENTRIES: the value must be within the [%u;%u] range. The value will be reset to default.",
			LIMIT_FAT_MAX_ENTRIES[0], LIMIT_FAT_MAX_ENTRIES[1]);
	}
	REAL_MEMORY_HEAP_SIZE = GetPrivateProfileIntW(L"MAIN", L"REAL_MEMORY_HEAP_SIZE", DEFAULT_REAL_MEMORY_HEAP_SIZE, filename.c_str());
	if (REAL_MEMORY_HEAP_SIZE < LIMIT_REAL_MEMORY_HEAP_SIZE[0] || REAL_MEMORY_HEAP_SIZE > LIMIT_REAL_MEMORY_HEAP_SIZE[1]) {
		REAL_MEMORY_HEAP_SIZE = DEFAULT_REAL_MEMORY_HEAP_SIZE;
		plugin::Warning("REAL_MEMORY_HEAP_SIZE: the value must be within the [%u;%u] range. The value will be reset to default.",
			LIMIT_REAL_MEMORY_HEAP_SIZE[0], LIMIT_REAL_MEMORY_HEAP_SIZE[1]);
	}
	SGR_TEXTURE_POOL_SIZE_BE = GetPrivateProfileIntW(L"MAIN", L"SGR_TEXTURE_POOL_SIZE_BE", DEFAULT_SGR_TEXTURE_POOL_SIZE_BE, filename.c_str());
	if (SGR_TEXTURE_POOL_SIZE_BE < LIMIT_SGR_TEXTURE_POOL_SIZE_BE[0] || SGR_TEXTURE_POOL_SIZE_BE > LIMIT_SGR_TEXTURE_POOL_SIZE_BE[1]) {
		SGR_TEXTURE_POOL_SIZE_BE = DEFAULT_SGR_TEXTURE_POOL_SIZE_BE;
		plugin::Warning("SGR_TEXTURE_POOL_SIZE_BE: the value must be within the [%u;%u] range. The value will be reset to default.",
			LIMIT_SGR_TEXTURE_POOL_SIZE_BE[0], LIMIT_SGR_TEXTURE_POOL_SIZE_BE[1]);
	}
	SGR_TEXTURE_POOL_SIZE_FE = GetPrivateProfileIntW(L"MAIN", L"SGR_TEXTURE_POOL_SIZE_FE", DEFAULT_SGR_TEXTURE_POOL_SIZE_FE, filename.c_str());
	if (SGR_TEXTURE_POOL_SIZE_FE < LIMIT_SGR_TEXTURE_POOL_SIZE_FE[0] || SGR_TEXTURE_POOL_SIZE_FE > LIMIT_SGR_TEXTURE_POOL_SIZE_FE[1]) {
		SGR_TEXTURE_POOL_SIZE_FE = DEFAULT_SGR_TEXTURE_POOL_SIZE_FE;
		plugin::Warning("SGR_TEXTURE_POOL_SIZE_FE: the value must be within the [%u;%u] range. The value will be reset to default.",
			LIMIT_SGR_TEXTURE_POOL_SIZE_FE[0], LIMIT_SGR_TEXTURE_POOL_SIZE_FE[1]);
	}
	TEXTURE_MEMORY_SIZE = GetPrivateProfileIntW(L"MAIN", L"TEXTURE_MEMORY_SIZE", DEFAULT_TEXTURE_MEMORY_SIZE, filename.c_str());
	if (TEXTURE_MEMORY_SIZE < LIMIT_TEXTURE_MEMORY_SIZE[0] || TEXTURE_MEMORY_SIZE > LIMIT_TEXTURE_MEMORY_SIZE[1]) {
		TEXTURE_MEMORY_SIZE = DEFAULT_TEXTURE_MEMORY_SIZE;
		plugin::Warning("TEXTURE_MEMORY_SIZE: the value must be within the [%u;%u] range. The value will be reset to default.",
			LIMIT_TEXTURE_MEMORY_SIZE[0], LIMIT_TEXTURE_MEMORY_SIZE[1]);
	}
	if ((float)REAL_MEMORY_HEAP_SIZE / ((float)SGR_TEXTURE_POOL_SIZE_BE + (float)SGR_TEXTURE_POOL_SIZE_FE) < 1.2f) {
		plugin::Warning("REAL_MEMORY_HEAP_SIZE must be at least 1.2 times bigger than the sum of "
			"SGR_TEXTURE_POOL_SIZE_BE and SGR_TEXTURE_POOL_SIZE_FE. Values will be reset to default.");
		REAL_MEMORY_HEAP_SIZE = DEFAULT_REAL_MEMORY_HEAP_SIZE;
		SGR_TEXTURE_POOL_SIZE_BE = DEFAULT_SGR_TEXTURE_POOL_SIZE_BE;
		SGR_TEXTURE_POOL_SIZE_FE = DEFAULT_SGR_TEXTURE_POOL_SIZE_FE;
		TEXTURE_MEMORY_SIZE = DEFAULT_TEXTURE_MEMORY_SIZE;
	}
	REAL_MEMORY_HEAP_SIZE *= MB_TO_BYTES;
	SGR_TEXTURE_POOL_SIZE_BE *= MB_TO_BYTES;
	SGR_TEXTURE_POOL_SIZE_FE *= MB_TO_BYTES;
	TEXTURE_MEMORY_SIZE *= MB_TO_BYTES;
}

Settings &settings() {
	static Settings s;
	return s;
}
