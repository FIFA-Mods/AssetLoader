#include "plugin.h"
#include "MyFilesystem.h"
#include "CustomCallbacks.h"
#include "MySettings.h"

using namespace plugin;
using namespace std;
using namespace MyFilesystem;

namespace wc06 {
    const unsigned int MAX_TEX_COLLECTIONS = 200; // default 200
    const unsigned int MAX_TEXTURES_IN_FSH = 1024; // default 64
    const unsigned int MAX_TEX_INFOS = 1280; // default 320
    const unsigned int MAX_TEX_DYNAMIC_DATA_PTRS = 1024; // default 256
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
    unsigned char NewTextureCollections[(0x10C + (MAX_TEXTURES_IN_FSH - 64) * 4) * MAX_TEX_COLLECTIONS];
    unsigned char NewTexInfos[0x11C * MAX_TEX_INFOS];
    unsigned char NewTexDynamicDataPtrs[4 * MAX_TEX_DYNAMIC_DATA_PTRS];
    unsigned int NewUsedTags[MAX_TEXTURES_IN_FSH];

    void __declspec(naked) OnCompareTexTag1() { //+
        __asm {
            cmp NewUsedTags[eax * TYPE int], ecx
            mov edx, 0x5563DB
            jmp edx
        }
    }

    void __declspec(naked) OnCompareTexTag2() { //+
        __asm {
            mov NewUsedTags[esi * TYPE int], edi
            mov edx, 0x556630
            jmp edx
        }
    }
    template<int S>
    void *WC06NumberCB1(int a1, int a2, int a3, int a4, int a5, int a6, int a7) { //+
        return CallAndReturn<void *, 0x555B20>(a1, S, S, a4, a5, a6, a7);
    }

    template<int S>
    void *WC06NumberCB2(int a1, int a2, int a3, int a4, int a5, int a6) { //+
        return CallAndReturn<void *, 0x555F60>(a1, S, S, a4, a5, a6);
    }

    void METHOD MyGetHotspotData(void *t, DUMMY_ARG, int *hs, void *data) { //+
        CallMethod<0x4F3C90>(t, hs, data);
        for (int i = 0; i < 12; i++) {
            hs[i + 1] *= 2;
        }
    }

    void OnTextureCopyData(void *texInfo, unsigned char *newData, unsigned int newDataSize) { //+
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
        currentDataSize = currentSection - currentData + CallAndReturn<unsigned int, 0x723AAD>(currentSection); // GetFshSectionSize()
        if (currentDataSize < newDataSize) { // if new data doesn't fit into previously allocated memory
            // temporary remove tex info and clear its memory
            Call<0x556090>(texInfo); // RemoveTexInfo()
            // allocate memory for new texture
            void *newMem = CallAndReturn<void *, 0x555360>(newData); // TexPoolAllocate()
            // re-activate tex info and store new memory
            *raw_ptr<unsigned char>(texInfo, 281) = 1; // texInfo.active = true
            *raw_ptr<void *>(texInfo, 0) = newMem; // texInfo.data = newMem
        }
        else // in other case just copy it
            Call<0x716C03>(currentData, newData, newDataSize); // MemCpy()
    }

    void *OnNewModelUserData(bool isOrd, unsigned char zero, void *sceneEntry, void *block, unsigned int fileSize, const char *fileName) {
        void *oldData = *raw_ptr<void *>(block, 0x50); // block.data //+
        void *modelCollection = nullptr;
        void *mc = useCustomHeadlods ? NewModelCollections : (void *)0x949320; // ModelCollections //+
        for (unsigned int i = 0; i < *(unsigned int *)0x949150; i++) { // NumModelCollections //+
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
                Call<0x715536>(oldData); // delete() //+
                void *newData = CallAndReturn<void *, 0x53BBA0>(gMemName, fileName, fileSize, 0x400, 16, 0); // FifaMemAllocate() //+
                *raw_ptr<void *>(block, 0x50) = newData; // block.data = newData
                *raw_ptr<void *>(modelCollection, 0x8) = newData; // modelCollection.data = newData
                *raw_ptr<unsigned int>(modelCollection, 0x10) = newFileSize; // modelCollection.size = newFileSize
                fileSize = newFileSize;
            }
        }
        return CallAndReturn<void *, 0x5574A0>(isOrd, zero, sceneEntry, block, fileSize, fileName); // NewModelUserData() //+
    }

    void *OnResolveScene(void *scene, void *attributeCallback) {
        void *result = CallAndReturn<void *, 0x558750>(scene, attributeCallback); //+
        void *sceneEntry = *raw_ptr<void *>(scene, 0x44);
        unsigned int numSceneEntries = *raw_ptr<unsigned int>(scene, 0x40);
        for (unsigned int i = 0; i < numSceneEntries; i++) {
            char const *sceneEntryName = *raw_ptr<char const *>(sceneEntry, 0x64);
            if (sceneEntryName) {
                string sceneEntryNameStr = sceneEntryName;
                if (StartsWith(sceneEntryNameStr, "Player(")) {
                    auto spacePos = sceneEntryNameStr.find(' ', 7);
                    if (spacePos != string::npos && spacePos != 7) {
                        int playerIndex = -1;
                        try {
                            playerIndex = stoi(sceneEntryNameStr.substr(7, spacePos - 7));
                        }
                        catch (...) {}
                        if (playerIndex >= 0 && playerIndex < NUM_PLAYERS) {
                            void *modelBlock = *raw_ptr<void *>(sceneEntry, 0x44);
                            unsigned int numModelBlocks = *raw_ptr<unsigned int>(sceneEntry, 0x40);
                            for (unsigned int m = 0; m < numModelBlocks; m++) {
                                char const *resNameFormat = *raw_ptr<char const *>(modelBlock, 0x4);
                                if (resNameFormat) {
                                    string resNameFormatStr = resNameFormat;
                                    if (resNameFormatStr == "m46__.o") {
                                        *raw_ptr<char const *>(modelBlock, 0x4) = HeadLodFormat1;
                                        *raw_ptr<unsigned int>(modelBlock, 0x28) = 1;
                                        PLAYER_HEADLOD1_ACCESSORS[playerIndex].a = &PLAYER_HEADLOD1_ID[playerIndex];
                                        PLAYER_HEADLOD1_ACCESSORS[playerIndex].b = 0;
                                        *raw_ptr<void *>(modelBlock, 0x2C) = &PLAYER_HEADLOD1_ACCESSORS[playerIndex];
                                    }
                                    else if (resNameFormatStr == "m47__.o") {
                                        *raw_ptr<char const *>(modelBlock, 0x4) = HeadLodFormat2;
                                        *raw_ptr<unsigned int>(modelBlock, 0x28) = 1;
                                        PLAYER_HEADLOD2_ACCESSORS[playerIndex].a = &PLAYER_HEADLOD2_ID[playerIndex];
                                        PLAYER_HEADLOD2_ACCESSORS[playerIndex].b = 0;
                                        *raw_ptr<void *>(modelBlock, 0x2C) = &PLAYER_HEADLOD2_ACCESSORS[playerIndex];
                                    }
                                }
                                modelBlock = raw_ptr<void>(modelBlock, 0x58);
                            }
                        }
                    }
                }
            }
            sceneEntry = raw_ptr<void>(sceneEntry, 0x6C);
        }
        return result;
    }

    void OnSetupPlayerModel(void *desc) { //+
        int playerIndex = ((int)desc - 0x928880) / 0x11A0; //+
        if (playerIndex >= 0 && playerIndex < NUM_PLAYERS) {
            int hairlodid = *raw_ptr<int>(desc, 0xFFC);
            PLAYER_HEADLOD1_ID[playerIndex] = -hairlodid;
            PLAYER_HEADLOD2_ID[playerIndex] = -hairlodid;
            int headid = *raw_ptr<int>(desc, 0xFD4); //+ head id
            if (headid > 0) { // has starhead
                int sizeHeadLod1 = CallMethodAndReturn<int, 0x53CD20>(*(void **)0x948D38, Format(HeadLodFormat1, headid).c_str());
                if (sizeHeadLod1 > 0)
                    PLAYER_HEADLOD1_ID[playerIndex] = headid;
                int sizeHeadLod2 = CallMethodAndReturn<int, 0x53CD20>(*(void **)0x948D38, Format(HeadLodFormat2, headid).c_str());
                if (sizeHeadLod2 > 0)
                    PLAYER_HEADLOD2_ID[playerIndex] = headid;
            }
        }
    }

    void __declspec(naked) OnSetupPlayerModelBE() { //+
        __asm {
            mov [esi + 0xE38], edx
            push esi
            call OnSetupPlayerModel
            add esp, 4
            mov edx, 0x539329
            jmp edx
        }
    }

    void METHOD OnSetupPlayerModelFE(void *p) { //+
        CallMethod<0x4DE400>(p); //+
        OnSetupPlayerModel((void *)0x928880); //+
    }

    void OnLoadTexturesToTexCollection(const char *poolName, int numFshs, void *fshCollection, char bReload, unsigned int maxTextures, unsigned int *outNumTextures, void *texCollection, void *sceneGroup) { //+
        Call<0x556320>(poolName, numFshs, fshCollection, bReload, MAX_TEXTURES_IN_FSH, outNumTextures, texCollection, sceneGroup);
    }
}

void Install_WC06() {
    using namespace wc06;
    patch::SetUInt(0x53EDCF + 1, settings().FAT_MAX_ENTRIES); // fat fix (50000 entries) //+
    patch::RedirectCall(0x556865, OnTextureCopyData); //+
    patch::SetUShort(0x556860, 0xD68B); // mov edx, [esi] => mov edx, esi //+
    patch::RedirectCall(0x559F80, OnNewModelUserData); //+
    if (useCustomHeadlods) {
        patch::RedirectCall(0x4D8CB1, OnResolveScene); //+
        patch::RedirectCall(0x4DD174, OnResolveScene); //+
        patch::RedirectJump(0x539323, OnSetupPlayerModelBE); //+
        patch::RedirectCall(0x4DF1AE, OnSetupPlayerModelFE); //+
        patch::SetPointer(0x54BD92 + 1, NewModelCollections + 8); //+
        patch::SetPointer(0x54BDA2 + 1, NewModelCollections + std::size(NewModelCollections) + 8); //+
        patch::SetPointer(0x54BDC2 + 1, NewModelCollections); //+
        patch::SetPointer(0x54CBC4 + 1, NewModelCollections + 8); //+
        patch::SetPointer(0x54CC37 + 2, NewModelCollections); //+
        patch::SetPointer(0x54D972 + 1, NewModelCollections + 8); //+
        patch::SetPointer(0x54D9B6 + 1, NewModelCollections); //+
        patch::SetPointer(0x54E3E4 + 2, NewModelCollections); //+
        patch::SetPointer(0x54E4F4 + 2, NewModelCollections); //+
        patch::SetPointer(0x54E4FC + 2, NewModelCollections); //+
        patch::SetPointer(0x54EFA7 + 1, NewModelCollections + 8); //+
        patch::SetPointer(0x54EFFF + 2, NewModelCollections); //+
        strcpy(HeadLodFormat1, "m46__%d.o");
        strcpy(HeadLodFormat2, "m47__%d.o");
    }
    // textures in fsh (texture collection)
    patch::RedirectCall(0x556AE5, OnLoadTexturesToTexCollection); //+
    patch::RedirectCall(0x556B30, OnLoadTexturesToTexCollection); //+
    patch::SetPointer(0x556A8F + 2, NewTextureCollections); //+
    patch::SetUInt(0x556A89 + 2, 0x10C + (MAX_TEXTURES_IN_FSH - 64) * 4); //+
    patch::SetUInt(0x556A96 + 1, 0x10C + (MAX_TEXTURES_IN_FSH - 64) * 4); //+
    patch::SetUInt(0x556AB3 + 2, MAX_TEXTURES_IN_FSH * 4 + 4); //+
    patch::SetUInt(0x556AD7 + 2, MAX_TEXTURES_IN_FSH * 4); //+
    patch::SetUInt(0x556B22 + 1, MAX_TEXTURES_IN_FSH * 4); //+
    patch::SetUInt(0x54CA88 + 2, MAX_TEXTURES_IN_FSH * 4); //+
    patch::SetUInt(0x54CAEB + 2, MAX_TEXTURES_IN_FSH * 4); //+
    patch::SetUInt(0x555934 + 2, MAX_TEXTURES_IN_FSH * 4); //+
    patch::SetUInt(0x4E12B9 + 2, MAX_TEXTURES_IN_FSH * 4); //+
    patch::SetUInt(0x4E12CC + 2, MAX_TEXTURES_IN_FSH * 4); //+
    patch::SetUInt(0x4E12E8 + 2, MAX_TEXTURES_IN_FSH * 4); //+
    patch::SetUInt(0x555924 + 2, MAX_TEXTURES_IN_FSH * 4); //+
    patch::SetUInt(0x555970 + 2, MAX_TEXTURES_IN_FSH * 4); //+
    patch::SetUInt(0x555B6D + 2, MAX_TEXTURES_IN_FSH * 4); //+
    patch::SetUInt(0x555B76 + 2, MAX_TEXTURES_IN_FSH * 4); //+
    // maybe more patches are required?
    //
  
    // textures in scene
    patch::SetPointer(0x555644 + 1, NewTexInfos); //+
    patch::SetUInt(0x5556B7 + 2, MAX_TEX_INFOS); //+
    patch::SetPointer(0x5556E3 + 1, NewTexInfos + 0x119); //+
    patch::SetPointer(0x5556F3 + 1, NewTexInfos + 0x119 + MAX_TEX_INFOS * 0x11C); //+
    patch::SetPointer(0x555705 + 2, NewTexInfos); //+
    patch::SetPointer(0x55571F + 2, NewTexInfos + 0x119); //+
    patch::SetPointer(0x555725 + 2, NewTexInfos + 0x110); //+
    patch::SetPointer(0x55572B + 2, NewTexInfos + 0x118); //+
    patch::SetPointer(0x555735 + 2, NewTexInfos + 0x4); //+
    patch::SetPointer(0x55574C + 2, NewTexInfos + 0x114); //+
    patch::SetPointer(0x555D13 + 1, NewTexInfos + 0x10C); //+
    patch::SetPointer(0x555D41 + 2, NewTexInfos + 0x10C + MAX_TEX_INFOS * 0x11C); //+
    patch::SetPointer(0x555D50 + 1, NewTexInfos); //+
    patch::SetUInt(0x555D4B + 1, MAX_TEX_INFOS * 0x11C / 4); //+
    patch::SetPointer(0x5560B9 + 1, NewTexInfos + 0x118); //+
    patch::SetPointer(0x556178 + 1, NewTexInfos + 0x118 + MAX_TEX_INFOS * 0x11C); //+
    patch::SetPointer(0x5561D8 + 1, NewTexInfos); //+
    patch::SetPointer(0x55629B + 1, NewTexInfos + MAX_TEX_INFOS * 0x11C); //+
  
    patch::SetPointer(0x556A31 + 3, NewTexDynamicDataPtrs); //+
    patch::SetPointer(0x556A46 + 3, NewTexDynamicDataPtrs); //+
    patch::SetPointer(0x5558B5 + 3, NewTexDynamicDataPtrs); //+
  
    patch::RedirectJump(0x5563D4, OnCompareTexTag1); //+
    patch::RedirectJump(0x556629, OnCompareTexTag2); //+

    patch::SetUInt(0x6CB3F5 + 1, 32); //+
    patch::SetUChar(0x4EC991 + 1, 64); //+
    patch::SetUInt(0x4EC993 + 1, 256); //+
    patch::SetUChar(0x4EE730 + 1, 50); //+
    patch::SetUChar(0x4EE74E + 1, 50);
    patch::RedirectCall(0x4ECA06, WC06NumberCB1<128>); //+
    patch::RedirectCall(0x4ECA94, WC06NumberCB1<64>); //+
    patch::RedirectCall(0x4ECA28, WC06NumberCB2<128>); //+
    patch::RedirectCall(0x4ECAC2, WC06NumberCB2<64>); //+
    patch::SetUInt(0x4F3DF5 + 1, 32 * 2); //+
    patch::SetUInt(0x4F3E0A + 1, 64 * 2); //+
    patch::SetUInt(0x4F3E5F + 4, 16 * 2); //+
    patch::RedirectCall(0x4F3D46, MyGetHotspotData); //+
    patch::SetUInt(0x82B788, settings().SGR_TEXTURE_POOL_SIZE_FE); //+
    patch::SetUInt(0x82B78C, settings().SGR_TEXTURE_POOL_SIZE_BE); //+
    patch::SetUInt(0x565A1A + 6, settings().REAL_MEMORY_HEAP_SIZE); /* * 2 */ //+
    patch::Nop(0x565A45, 6); //+
    patch::Nop(0x565A2E, 10); //+
    patch::SetUInt(0x565A64 + 1, settings().TEXTURE_MEMORY_SIZE); //+
    patch::SetUInt(0x565A38 + 1, settings().TEXTURE_MEMORY_SIZE); //+
}
