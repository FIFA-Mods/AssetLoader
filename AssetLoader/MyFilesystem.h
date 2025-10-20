#pragma once
#include <string>

namespace MyFilesystem {

bool exists(const std::string &name);
bool exists(const std::wstring &name);

}
