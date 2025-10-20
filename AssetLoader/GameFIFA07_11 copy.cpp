#include "plugin-std.h"
#include <filesystem>
#include "CustomCallbacks.h"

using namespace plugin;
using namespace std;
using namespace std::filesystem;

namespace fifa07_11 {
    const unsigned int MAX_TEX_COLLECTIONS = 200; // default 200
    const unsigned int MAX_TEXTURES_IN_FSH = 1024; // default 64
    const unsigned int MAX_TEX_INFOS = 1280; // default 320
    const unsigned int MAX_TEX_DYNAMIC_DATA_PTRS = 1024; // default 256
    bool useCustomHeadlods = false;
    bool useCustomBodies = false;
    const unsigned int NUM_PLAYERS = 25; // 11 * 2 players + 1 referee + 2 linesmen
    struct ResFormatArg { int *a; int b; };
    int PLAYER_HEADLOD1_ID[NUM_PLAYERS];
    int PLAYER_HEADLOD2_ID[NUM_PLAYERS];
    ResFormatArg PLAYER_HEADLOD1_ACCESSORS[NUM_PLAYERS];
    ResFormatArg PLAYER_HEADLOD2_ACCESSORS[NUM_PLAYERS];
    char HeadLodFormat1[64];
    char HeadLodFormat2[64];
    int PLAYER_BODY6_ID[NUM_PLAYERS];
    int PLAYER_BODY7_ID[NUM_PLAYERS];
    int PLAYER_BODY8_ID[NUM_PLAYERS];
    int PLAYER_BODY9_ID[NUM_PLAYERS];
    ResFormatArg PLAYER_BODY6_ACCESSORS[NUM_PLAYERS];
    ResFormatArg PLAYER_BODY7_ACCESSORS[NUM_PLAYERS];
    ResFormatArg PLAYER_BODY8_ACCESSORS[NUM_PLAYERS];
    ResFormatArg PLAYER_BODY9_ACCESSORS[NUM_PLAYERS];
    char BodyFormat6[64];
    char BodyFormat7[64];
    char BodyFormat8[64];
    char BodyFormat9[64];
    unsigned char NewModelCollections[0x120 * 1024];
    unsigned char NewTextureCollections[(0x10C + (MAX_TEXTURES_IN_FSH - 64) * 4) * MAX_TEX_COLLECTIONS];
    unsigned char NewTexInfos[0x11C * MAX_TEX_INFOS];
    unsigned char NewTexDynamicDataPtrs[4 * MAX_TEX_DYNAMIC_DATA_PTRS];
    unsigned int NewUsedTags[MAX_TEXTURES_IN_FSH];

    void __declspec(naked) OnCompareTexTag1() {
        __asm {
            cmp NewUsedTags[eax * TYPE int], ecx
            mov edx, 0x5DEADB
            jmp edx
        }
    }

    void __declspec(naked) OnCompareTexTag2() {
        __asm {
            mov NewUsedTags[esi * TYPE int], edi
            mov edx, 0x5DED30
            jmp edx
        }
    }
    template<int S>
    void *Fifa07NumberCB1(int a1, int a2, int a3, int a4, int a5, int a6, int a7) {
        return CallAndReturn<void *, 0x5DDE20>(a1, S, S, a4, a5, a6, a7);
    }

    template<int S>
    void *Fifa07NumberCB2(int a1, int a2, int a3, int a4, int a5, int a6) {
        return CallAndReturn<void *, 0x5DF170>(a1, S, S, a4, a5, a6);
    }

    void METHOD MyGetHotspotData(void *t, DUMMY_ARG, int *hs, void *data) {
        CallMethod<0x59F270>(t, hs, data);
        for (int i = 0; i < 12; i++) {
            hs[i + 1] *= 2;
        }
    }

    void OnTextureCopyData(void *texInfo, unsigned char *newData, unsigned int newDataSize) {
        unsigned char *currentData = *raw_ptr<unsigned char *>(texInfo, 0);
        auto currentDataSize = 0;
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
        currentDataSize = currentSection - currentData + CallAndReturn<unsigned int, 0x7A946F>(currentSection); // GetFshSectionSize()
        if (currentDataSize < newDataSize) { // if new data doesn't fit into previously allocated memory
            // temporary remove tex info and clear its memory
            Call<0x5DE7D0>(texInfo); // RemoveTexInfo()
            // allocate memory for new texture
            void *newMem = CallAndReturn<void *, 0x5DD620>(newData); // TexPoolAllocate()
            // re-activate tex info and store new memory
            *raw_ptr<unsigned char>(texInfo, 281) = 1; // texInfo.active = true
            *raw_ptr<void *>(texInfo, 0) = newMem; // texInfo.data = newMem
        }
        else // in other case just copy it
            Call<0x79C672>(currentData, newData, newDataSize); // MemCpy()
    }

    void *OnNewModelUserData(bool isOrd, unsigned char zero, void *sceneEntry, void *block, unsigned int fileSize, const char *fileName) {
        void *oldData = *raw_ptr<void *>(block, 0x50); // block.data
        void *modelCollection = nullptr;
        void *mc = (void *)0xA0C308; // ModelCollections
        for (unsigned int i = 0; i < *(unsigned int *)0xA0C0D8; i++) { // NumModelCollections
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
                Call<0x79B475>(oldData); // delete()
                void *newData = CallAndReturn<void *, 0x5AE760>(gMemName, fileName, fileSize, 0x400, 16, 0); // FifaMemAllocate()
                *raw_ptr<void *>(block, 0x50) = newData; // block.data = newData
                *raw_ptr<void *>(modelCollection, 0x8) = newData; // modelCollection.data = newData
                *raw_ptr<unsigned int>(modelCollection, 0x10) = newFileSize; // modelCollection.size = newFileSize
                fileSize = newFileSize;
            }
        }
        return CallAndReturn<void *, 0x5E02B0>(isOrd, zero, sceneEntry, block, fileSize, fileName); // NewModelUserData()
    }

    void *OnResolveScene(void *scene, void *attributeCallback) {
        void *result = CallAndReturn<void *, 0x5E14D0>(scene, attributeCallback);
        void *sceneEntry = *raw_ptr<void *>(scene, 0x44);
        unsigned int numSceneEntries = *raw_ptr<unsigned int>(scene, 0x40);
        for (unsigned int i = 0; i < numSceneEntries; i++) {
            char const *sceneEntryName = *raw_ptr<char const *>(sceneEntry, 0x64);
            if (sceneEntryName) {
                string sceneEntryNameStr = sceneEntryName;
                if (sceneEntryNameStr.starts_with("Player(")) {
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
                                    if (useCustomHeadlods) {
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
                                    if (useCustomBodies) {
                                        if (resNameFormatStr == "m41__.o") {
                                            *raw_ptr<char const *>(modelBlock, 0x4) = BodyFormat6;
                                            *raw_ptr<unsigned int>(modelBlock, 0x28) = 1;
                                            PLAYER_BODY6_ACCESSORS[playerIndex].a = &PLAYER_BODY6_ID[playerIndex];
                                            PLAYER_BODY6_ACCESSORS[playerIndex].b = 0;
                                            *raw_ptr<void *>(modelBlock, 0x2C) = &PLAYER_BODY6_ACCESSORS[playerIndex];
                                        }
                                        else if (resNameFormatStr == "m51__.o") {
                                            *raw_ptr<char const *>(modelBlock, 0x4) = BodyFormat7;
                                            *raw_ptr<unsigned int>(modelBlock, 0x28) = 1;
                                            PLAYER_BODY7_ACCESSORS[playerIndex].a = &PLAYER_BODY7_ID[playerIndex];
                                            PLAYER_BODY7_ACCESSORS[playerIndex].b = 0;
                                            *raw_ptr<void *>(modelBlock, 0x2C) = &PLAYER_BODY7_ACCESSORS[playerIndex];
                                        }
                                        else if (resNameFormatStr == "m124__.o") {
                                            *raw_ptr<char const *>(modelBlock, 0x4) = BodyFormat8;
                                            *raw_ptr<unsigned int>(modelBlock, 0x28) = 1;
                                            PLAYER_BODY8_ACCESSORS[playerIndex].a = &PLAYER_BODY8_ID[playerIndex];
                                            PLAYER_BODY8_ACCESSORS[playerIndex].b = 0;
                                            *raw_ptr<void *>(modelBlock, 0x2C) = &PLAYER_BODY8_ACCESSORS[playerIndex];
                                        }
                                        else if (resNameFormatStr == "m131__.o") {
                                            *raw_ptr<char const *>(modelBlock, 0x4) = BodyFormat9;
                                            *raw_ptr<unsigned int>(modelBlock, 0x28) = 1;
                                            PLAYER_BODY9_ACCESSORS[playerIndex].a = &PLAYER_BODY9_ID[playerIndex];
                                            PLAYER_BODY9_ACCESSORS[playerIndex].b = 0;
                                            *raw_ptr<void *>(modelBlock, 0x2C) = &PLAYER_BODY9_ACCESSORS[playerIndex];
                                        }
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

    int OnFormatModelName1Arg(char *dst, char const *format, int arg) {
        if (arg == 0) {
            if (useCustomHeadlods) {
                if (!strcmp(format, "m46__%d.o")) {
                    strcpy(dst, "m46__.o");
                    return 1;
                }
                else if (!strcmp(format, "m47__%d.o")) {
                    strcpy(dst, "m47__.o");
                    return 1;
                }
            }
            if (useCustomBodies) {
                if (!strcmp(format, "m41__%d.o")) {
                    strcpy(dst, "m41__.o");
                    return 1;
                }
                else if (!strcmp(format, "m51__%d.o")) {
                    strcpy(dst, "m51__.o");
                    return 1;
                }
                else if (!strcmp(format, "m124__%d.o")) {
                    strcpy(dst, "m124__.o");
                    return 1;
                }
                else if (!strcmp(format, "m131__%d.o")) {
                    strcpy(dst, "m131__.o");
                    return 1;
                }
            }
        }
        return CallAndReturn<int, 0x81E4CD>(dst, format, arg); // sprintf
    }

    void OnSetupPlayerModelBE(void *desc) {
        int playerIndex = ((int)desc - 0x9E8F90) / 0x11D0;
        if (playerIndex >= 0 && playerIndex < NUM_PLAYERS) {
            if (useCustomHeadlods) {
                PLAYER_HEADLOD1_ID[playerIndex] = 0;
                PLAYER_HEADLOD2_ID[playerIndex] = 0;
                bool hasheadlodid = false;
                int headlodid = *raw_ptr<unsigned int>(desc, 0x1008);
                if (headlodid > 0)
                    hasheadlodid = true;
                else if (headlodid < 0) {
                    int hairtypeid = *raw_ptr<unsigned int>(desc, 0x100C);
                    if (hairtypeid >= 0) {
                        headlodid = -hairtypeid;
                        hasheadlodid = true;
                    }
                }
                if (hasheadlodid) {
                    int sizeHeadLod1 = CallMethodAndReturn<int, 0x5AF8F0>(*(void **)0xA0BD20, Format(HeadLodFormat1, headlodid).c_str());
                    if (sizeHeadLod1 > 0)
                        PLAYER_HEADLOD1_ID[playerIndex] = headlodid;
                    int sizeHeadLod2 = CallMethodAndReturn<int, 0x5AF8F0>(*(void **)0xA0BD20, Format(HeadLodFormat2, headlodid).c_str());
                    if (sizeHeadLod2 > 0)
                        PLAYER_HEADLOD2_ID[playerIndex] = headlodid;
                }
            }
            if (useCustomBodies) {
                PLAYER_BODY6_ID[playerIndex] = 0;
                PLAYER_BODY7_ID[playerIndex] = 0;
                PLAYER_BODY8_ID[playerIndex] = 0;
                PLAYER_BODY9_ID[playerIndex] = 0;
                int playerid = *raw_ptr<unsigned int>(desc, 0xFF4);
                if (playerid > 0) {
                    int sizeBody6 = CallMethodAndReturn<int, 0x5AF8F0>(*(void **)0xA0BD20, Format(BodyFormat6, playerid).c_str());
                    if (sizeBody6 > 0)
                        PLAYER_BODY6_ID[playerIndex] = playerid;
                    int sizeBody7 = CallMethodAndReturn<int, 0x5AF8F0>(*(void **)0xA0BD20, Format(BodyFormat7, playerid).c_str());
                    if (sizeBody7 > 0)
                        PLAYER_BODY7_ID[playerIndex] = playerid;
                    int sizeBody8 = CallMethodAndReturn<int, 0x5AF8F0>(*(void **)0xA0BD20, Format(BodyFormat8, playerid).c_str());
                    if (sizeBody8 > 0)
                        PLAYER_BODY8_ID[playerIndex] = playerid;
                    int sizeBody9 = CallMethodAndReturn<int, 0x5AF8F0>(*(void **)0xA0BD20, Format(BodyFormat9, playerid).c_str());
                    if (sizeBody9 > 0)
                        PLAYER_BODY9_ID[playerIndex] = playerid;
                }
            }
        }
        Call<0x59BDC0>(desc);
    }

    void OnSetupPlayerModelFE(int a, int playerid) {
        Call<0x586BD0>(a, playerid);
        OnSetupPlayerModelBE((void *)0x9E8F90);
    }

    void OnLoadTexturesToTexCollection(const char *poolName, int numFshs, void *fshCollection, char bReload, unsigned int maxTextures, unsigned int *outNumTextures, void *texCollection, void *sceneGroup) {
        Call<0x5DEA20>(poolName, numFshs, fshCollection, bReload, MAX_TEXTURES_IN_FSH, outNumTextures, texCollection, sceneGroup);
    }
}

void Install_FIFA07_11() {
    using namespace fifa07_11;
    patch::SetUInt(0x5B19CF + 1, 50'000); // fat fix (50000 entries)
    patch::RedirectCall(0x5DEF5D, OnTextureCopyData);
    patch::SetUShort(0x5DEF58, 0xD68B); // mov edi, [esi] => mov edi, esi
    patch::RedirectCall(0x5E2DA4, OnNewModelUserData);
    useCustomHeadlods = true;
    useCustomBodies = true;
    if (useCustomHeadlods || useCustomBodies) {
        patch::RedirectCall(0x580744, OnResolveScene);
        patch::RedirectCall(0x587BAF, OnResolveScene);
        patch::RedirectCall(0x5E0E4D, OnFormatModelName1Arg);
        patch::RedirectCall(0x5A977E, OnSetupPlayerModelBE);
        patch::RedirectCall(0x5A90D9, OnSetupPlayerModelBE);
        patch::RedirectCall(0x587DC9, OnSetupPlayerModelFE);
    }
    // player id 200000
    unsigned int NEW_GEN_PLAYER_ID = 499'000;
    patch::SetUInt(0x4870F2 + 1, NEW_GEN_PLAYER_ID + 999 /*200'000*/);
    patch::SetUInt(0x487110 + 1, NEW_GEN_PLAYER_ID);
    patch::SetUInt(0x49DC72 + 1, NEW_GEN_PLAYER_ID);
    patch::SetUInt(0x49DC8F + 1, NEW_GEN_PLAYER_ID + 484);
    patch::SetUInt(0x49DC72 + 1, NEW_GEN_PLAYER_ID);
    patch::SetUInt(0x49DDF9 + 2, NEW_GEN_PLAYER_ID - 1);
    patch::SetUInt(0x49DC72 + 1, NEW_GEN_PLAYER_ID);
    patch::SetUInt(0x4C9514 + 1, NEW_GEN_PLAYER_ID);
    patch::SetUInt(0x4C9885 + 1, NEW_GEN_PLAYER_ID);
    patch::SetUInt(0x4F9257 + 2, NEW_GEN_PLAYER_ID);
    patch::SetUInt(0x50A5BE + 1, NEW_GEN_PLAYER_ID + 16);
    patch::SetUInt(0x50A5D9 + 1, NEW_GEN_PLAYER_ID);
    patch::SetUInt(0x587AD0 + 2, NEW_GEN_PLAYER_ID);
    patch::SetUInt(0x49E00A + 2, NEW_GEN_PLAYER_ID + 484);
    if (useCustomBodies) {
        patch::SetPointer(0x5CE632 + 1, NewModelCollections + 8);
        patch::SetPointer(0x5CE662 + 1, NewModelCollections);
        patch::SetPointer(0x5CF4C4 + 1, NewModelCollections + 8);
        patch::SetPointer(0x5CF537 + 2, NewModelCollections);
        patch::SetPointer(0x5D01D2 + 1, NewModelCollections + 8);
        patch::SetPointer(0x5D0216 + 1, NewModelCollections);
        patch::SetPointer(0x5D0C44 + 2, NewModelCollections);
        patch::SetPointer(0x5D0D54 + 2, NewModelCollections);
        patch::SetPointer(0x5D0D5C + 2, NewModelCollections);
        patch::SetPointer(0x5D1827 + 1, NewModelCollections + 8);
        patch::SetPointer(0x5D187F + 2, NewModelCollections);
        patch::SetPointer(0x5CE632 + 1, NewModelCollections + 8);
        patch::SetPointer(0x5CE662 + 1, NewModelCollections);
        patch::SetPointer(0x5CF4C4 + 1, NewModelCollections + 8);
        patch::SetPointer(0x5CF537 + 2, NewModelCollections);
        patch::SetPointer(0x5D01D2 + 1, NewModelCollections + 8);
        patch::SetPointer(0x5D0216 + 1, NewModelCollections);
        patch::SetPointer(0x5D1827 + 1, NewModelCollections + 8);
        patch::SetPointer(0x5D187F + 2, NewModelCollections);
        patch::SetPointer(0x5CE642 + 1, NewModelCollections + std::size(NewModelCollections) + 8);
    }
    // textures in fsh (texture collection)
    patch::RedirectCall(0x5DF205, OnLoadTexturesToTexCollection);
    patch::RedirectCall(0x5DF255, OnLoadTexturesToTexCollection);
    patch::SetPointer(0x5DF1AF + 2, NewTextureCollections);
    patch::SetUInt(0x5DF1A9 + 2, 0x10C + (MAX_TEXTURES_IN_FSH - 64) * 4);
    patch::SetUInt(0x5DF1B6 + 1, 0x10C + (MAX_TEXTURES_IN_FSH - 64) * 4);
    patch::SetUInt(0x5DF1D3 + 2, MAX_TEXTURES_IN_FSH * 4 + 4);
    patch::SetUInt(0x5DF1F7 + 2, MAX_TEXTURES_IN_FSH * 4);
    patch::SetUInt(0x5DF247 + 1, MAX_TEXTURES_IN_FSH * 4);
    patch::SetUInt(0x5CF388 + 2, MAX_TEXTURES_IN_FSH * 4);
    patch::SetUInt(0x5CF3EB + 2, MAX_TEXTURES_IN_FSH * 4);
    patch::SetUInt(0x5DDC34 + 2, MAX_TEXTURES_IN_FSH * 4);

    patch::SetUInt(0x5894E9 + 2, MAX_TEXTURES_IN_FSH * 4);
    patch::SetUInt(0x5894FC + 2, MAX_TEXTURES_IN_FSH * 4);
    patch::SetUInt(0x589518 + 2, MAX_TEXTURES_IN_FSH * 4);
    patch::SetUInt(0x5DDC24 + 2, MAX_TEXTURES_IN_FSH * 4);
    patch::SetUInt(0x5DDC70 + 2, MAX_TEXTURES_IN_FSH * 4);
    patch::SetUInt(0x5DDE6D + 2, MAX_TEXTURES_IN_FSH * 4);
    patch::SetUInt(0x5DDE76 + 2, MAX_TEXTURES_IN_FSH * 4);
    // maybe more patches are required?
    //

    // textures in scene
    patch::SetPointer(0x5DD904 + 1, NewTexInfos);
    patch::SetUInt(0x5DD977 + 2, MAX_TEX_INFOS);
    patch::SetPointer(0x5DD9A3 + 1, NewTexInfos + 0x119);
    patch::SetPointer(0x5DD9B3 + 1, NewTexInfos + 0x119 + MAX_TEX_INFOS * 0x11C);
    patch::SetPointer(0x5DD9C5 + 2, NewTexInfos);
    patch::SetPointer(0x5DD9DF + 2, NewTexInfos + 0x119);
    patch::SetPointer(0x5DD9E5 + 2, NewTexInfos + 0x110);
    patch::SetPointer(0x5DD9EB + 2, NewTexInfos + 0x118);
    patch::SetPointer(0x5DD9F5 + 2, NewTexInfos + 0x4);
    patch::SetPointer(0x5DDA0C + 2, NewTexInfos + 0x114);
    patch::SetPointer(0x5DE543 + 1, NewTexInfos + 0x10C);
    patch::SetPointer(0x5DE571 + 2, NewTexInfos + 0x10C + MAX_TEX_INFOS * 0x11C);
    patch::SetPointer(0x5DE580 + 1, NewTexInfos);
    patch::SetUInt(0x5DE57B + 1, MAX_TEX_INFOS * 0x11C / 4);
    patch::SetPointer(0x5DE7F9 + 1, NewTexInfos + 0x118);
    patch::SetPointer(0x5DE8B8 + 1, NewTexInfos + 0x118 + MAX_TEX_INFOS * 0x11C);
    patch::SetPointer(0x5DE917 + 1, NewTexInfos);
    patch::SetPointer(0x5DE9DB + 1, NewTexInfos + MAX_TEX_INFOS * 0x11C);

    patch::SetPointer(0x5DF121 + 3, NewTexDynamicDataPtrs);
    patch::SetPointer(0x5DF136 + 3, NewTexDynamicDataPtrs);
    patch::SetPointer(0x5DDBB5 + 3, NewTexDynamicDataPtrs);

    patch::RedirectJump(0x5DEAD4, OnCompareTexTag1);
    patch::RedirectJump(0x5DED29, OnCompareTexTag2);

    patch::SetUInt(0x74B095 + 1, 32);
    patch::SetUChar(0x595BE3 + 1, 64);
    patch::SetUInt(0x595BE5 + 1, 256);
    patch::SetUChar(0x597824 + 1, 50);
    patch::RedirectCall(0x595C58, Fifa07NumberCB1<128>);
    patch::RedirectCall(0x595CE6, Fifa07NumberCB1<64>);
    patch::RedirectCall(0x595C7A, Fifa07NumberCB2<128>);
    patch::RedirectCall(0x595D14, Fifa07NumberCB2<64>);
    patch::SetUInt(0x59F3D5 + 1, 32 * 2);
    patch::SetUInt(0x59F3EA + 1, 64 * 2);
    patch::SetUInt(0x59F43F + 4, 16 * 2);
    patch::RedirectCall(0x59F326, MyGetHotspotData);
    patch::SetUInt(0x8C56F0, 10485760);
    patch::SetUInt(0x8C56F4, 62914560 /*62914560*/);
    patch::SetUInt(0x5F113A + 6, 226492416);
    patch::Nop(0x5F1165, 6);
    patch::Nop(0x5F114E, 10);
    patch::SetUInt(0x5F1184 + 1, 75497472);
    patch::SetUInt(0x5F1158 + 1, 75497472);

    // adboards 16
    patch::SetUInt(0x5845AB + 4, 16);

    if (useCustomHeadlods) {
        strcpy(HeadLodFormat1, "m46__%d.o");
        strcpy(HeadLodFormat2, "m47__%d.o");
    }
    if (useCustomBodies) {
        strcpy(BodyFormat6, "m41__%d.o");
        strcpy(BodyFormat7, "m51__%d.o");
        strcpy(BodyFormat8, "m124__%d.o");
        strcpy(BodyFormat9, "m131__%d.o");
    }
}
