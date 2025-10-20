#include "MyLog.h"
#include "MySettings.h"

std::wstring &LogPath() {
	static std::wstring logPath = FIFA::GameDirPath(L"plugins\\AssetLoader.log");
	return logPath;
}

void OpenLog() {
	DWORD attrs = GetFileAttributesW(LogPath().c_str());
	if (attrs & FILE_ATTRIBUTE_READONLY)
		SetFileAttributesW(LogPath().c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);
	DeleteFileW(LogPath().c_str());
	Log("");
}

void CloseLog() {}

void Log(std::string const &text) {
	if (settings().ENABLE_LOG) {
		FILE *file = _wfopen(LogPath().c_str(), L"at");
		if (file) {
			fputs(text.c_str(), file);
			fclose(file);
		}
	}
}
