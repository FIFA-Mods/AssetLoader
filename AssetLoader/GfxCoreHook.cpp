#include "GfxCoreHook.h"

void(*gLoadGfxCoreCallback)() = nullptr;
unsigned int gGfxRetAddr = 0;
unsigned int hLibrary = 0;

struct GfxCoreModule {
    void *pInterface;
    unsigned int hLibrary;
    unsigned int initCallback;
};

unsigned int GfxCoreAddress(unsigned int addr) {
    return hLibrary + addr;
}

void *OnGfxCoreGetPlugin(GfxCoreModule *moduleInfo) {
    hLibrary = moduleInfo->hLibrary;
    if (gLoadGfxCoreCallback)
        gLoadGfxCoreCallback();
    return CallAndReturnDynGlobal<void *>(moduleInfo->initCallback, moduleInfo);
}

void __declspec(naked) SetGfxCoreGetPluginCallback() {
    __asm call OnGfxCoreGetPlugin
    __asm add esp, 4
    __asm mov eax, gGfxRetAddr
    __asm jmp eax
}

void InitGfxCoreHook(unsigned int entryPoint, void(*callback)()) {
    gLoadGfxCoreCallback = callback;
    gGfxRetAddr = entryPoint + 5;
    patch::RedirectJump(entryPoint, SetGfxCoreGetPluginCallback);
}
