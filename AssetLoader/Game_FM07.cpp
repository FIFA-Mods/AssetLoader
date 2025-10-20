#include "plugin.h"
#include "MyFilesystem.h"
#include "CustomCallbacks.h"
#include "GfxCoreHook.h"

using namespace plugin;
using namespace std;
using namespace MyFilesystem;

namespace fm07 {
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
        currentDataSize = currentSection - currentData + CallAndReturnDynGlobal<unsigned int>(GfxCoreAddress(0x2CA4AD), currentSection); // GetFshSectionSize() // done
        if (currentDataSize < newDataSize) { // if new data doesn't fit into previously allocated memory
            // temporary remove tex info and clear its memory
            CallDynGlobal(GfxCoreAddress(0x353720), texInfo); // RemoveTexInfo() // done
            // allocate memory for new texture
            void* newMem = CallAndReturnDynGlobal<void*>(GfxCoreAddress(0x352A90), newData); // TexPoolAllocate() // done
            // re-activate tex info and store new memory
            *raw_ptr<unsigned char>(texInfo, 281) = 1; // texInfo.active = true // done
            *raw_ptr<void*>(texInfo, 0) = newMem; // texInfo.data = newMem
        }
        else // in other case just copy it
            CallDynGlobal(GfxCoreAddress(0x2BEE20), currentData, newData, newDataSize); // MemCpy() // done
    }

    void* OnNewModelUserData(bool isOrd, unsigned char zero, void* sceneEntry, void* block, unsigned int fileSize, const char* fileName) {
        void* oldData = *raw_ptr<void*>(block, 0x50); // block.data // done
        void* modelCollection = nullptr;
        void* mc = (void*)GfxCoreAddress(0x71AD98); // ModelCollections // done
        for (unsigned int i = 0; i < *(unsigned int*)GfxCoreAddress(0x71ABE8); i++) { // NumModelCollections // done
            if (*raw_ptr<void*>(mc, 0x8) == oldData) { // modelCollection.data // done
                modelCollection = mc;
                break;
            }
            mc = raw_ptr<void>(mc, 288); // mc++ // done
        }
        if (modelCollection) {
            unsigned int newFileSize = max(912u, fileSize); // done
            if (*raw_ptr<unsigned int>(modelCollection, 0x10) < newFileSize) { // modelCollection.size // done
                static char gMemName[16];
                void *poolName = *raw_ptr<void *>(sceneEntry, 0x60); // sceneEntry.poolName // done
                if (poolName)
                    sprintf(gMemName, "SGSM::%s", raw_ptr<char const>(poolName, 1));
                else
                    strcpy(gMemName, "SGSM::none");
                CallDynGlobal(GfxCoreAddress(0x2B3C30), oldData); // delete() // done
                void *newData = CallAndReturnDynGlobal<void *>(GfxCoreAddress(0x12880), gMemName, fileName, fileSize, 0x400, 16, 0); // FifaMemAllocate() // done
                *raw_ptr<void *>(block, 0x50) = newData; // block.data = newData // done
                *raw_ptr<void *>(modelCollection, 0x8) = newData; // modelCollection.data = newData // done
                *raw_ptr<unsigned int>(modelCollection, 0x10) = newFileSize; // modelCollection.size = newFileSize // done
                fileSize = newFileSize;
            }
        }
        return CallAndReturnDynGlobal<void*>(GfxCoreAddress(0x348D60), isOrd, zero, sceneEntry, block, fileSize, fileName); // NewModelUserData() // done
    }
}

void InstallGfx_FM07() {
    using namespace fm07;
    patch::SetUInt(GfxCoreAddress(0x34B933) + 1, 50'000); // fat fix (50000 entries) // done
    patch::RedirectCall(GfxCoreAddress(0x353EFD), OnTextureCopyData); // done
    patch::SetUShort(GfxCoreAddress(0x353EF8), 0xD68B); // mov edx, [esi] => mov edx, esi // done
    patch::RedirectCall(GfxCoreAddress(0x347DFE), OnNewModelUserData); // done

    patch::SetUInt(GfxCoreAddress(0x27E0C5 + 1), 32); // done

    patch::SetUInt(GfxCoreAddress(0x4B7710), 10485760 * 2); // done
    patch::SetUInt(GfxCoreAddress(0x4B7710 + 4), 62914560 * 2); // done
    patch::SetUInt(GfxCoreAddress(0xEE5A + 6), 326492416); // done
    patch::Nop(GfxCoreAddress(0xEE85), 6); // done
    patch::Nop(GfxCoreAddress(0xEE6E), 10); // done
    patch::SetUInt(GfxCoreAddress(0xEEA4 + 1), 75497472 * 2); // done
    patch::SetUInt(GfxCoreAddress(0xEE78 + 1), 75497472 * 2); // done
}
