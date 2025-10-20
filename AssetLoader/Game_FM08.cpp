#include "plugin.h"
#include "MyFilesystem.h"
#include "CustomCallbacks.h"
#include "GfxCoreHook.h"

using namespace plugin;
using namespace std;
using namespace MyFilesystem;

namespace fm08 {
    void OnTextureCopyData(void* texInfo, unsigned char* newData, unsigned int newDataSize) {
        unsigned char* currentData = *raw_ptr<unsigned char*>(texInfo, 0);
        unsigned int currentDataSize = 0;
        unsigned char* currentSection = currentData;
        if (*(unsigned int*)currentData & 0xFFFFFF00) {
            do {
                unsigned int nextOffset = *(unsigned int*)currentSection >> 8;
                if (nextOffset)
                    currentSection += nextOffset;
                else
                    currentSection = 0;
            } while (*(unsigned int*)currentSection & 0xFFFFFF00);
        }
        currentDataSize = currentSection - currentData + CallAndReturnDynGlobal<unsigned int>(GfxCoreAddress(0x2CA4AD), currentSection); // GetFshSectionSize() // notdon
        if (currentDataSize < newDataSize) { // if new data doesn't fit into previously allocated memory
            // temporary remove tex info and clear its memory
            CallDynGlobal(GfxCoreAddress(0x353720), texInfo); // RemoveTexInfo() // notdon
            // allocate memory for new texture
            void* newMem = CallAndReturnDynGlobal<void*>(GfxCoreAddress(0x352A90), newData); // TexPoolAllocate() // notdon
            // re-activate tex info and store new memory
            *raw_ptr<unsigned char>(texInfo, 281) = 1; // texInfo.active = true // notdon
            *raw_ptr<void*>(texInfo, 0) = newMem; // texInfo.data = newMem
        }
        else // in other case just copy it
            CallDynGlobal(GfxCoreAddress(0x2BEE20), currentData, newData, newDataSize); // MemCpy() // notdon
    }

    void* OnNewModelUserData(bool isOrd, unsigned char zero, void* sceneEntry, void* block, unsigned int fileSize, const char* fileName) {
        void* oldData = *raw_ptr<void*>(block, 0x50); // block.data // notdon
        void* modelCollection = nullptr;
        void* mc = (void*)GfxCoreAddress(0x71AD98); // ModelCollections // notdon
        for (unsigned int i = 0; i < *(unsigned int*)GfxCoreAddress(0x71ABE8); i++) { // NumModelCollections // notdon
            if (*raw_ptr<void*>(mc, 0x8) == oldData) { // modelCollection.data // notdon
                modelCollection = mc;
                break;
            }
            mc = raw_ptr<void>(mc, 288); // mc++ // notdon
        }
        if (modelCollection) {
            unsigned int newFileSize = max(912u, fileSize); // notdon
            if (*raw_ptr<unsigned int>(modelCollection, 0x10) < newFileSize) { // modelCollection.size // notdon
                static char gMemName[16];
                void *poolName = *raw_ptr<void *>(sceneEntry, 0x60); // sceneEntry.poolName // notdon
                if (poolName)
                    sprintf(gMemName, "SGSM::%s", raw_ptr<char const>(poolName, 1));
                else
                    strcpy(gMemName, "SGSM::none");
                CallDynGlobal(GfxCoreAddress(0x2B3C30), oldData); // delete() // notdon
                void *newData = CallAndReturnDynGlobal<void *>(GfxCoreAddress(0x12880), gMemName, fileName, fileSize, 0x400, 16, 0); // FifaMemAllocate() // notdon
                *raw_ptr<void *>(block, 0x50) = newData; // block.data = newData // notdon
                *raw_ptr<void *>(modelCollection, 0x8) = newData; // modelCollection.data = newData // notdon
                *raw_ptr<unsigned int>(modelCollection, 0x10) = newFileSize; // modelCollection.size = newFileSize // notdon
                fileSize = newFileSize;
            }
        }
        return CallAndReturnDynGlobal<void*>(GfxCoreAddress(0x348D60), isOrd, zero, sceneEntry, block, fileSize, fileName); // NewModelUserData() // notdon
    }
}

void InstallGfx_FM08() {
    using namespace fm08;
    patch::SetUInt(GfxCoreAddress(0x34B933) + 1, 50'000); // fat fix (50000 entries) // notdon
    patch::RedirectCall(GfxCoreAddress(0x353EFD), OnTextureCopyData); // notdon
    patch::SetUShort(GfxCoreAddress(0x353EF8), 0xD68B); // mov edx, [esi] => mov edx, esi // notdon
    patch::RedirectCall(GfxCoreAddress(0x347DFE), OnNewModelUserData); // notdon

    patch::SetUInt(GfxCoreAddress(0x27E0C5 + 1), 32); // notdon

    patch::SetUInt(GfxCoreAddress(0x4B7710), 10485760 * 2); // notdon
    patch::SetUInt(GfxCoreAddress(0x4B7710 + 4), 62914560 * 2); // notdon
    patch::SetUInt(GfxCoreAddress(0xEE5A + 6), 326492416); // notdon
    patch::Nop(GfxCoreAddress(0xEE85), 6); // notdon
    patch::Nop(GfxCoreAddress(0xEE6E), 10); // notdon
    patch::SetUInt(GfxCoreAddress(0xEEA4 + 1), 75497472 * 2); // notdon
    patch::SetUInt(GfxCoreAddress(0xEE78 + 1), 75497472 * 2); // notdon
}
