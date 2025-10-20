#pragma once
#include "plugin.h"

using namespace plugin;

unsigned int GfxCoreAddress(unsigned int addr);
void InitGfxCoreHook(unsigned int entryPoint, void(*callback)());
