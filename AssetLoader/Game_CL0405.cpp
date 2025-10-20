#include "plugin.h"
#include "MyFilesystem.h"
#include "CustomCallbacks.h"
#include "GfxCoreHook.h"
#include "MySettings.h"

using namespace plugin;

namespace cl0405 {
    const bool useCustomHeadlods = true;
    const unsigned int NUM_PLAYERS = 25; // 11 * 2 players + 1 referee + 2 linesmen
    struct ResFormatArg { int *a; int b; };
    int PLAYER_HEADLOD1_ID[NUM_PLAYERS];
    int PLAYER_HEADLOD2_ID[NUM_PLAYERS];
    ResFormatArg PLAYER_HEADLOD1_ACCESSORS[NUM_PLAYERS];
    ResFormatArg PLAYER_HEADLOD2_ACCESSORS[NUM_PLAYERS];
    char HeadLodFormat1[64];
    char HeadLodFormat2[64];
    unsigned char NewModelCollections[0x120 * 1024];
    unsigned int gPlayerInitWV = 0x5E2EA0;

    int Ret1() {
        return 1;
    }

    void OnTextureCopyData(void *texInfo, unsigned char *newData, unsigned int newDataSize) {
        unsigned char *currentData = *raw_ptr<unsigned char *>(texInfo, 0);
        unsigned int currentDataSize = 0;
        unsigned char *currentSection = currentData;
        if (*(unsigned int *)currentData & 0xFFFFFF00) {
            do {
                unsigned int nextOffset = *(unsigned int *)currentSection >> 8;
                if (nextOffset)
                    currentSection += nextOffset;
                else
                    currentSection = 0;
            } while (*(unsigned int *)currentSection & 0xFFFFFF00);
        }
        currentDataSize = currentSection - currentData + CallAndReturn<unsigned int, 0x6B62F5>(currentSection); // GetFshSectionSize() +
        if (currentDataSize < newDataSize) { // if new data doesn't fit into previously allocated memory
            // temporary remove tex info and clear its memory
            Call<0x606A90>(texInfo); // RemoveTexInfo() +
            // allocate memory for new texture
            void *newMem = CallAndReturn<void *, 0x605E20>(newData); // TexPoolAllocate() +
            // re-activate tex info and store new memory
            *raw_ptr<unsigned char>(texInfo, 281) = 1; // texInfo.active = true
            *raw_ptr<void *>(texInfo, 0) = newMem; // texInfo.data = newMem
        }
        else // in other case just copy it
            Call<0x6AA6E0>(currentData, newData, newDataSize); // MemCpy() +
    }

    void *OnNewModelUserData(bool isOrd, unsigned char zero, void *sceneEntry, void *block, unsigned int fileSize, const char *fileName) {
        void *oldData = *raw_ptr<void *>(block, 0x50); // block.data
        void *modelCollection = nullptr;
        void *mc = useCustomHeadlods ? NewModelCollections : (void *)0x8A1328; // ModelCollections +
        for (unsigned int i = 0; i < *(unsigned int *)0x8A1178; i++) { // NumModelCollections +
            if (*raw_ptr<void *>(mc, 0x8) == oldData) { // modelCollection.data
                modelCollection = mc;
                break;
            }
            mc = raw_ptr<void>(mc, 288); // mc++
        }
        if (modelCollection) {
            unsigned int newFileSize = max(912u, fileSize);
            if (*raw_ptr<unsigned int>(modelCollection, 0x10) < newFileSize) { // modelCollection.size
                static char gMemName[16];
                void *poolName = *raw_ptr<void *>(sceneEntry, 0x60); // sceneEntry.poolName
                if (poolName)
                    sprintf(gMemName, "SGSM::%s", raw_ptr<char const>(poolName, 1));
                else
                    strcpy(gMemName, "SGSM::none");
                Call<0x6A67E0>(oldData); // delete() +
                void *newData = CallAndReturn<void *, 0x5E6DC0>(gMemName, fileName, fileSize, 0x400, 16, 0); // FifaMemAllocate() +
                *raw_ptr<void *>(block, 0x50) = newData; // block.data = newData
                *raw_ptr<void *>(modelCollection, 0x8) = newData; // modelCollection.data = newData
                *raw_ptr<unsigned int>(modelCollection, 0x10) = newFileSize; // modelCollection.size = newFileSize
                fileSize = newFileSize;
            }
        }
        return CallAndReturn<void *, 0x607B50>(isOrd, zero, sceneEntry, block, fileSize, fileName); // NewModelUserData() +
    }

    void *OnResolveScene(void *scene, void *attributeCallback) {
        void *result = CallAndReturn<void *, 0x608EA0>(scene, attributeCallback); // +
        void *sceneEntry = *raw_ptr<void *>(scene, 0x4C); // +
        unsigned int numSceneEntries = *raw_ptr<unsigned int>(scene, 0x48); // +
        //std::set<int> replaced1, replaced2;
        for (unsigned int i = 0; i < numSceneEntries; i++) {
            char const *sceneEntryName = *raw_ptr<char const *>(sceneEntry, 0x64);
            if (sceneEntryName) {
                string sceneEntryNameStr = ToLower(sceneEntryName);
                int playerIndex = -1;
                if (StartsWith(sceneEntryNameStr, "player(")) {
                    auto spacePos = sceneEntryNameStr.find(' ', 7);
                    if (spacePos != string::npos && spacePos != 7) {
                        try {
                            playerIndex = stoi(sceneEntryNameStr.substr(7, spacePos - 7));
                        }
                        catch (...) {}
                    }
                }
                if (playerIndex >= 0 && playerIndex < NUM_PLAYERS) {
                    void *modelBlock = *raw_ptr<void *>(sceneEntry, 0x44);
                    unsigned int numModelBlocks = *raw_ptr<unsigned int>(sceneEntry, 0x40);
                    for (unsigned int m = 0; m < numModelBlocks; m++) {
                        char const *resNameFormat = *raw_ptr<char const *>(modelBlock, 0x4);
                        if (resNameFormat) {
                            string resNameFormatStr = ToLower(resNameFormat);
                            if (resNameFormatStr == "player__medhead__model1020311275__.o") {
                                *raw_ptr<char const *>(modelBlock, 0x4) = HeadLodFormat1;
                                *raw_ptr<unsigned int>(modelBlock, 0x28) = 1;
                                PLAYER_HEADLOD1_ACCESSORS[playerIndex].a = &PLAYER_HEADLOD1_ID[playerIndex];
                                PLAYER_HEADLOD1_ACCESSORS[playerIndex].b = 0;
                                *raw_ptr<void *>(modelBlock, 0x2C) = &PLAYER_HEADLOD1_ACCESSORS[playerIndex];
                                //replaced1.insert(playerIndex);
                            }
                            else if (resNameFormatStr == "player__lowhead__model1020311335__.o") {
                                *raw_ptr<char const *>(modelBlock, 0x4) = HeadLodFormat2;
                                *raw_ptr<unsigned int>(modelBlock, 0x28) = 1;
                                PLAYER_HEADLOD2_ACCESSORS[playerIndex].a = &PLAYER_HEADLOD2_ID[playerIndex];
                                PLAYER_HEADLOD2_ACCESSORS[playerIndex].b = 0;
                                *raw_ptr<void *>(modelBlock, 0x2C) = &PLAYER_HEADLOD2_ACCESSORS[playerIndex];
                                //replaced2.insert(playerIndex);
                            }
                        }
                        modelBlock = raw_ptr<void>(modelBlock, 0x58);
                    }
                }
            }
            sceneEntry = raw_ptr<void>(sceneEntry, 0x6C); // +
        }
        return result;
    }

    void OnSetupPlayerModel(void *desc) {
        int playerIndex = ((int)desc - 0x880380) / 0x1230; // +
        if (playerIndex >= 0 && playerIndex < NUM_PLAYERS) {
            int hairlodid = *raw_ptr<int>(desc, 0x1004);
            PLAYER_HEADLOD1_ID[playerIndex] = -hairlodid;
            PLAYER_HEADLOD2_ID[playerIndex] = -hairlodid;
            int headid = *raw_ptr<int>(desc, 0xFF8); // +
            //::Warning("player %d (%s)\nheadid %d hairlodid %d", playerIndex, raw_ptr<char const>(desc, 0xFA4), headid, hairlodid);
            if (headid > 0) { // has starhead
                int sizeHeadLod1 = CallMethodAndReturn<int, 0x5E7BD0>(*(void **)0x8A0D44, Format(HeadLodFormat1, headid).c_str()); // +
                if (sizeHeadLod1 > 0)
                    PLAYER_HEADLOD1_ID[playerIndex] = headid;
                int sizeHeadLod2 = CallMethodAndReturn<int, 0x5E7BD0>(*(void **)0x8A0D44, Format(HeadLodFormat2, headid).c_str()); // +
                if (sizeHeadLod2 > 0)
                    PLAYER_HEADLOD2_ID[playerIndex] = headid;
            }
        }
    }

    void __declspec(naked) OnSetupPlayerModelBE() {
        __asm {
            push eax
            call gPlayerInitWV
            call OnSetupPlayerModel
            add esp, 4
            retn
        }
    }
}

void Install_CL0405() {
    using namespace cl0405;
    patch::RedirectJump(0x619BF0, Ret1); // NO-CD
    patch::SetUInt(0x5EA21F + 1, settings().FAT_MAX_ENTRIES); // fat fix (50000 entries) +
    patch::RedirectCall(0x60726D, OnTextureCopyData); // +
    patch::SetUShort(0x607268, 0xD68B); // mov edx, [esi] => mov edx, esi +
    patch::RedirectCall(0x60AE0E, OnNewModelUserData); // +
    if (useCustomHeadlods) {
        patch::RedirectCall(0x5A6851, OnResolveScene); // +
        patch::RedirectCall(0x5E3EF7, OnSetupPlayerModelBE); // +
        patch::RedirectCall(0x5E4726, OnSetupPlayerModelBE); // +
        patch::RedirectCall(0x5E66CE, OnSetupPlayerModelBE); // +
        strcpy(HeadLodFormat1, "Player__medhead__model1020311275__%d.o");
        strcpy(HeadLodFormat2, "Player__lowhead__model1020311335__%d.o");
        patch::SetPointer(0x5FE6A2 + 1, NewModelCollections + 8); // SGR_LoaderClaimNextFree
        patch::SetPointer(0x5FE6B2 + 1, NewModelCollections + std::size(NewModelCollections) + 8); // SGR_LoaderClaimNextFree
        patch::SetPointer(0x5FE6D2 + 1, NewModelCollections); // SGR_LoaderClaimNextFree
        patch::SetPointer(0x5FF164 + 1, NewModelCollections + 8); // SGR_clearModelForSwap
        patch::SetPointer(0x5FF1D7 + 2, NewModelCollections); // SGR_clearModelForSwap
        patch::SetPointer(0x5FFED2 + 1, NewModelCollections + 8); // dynamicLoad
        patch::SetPointer(0x5FFF16 + 1, NewModelCollections); // dynamicLoad
        patch::SetPointer(0x600944 + 2, NewModelCollections); // SGR_deleteAllObjects
        patch::SetPointer(0x600A54 + 2, NewModelCollections); // SGR_purge
        patch::SetPointer(0x600A5C + 2, NewModelCollections); // SGR_purge
        patch::SetPointer(0x6015B4 + 1, NewModelCollections + 8); // SGR_swapClearedModel
        patch::SetPointer(0x60160F + 2, NewModelCollections); // SGR_swapClearedModel
    }
    patch::SetUInt(0x661495 + 1, 32); //+
    patch::SetUChar(0x5BCCB5 + 2, 50); // +
    patch::SetUInt(0x7E1EF8, settings().SGR_TEXTURE_POOL_SIZE_FE); // +
    patch::SetUInt(0x7E1EFC, settings().SGR_TEXTURE_POOL_SIZE_BE); // +
    patch::SetUInt(0x6170CA + 6, settings().REAL_MEMORY_HEAP_SIZE); /* * 2 */ // +
    patch::Nop(0x6170F5, 6); // +
    patch::Nop(0x6170DE, 10); // +
    patch::SetUInt(0x617114 + 1, settings().TEXTURE_MEMORY_SIZE); // +
    patch::SetUInt(0x6170E8 + 1, settings().TEXTURE_MEMORY_SIZE); // +
}
