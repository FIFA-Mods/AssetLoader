#include "MyFilesystem.h"
#include "plugin.h"

bool MyFilesystem::exists(const std::string &name) {
    return GetFileAttributesA(name.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool MyFilesystem::exists(const std::wstring &name) {
    return GetFileAttributesW(name.c_str()) != INVALID_FILE_ATTRIBUTES;
}
