#include "plugin.h"
#include "MyFilesystem.h"
#include "CustomCallbacks.h"
#include "MySettings.h"

using namespace plugin;
using namespace std;
using namespace MyFilesystem;

namespace fifa10 {
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
    int PLAYER_BODY_DRAWSTATUS[NUM_PLAYERS];
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
            mov edx, 0x63F1A7 // done
            jmp edx
        }
    }

    void __declspec(naked) OnCompareTexTag2() {
        __asm {
            mov NewUsedTags[edi * TYPE int], esi // done
            mov edx, 0x63F40C // done
            jmp edx
        }
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
        currentDataSize = currentSection - currentData + CallAndReturn<unsigned int, 0x84CEBF>(currentSection); // GetFshSectionSize() // done
        if (currentDataSize < newDataSize) { // if new data doesn't fit into previously allocated memory
            // temporary remove tex info and clear its memory
            Call<0x63EEA0>(texInfo); // RemoveTexInfo() // done
            // allocate memory for new texture
            void *newMem = CallAndReturn<void *, 0x63DD70>(newData); // TexPoolAllocate() // done
            // re-activate tex info and store new memory
            *raw_ptr<unsigned char>(texInfo, 281) = 1; // texInfo.active = true
            *raw_ptr<void *>(texInfo, 0) = newMem; // texInfo.data = newMem
        }
        else // in other case just copy it
            Call<0x840075>(currentData, newData, newDataSize); // MemCpy() // done
    }

    void *OnNewModelUserData(bool isOrd, unsigned char zero, void *sceneEntry, void *block, unsigned int fileSize, const char *fileName) {
        void *oldData = *raw_ptr<void *>(block, 0x50); // block.data
        void *modelCollection = nullptr;
        void *mc = (useCustomHeadlods || useCustomBodies) ? NewModelCollections : (void *)0x13070E8; // done
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
                Call<0x83E896>(oldData); // delete() // done
                void *newData = CallAndReturn<void *, 0x5D1260>(gMemName, fileName, fileSize, 0x400, 16, 0); // FifaMemAllocate() // done
                *raw_ptr<void *>(block, 0x50) = newData; // block.data = newData
                *raw_ptr<void *>(modelCollection, 0x8) = newData; // modelCollection.data = newData
                *raw_ptr<unsigned int>(modelCollection, 0x10) = newFileSize; // modelCollection.size = newFileSize
                fileSize = newFileSize;
            }
        }
        return CallAndReturn<void *, 0x640960>(isOrd, zero, sceneEntry, block, fileSize, fileName); // NewModelUserData() // done
    }

    void *OnResolveScene(void *scene, void *attributeCallback) {
        void *result = CallAndReturn<void *, 0x641B80>(scene, attributeCallback); // done
        void *sceneEntry = *raw_ptr<void *>(scene, 0x44);
        unsigned int numSceneEntries = *raw_ptr<unsigned int>(scene, 0x40);
        for (unsigned int i = 0; i < numSceneEntries; i++) {
            char const *sceneEntryName = *raw_ptr<char const *>(sceneEntry, 0x64);
            if (sceneEntryName) {
                string sceneEntryNameStr = sceneEntryName;
                int playerIndex = -1;
                if (StartsWith(sceneEntryNameStr, "Player(")) {
                    auto spacePos = sceneEntryNameStr.find(' ', 7);
                    if (spacePos != string::npos && spacePos != 7) {
                        try {
                            playerIndex = stoi(sceneEntryNameStr.substr(7, spacePos - 7));
                        }
                        catch (...) {}
                    }
                }
                else if (StartsWith(sceneEntryNameStr, "PlayerFE("))
                    playerIndex = 0;
                if (playerIndex >= 0 && playerIndex < NUM_PLAYERS) {
                    void *modelBlock = *raw_ptr<void *>(sceneEntry, 0x44);
                    unsigned int numModelBlocks = *raw_ptr<unsigned int>(sceneEntry, 0x40);
                    for (unsigned int m = 0; m < numModelBlocks; m++) {
                        char const *resNameFormat = *raw_ptr<char const *>(modelBlock, 0x4);
                        if (resNameFormat) {
                            string resNameFormatStr = resNameFormat;
                            if (resNameFormatStr == "m200__.o"
                                || resNameFormatStr == "m232__.o"
                                || resNameFormatStr == "m242__.o"
                                || resNameFormatStr == "m243__.o"
                                || resNameFormatStr == "m335__.o"
                                || resNameFormatStr == "m20__%d.o")
                            {
                                *raw_ptr<int*>(modelBlock, 0x44) = &PLAYER_BODY_DRAWSTATUS[playerIndex];
                            }
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
        return CallAndReturn<int, 0x8E5A43>(dst, format, arg); // sprintf // done
    }

    void OnSetupPlayerModel(void *desc) {
        bool hashairlodid = false;
        int hairlodid = *raw_ptr<int>(desc, 0x1024);
        if (hairlodid > 0)
            hashairlodid = true;
        else if (hairlodid < 0) {
            int hairtypeid = *raw_ptr<unsigned int>(desc, 0x1028);
            if (hairtypeid > 0)
                hairlodid = -hairtypeid;
        }
        bool hairlodset = false;
        if (hashairlodid) {
            int sizeHairLod = CallMethodAndReturn<int, 0x60D450>(*(void **)0x1306AF8, Format("m728__%d.o", hairlodid).c_str()); // done
            if (sizeHairLod > 0)
                *raw_ptr<int>(desc, 0x1084) = hairlodid;
        }
        int playerIndex = ((int)desc - 0x12DE110) / 0x1220; // done
        if (playerIndex >= 0 && playerIndex < NUM_PLAYERS) {
            PLAYER_BODY_DRAWSTATUS[playerIndex] = 1;
            unsigned int bodyid = *raw_ptr<unsigned int>(desc, 0x100C);
            if (bodyid > 0) {
                int sizeBody = CallMethodAndReturn<int, 0x60D450>(*(void**)0x1306AF8, Format("custom_body_%d", bodyid).c_str());
                if (sizeBody > 0)
                    PLAYER_BODY_DRAWSTATUS[playerIndex] = 0;
            }
            if (useCustomHeadlods) {
                PLAYER_HEADLOD1_ID[playerIndex] = 0;
                PLAYER_HEADLOD2_ID[playerIndex] = 0;
                bool hasheadlodid = false;
                int headlodid = *raw_ptr<int>(desc, 0x1024);
                if (headlodid > 0)
                    hasheadlodid = true;
                //else if (headlodid < 0) {
                //    int hairtypeid = *raw_ptr<int>(desc, 0x1028);
                //    if (hairtypeid >= 0) {
                //        headlodid = -hairtypeid;
                //        hasheadlodid = true;
                //    }
                //}
                if (hasheadlodid) {
                    int sizeHeadLod1 = CallMethodAndReturn<int, 0x60D450>(*(void **)0x1306AF8, Format(HeadLodFormat1, headlodid).c_str()); // done
                    if (sizeHeadLod1 > 0)
                        PLAYER_HEADLOD1_ID[playerIndex] = headlodid;
                    int sizeHeadLod2 = CallMethodAndReturn<int, 0x60D450>(*(void **)0x1306AF8, Format(HeadLodFormat2, headlodid).c_str()); // done
                    if (sizeHeadLod2 > 0)
                        PLAYER_HEADLOD2_ID[playerIndex] = headlodid;
                }
            }
            if (useCustomBodies) {
                PLAYER_BODY6_ID[playerIndex] = 0;
                PLAYER_BODY7_ID[playerIndex] = 0;
                PLAYER_BODY8_ID[playerIndex] = 0;
                PLAYER_BODY9_ID[playerIndex] = 0;
                int playerid = *raw_ptr<int>(desc, 0x100C);
                if (playerid > 0) {
                    int sizeBody6 = CallMethodAndReturn<int, 0x60D450>(*(void **)0x1306AF8, Format(BodyFormat6, playerid).c_str()); // done
                    if (sizeBody6 > 0)
                        PLAYER_BODY6_ID[playerIndex] = playerid;
                    int sizeBody7 = CallMethodAndReturn<int, 0x60D450>(*(void **)0x1306AF8, Format(BodyFormat7, playerid).c_str()); // done
                    if (sizeBody7 > 0)
                        PLAYER_BODY7_ID[playerIndex] = playerid;
                    int sizeBody8 = CallMethodAndReturn<int, 0x60D450>(*(void **)0x1306AF8, Format(BodyFormat8, playerid).c_str()); // done
                    if (sizeBody8 > 0)
                        PLAYER_BODY8_ID[playerIndex] = playerid;
                    int sizeBody9 = CallMethodAndReturn<int, 0x60D450>(*(void **)0x1306AF8, Format(BodyFormat9, playerid).c_str()); // done
                    if (sizeBody9 > 0)
                        PLAYER_BODY9_ID[playerIndex] = playerid;
                }
            }
        }
    }

    void OnSetupPlayerModelBE(void *desc) {
        OnSetupPlayerModel(desc);
        Call<0x5BF110>(desc); // done
    }

    void OnSetupPlayerModelFE(int a, int playerid) {
        Call<0x59F260>(a, playerid); // done
        OnSetupPlayerModel((void *)0x12DE110); // done
    }

    void OnSetupPlayerModelFE_CreatePlayer(int a) {
        PLAYER_BODY_DRAWSTATUS[0] = 1;
        Call<0x59EFE0>(a);
    }

    void OnLoadTexturesToTexCollection(const char *poolName, int numFshs, void *fshCollection, char bReload, unsigned int maxTextures, unsigned int *outNumTextures, void *texCollection, void *sceneGroup) {
        Call<0x63F0E0>(poolName, numFshs, fshCollection, bReload, MAX_TEXTURES_IN_FSH, outNumTextures, texCollection, sceneGroup); // done
    }

    void *gCurrentPlayerDesc = nullptr;

    bool TextureExists(string const &filename) {
        return exists("data\\assets\\" + filename);
        //return CallMethodAndReturn<int, 0x60D450>(*(void **)0x1306AF8, filename.c_str()) > 0; // done
    }

    int OnPlayerDesc1(void *a, void *desc) {
        gCurrentPlayerDesc = desc;
        int result = CallAndReturn<int, 0x5B83F0>(a, desc);
        gCurrentPlayerDesc = nullptr;
        return result;
    }

    int OnPlayerDesc2() {
        gCurrentPlayerDesc = (void *)0x12DE110;
        int result = CallAndReturn<int, 0x14656F0>();
        gCurrentPlayerDesc = nullptr;
        return result;
    }

    void OnSetupGenericPlayerSpecialTextures(void *a, int *hairId, int hairColorId) {
        gCurrentPlayerDesc = (unsigned char *)hairId - 0x1034;
        Call<0x5B7230>(a, *hairId, hairColorId);
        gCurrentPlayerDesc = nullptr;
    }

    void OnSetupCustomPlayerSpecialTextures(void *a, int *headId) {
        gCurrentPlayerDesc = (unsigned char *)headId - 0x1024;
        Call<0x5B6F60>(a, *headId);
        gCurrentPlayerDesc = nullptr;
    }

    int OnGetPlayerEffTexture_BE(char const *name) {
        if (gCurrentPlayerDesc) {
            int headId = *raw_ptr<int>(gCurrentPlayerDesc, 0x1024);
            auto customEffTexPath = "eff_" + Format("%d", headId) + ".dds";
            if (TextureExists(customEffTexPath))
                return CallAndReturn<int, 0x6376B0>(customEffTexPath.c_str());
            else {
                customEffTexPath = "eff_" + Format("%d", headId) + ".tga";
                if (TextureExists(customEffTexPath))
                    return CallAndReturn<int, 0x6376B0>(customEffTexPath.c_str());
            }
        }
        return CallAndReturn<int, 0x6376B0>(name);
    }

    void OnGetPlayerEffTexture_FE(char const *name, int a, unsigned char b) {
        int headid = *(int *)0x12DF134;
        auto customEffTexPath = "eff_" + Format("%d", headid) + ".dds";
        if (TextureExists(customEffTexPath)) {
            Call<0x637760>(customEffTexPath.c_str(), a, b);
            return;
        }
        else {
            customEffTexPath = "eff_" + Format("%d", headid) + ".tga";
            if (TextureExists(customEffTexPath)) {
                Call<0x637760>(customEffTexPath.c_str(), a, b);
                return;
            }
        }
        Call<0x637760>(name, a, b);
    }

    void OnPreLoadFifaPlayerTex(char const *name, int a, unsigned char b) {
        bool loadDefaultEff = false;
        for (int i = 0; i < 26; i++) {
            void *playerDesc = (void *)(0x12DE110 + i * 0x1220);
            int headId = *raw_ptr<int>(playerDesc, 0x1024);
            auto customEffTexPath = "eff_" + Format("%d", headId) + ".dds";
            bool customEff = false;
            if (TextureExists(customEffTexPath)) {
                Call<0x637760>(customEffTexPath.c_str(), a, b);
                customEff = true;
            }
            else {
                customEffTexPath = "eff_" + Format("%d", headId) + ".tga";
                if (TextureExists(customEffTexPath)) {
                    Call<0x637760>(customEffTexPath.c_str(), a, b);
                    customEff = true;
                }
            }
            if (!customEff)
                loadDefaultEff = true;
        }
        if (loadDefaultEff)
            Call<0x637760>(name, a, b);
    }

    int OnGetPlayerSpecialTexture_6376B0(char const *name) {
        string strname = name;
        strname = strname.substr(0, strname.size() - 3) + "dds";
        if (TextureExists(strname))
            return CallAndReturn<int, 0x6376B0>(strname.c_str());
        return CallAndReturn<int, 0x6376B0>(name);
    }

    void OnGetPlayerSpecialTexture_637760(char const *name, int a, unsigned char b) {
        string strname = name;
        strname = strname.substr(0, strname.size() - 3) + "dds";
        if (TextureExists(strname)) {
            Call<0x637760>(strname.c_str(), a, b);
            return;
        }
        Call<0x637760>(name, a, b);
    }

    void OnRenderRenderSlots(void *s) {
        for (unsigned int i = 0; i < 8; i++) {
            CallMethod<0x7EBB90>(nullptr, i, 1);
            CallMethod<0x7EBBE0>(nullptr, i, 1);
        }
        Call<0x641AA0>(s);
    }

    void OnPitchAftRender(void *a) {
        Call<0x5A09A0>(a);
        for (unsigned int i = 0; i < 8; i++) {
            CallMethod<0x7EBB90>(nullptr, i, 1);
            CallMethod<0x7EBBE0>(nullptr, i, 1);
        }
    }
}

void Install_FIFA10() {
    using namespace fifa10;
    patch::SetUInt(0x60F49F + 1, settings().FAT_MAX_ENTRIES); // fat fix (50000 entries) // done
    patch::RedirectCall(0x63F63E, OnTextureCopyData); // done
    patch::SetUShort(0x63F639, 0xD68B); // mov edx, [esi] => mov edx, esi // done
    patch::RedirectCall(0x6433F4, OnNewModelUserData); // done
    useCustomHeadlods = true;
    useCustomBodies = false;
    if (useCustomHeadlods || useCustomBodies) {
        patch::RedirectCall(0x597C64, OnResolveScene); // done
        patch::RedirectCall(0x5A02E5, OnResolveScene); // done
        patch::RedirectCall(0x6414FD, OnFormatModelName1Arg); // done
        patch::RedirectCall(0x5CDF66, OnSetupPlayerModelBE); // done
        patch::RedirectCall(0x5CE8DA, OnSetupPlayerModelBE); // done
        patch::RedirectCall(0x5A04F6, OnSetupPlayerModelFE); // done
        patch::RedirectCall(0x5A0508, OnSetupPlayerModelFE_CreatePlayer);
    }
    // player id 200000
    unsigned int NEW_GEN_PLAYER_ID = 499'000;
    patch::SetUInt(0x459D18 + 1, NEW_GEN_PLAYER_ID + 999); // done
    patch::SetUInt(0x459D36 + 1, NEW_GEN_PLAYER_ID); // done
    patch::SetUInt(0x46D085 + 1, NEW_GEN_PLAYER_ID); // new
    patch::SetUInt(0x4768DE + 1, NEW_GEN_PLAYER_ID); // done
    patch::SetUInt(0x4768ED + 1, NEW_GEN_PLAYER_ID + 486); // done
    patch::SetUInt(0x4768F2 + 1, NEW_GEN_PLAYER_ID + 1); // done
    patch::SetUInt(0x476961 + 2, NEW_GEN_PLAYER_ID); // done
    patch::SetUInt(0x476B3F + 2, NEW_GEN_PLAYER_ID + 484); // done
    patch::SetUInt(0x49527C + 2, NEW_GEN_PLAYER_ID); // not sure
    patch::SetUInt(0x495292 + 2, NEW_GEN_PLAYER_ID + 999); // not sure
    patch::SetUInt(0x49A5A5 + 1, NEW_GEN_PLAYER_ID); // done
    patch::SetUInt(0x49A894 + 1, NEW_GEN_PLAYER_ID); // done
    patch::SetUInt(0x4B5416 + 1, NEW_GEN_PLAYER_ID + 16); // done
    patch::SetUInt(0x4B5434 + 1, NEW_GEN_PLAYER_ID); // done
    patch::SetUInt(0x543562 + 2, NEW_GEN_PLAYER_ID); // done
    patch::SetUInt(0x55074B + 3, NEW_GEN_PLAYER_ID); // new
    patch::SetUInt(0x5A0206 + 2, NEW_GEN_PLAYER_ID); // new
    patch::SetUInt(0x144045D + 2, NEW_GEN_PLAYER_ID); // new
    patch::SetUInt(0x1452825 + 6, NEW_GEN_PLAYER_ID); // not sure
    
    if (useCustomHeadlods || useCustomBodies) {
        patch::SetPointer(0x62D272 + 1, NewModelCollections + 8); // done
        patch::SetPointer(0x62D282 + 1, NewModelCollections + std::size(NewModelCollections) + 8); // done
        patch::SetPointer(0x62D2A2 + 1, NewModelCollections); // done
        patch::SetPointer(0x62E164 + 1, NewModelCollections + 8); // done
        patch::SetPointer(0x62E1D7 + 2, NewModelCollections); // done
        patch::SetPointer(0x62F422 + 1, NewModelCollections + 8); // done
        patch::SetPointer(0x62F466 + 1, NewModelCollections); // done
        patch::SetPointer(0x62FE94 + 2, NewModelCollections); // done
        patch::SetPointer(0x62FFA4 + 2, NewModelCollections); // done
        patch::SetPointer(0x62FFAC + 2, NewModelCollections); // done
        patch::SetPointer(0x630A77 + 1, NewModelCollections + 8); // done
        patch::SetPointer(0x630ACF + 2, NewModelCollections); // done
    }
    // textures in fsh (texture collection)
    patch::RedirectCall(0x63F8E5, OnLoadTexturesToTexCollection); // done
    patch::RedirectCall(0x63F935, OnLoadTexturesToTexCollection); // done
    patch::SetPointer(0x63F88F + 2, NewTextureCollections); // done
    patch::SetUInt(0x63F889 + 2, 0x10C + (MAX_TEXTURES_IN_FSH - 64) * 4); // done
    patch::SetUInt(0x63F896 + 1, 0x10C + (MAX_TEXTURES_IN_FSH - 64) * 4); // done
    patch::SetUInt(0x63F8B3 + 2, MAX_TEXTURES_IN_FSH * 4 + 4); // done
    patch::SetUInt(0x63F8D7 + 2, MAX_TEXTURES_IN_FSH * 4); // done
    patch::SetUInt(0x63F927 + 1, MAX_TEXTURES_IN_FSH * 4); // done
    patch::SetUInt(0x62E028 + 2, MAX_TEXTURES_IN_FSH * 4); // done
    patch::SetUInt(0x62E08B + 2, MAX_TEXTURES_IN_FSH * 4); // done
    patch::SetUInt(0x63E364 + 2, MAX_TEXTURES_IN_FSH * 4); // done
    patch::SetUInt(0x63E354 + 2, MAX_TEXTURES_IN_FSH * 4); // done
    patch::SetUInt(0x63E3A0 + 2, MAX_TEXTURES_IN_FSH * 4); // done
    patch::SetUInt(0x63E59D + 2, MAX_TEXTURES_IN_FSH * 4); // done
    patch::SetUInt(0x63E5A6 + 2, MAX_TEXTURES_IN_FSH * 4); // done
    // maybe more patches are required?
    //
    
    // textures in scene
    patch::SetPointer(0x63E054 + 1, NewTexInfos); // done
    patch::SetUInt(0x63E0C7 + 2, MAX_TEX_INFOS); // done
    patch::SetPointer(0x63E0F3 + 1, NewTexInfos + 0x119); // done
    patch::SetPointer(0x63E103 + 1, NewTexInfos + 0x119 + MAX_TEX_INFOS * 0x11C); // done
    patch::SetPointer(0x63E115 + 2, NewTexInfos); // done
    patch::SetPointer(0x63E12F + 2, NewTexInfos + 0x119); // done
    patch::SetPointer(0x63E135 + 2, NewTexInfos + 0x110); // done
    patch::SetPointer(0x63E13B + 2, NewTexInfos + 0x118); // done
    patch::SetPointer(0x63E145 + 2, NewTexInfos + 0x4); // done
    patch::SetPointer(0x63E15C + 2, NewTexInfos + 0x114); // done
    patch::SetPointer(0x63EC23 + 1, NewTexInfos + 0x10C); // done
    patch::SetPointer(0x63EC51 + 2, NewTexInfos + 0x10C + MAX_TEX_INFOS * 0x11C); // done
    patch::SetPointer(0x63EC60 + 1, NewTexInfos); // done
    patch::SetUInt(0x63EC5B + 1, MAX_TEX_INFOS * 0x11C / 4); // done
    patch::SetPointer(0x63EEC9 + 1, NewTexInfos + 0x118); // done
    patch::SetPointer(0x63EF88 + 1, NewTexInfos + 0x118 + MAX_TEX_INFOS * 0x11C); // done
    patch::SetPointer(0x63EFE7 + 1, NewTexInfos); // done
    patch::SetPointer(0x63F0AB + 1, NewTexInfos + MAX_TEX_INFOS * 0x11C); // done
    
    patch::SetPointer(0x63F810 + 3, NewTexDynamicDataPtrs); // done
    patch::SetPointer(0x63F825 + 3, NewTexDynamicDataPtrs); // done
    patch::SetPointer(0x63E2E5 + 3, NewTexDynamicDataPtrs); // done
    
    patch::RedirectJump(0x63F1A0, OnCompareTexTag1); // done
    patch::RedirectJump(0x63F405, OnCompareTexTag2); // done
    
    patch::SetUInt(0x7ECFB5 + 1, 32); // done
    patch::SetUInt(0x9CEEF0, settings().SGR_TEXTURE_POOL_SIZE_FE); // done
    patch::SetUInt(0x9CEEF4, settings().SGR_TEXTURE_POOL_SIZE_BE); // done
    patch::SetUInt(0x654D3F + 6, settings().REAL_MEMORY_HEAP_SIZE); // done
    patch::SetUInt(0x654D3A + 1, settings().TEXTURE_MEMORY_SIZE); // done

    // custom eff

    patch::RedirectCall(0x14653E4, OnGetPlayerEffTexture_BE);
    patch::RedirectCall(0x146528F, OnGetPlayerEffTexture_BE);
    patch::RedirectCall(0x14635A9, OnGetPlayerEffTexture_FE);
    patch::RedirectCall(0x1464F3C, OnPreLoadFifaPlayerTex);
    patch::RedirectCall(0x1465391, OnGetPlayerSpecialTexture_6376B0);
    patch::RedirectCall(0x14653BB, OnGetPlayerSpecialTexture_6376B0);
    patch::RedirectCall(0x146523C, OnGetPlayerSpecialTexture_6376B0);
    patch::RedirectCall(0x1465266, OnGetPlayerSpecialTexture_6376B0);
    patch::RedirectCall(0x5B672F, OnGetPlayerSpecialTexture_637760);
    patch::RedirectCall(0x5B6753, OnGetPlayerSpecialTexture_637760);
    patch::RedirectCall(0x5B66DF, OnGetPlayerSpecialTexture_637760);
    patch::RedirectCall(0x5B66FD, OnGetPlayerSpecialTexture_637760);
    patch::RedirectCall(0x1463566, OnGetPlayerSpecialTexture_637760);
    patch::RedirectCall(0x146358A, OnGetPlayerSpecialTexture_637760);
    patch::RedirectCall(0x1463511, OnGetPlayerSpecialTexture_637760);
    patch::RedirectCall(0x1463534, OnGetPlayerSpecialTexture_637760);
    patch::RedirectCall(0x1464DCF, OnGetPlayerSpecialTexture_637760);
    patch::RedirectCall(0x1464DF0, OnGetPlayerSpecialTexture_637760);
    patch::RedirectCall(0x1464D82, OnGetPlayerSpecialTexture_637760);
    patch::RedirectCall(0x1464DA2, OnGetPlayerSpecialTexture_637760);
    patch::SetPointer(0x9BD658, OnPlayerDesc1);
    patch::SetPointer(0x9C8BF0, OnPlayerDesc1);
    patch::RedirectJump(0x5B8380, OnPlayerDesc2);
    patch::RedirectCall(0x14656D0, OnSetupGenericPlayerSpecialTextures);
    patch::RedirectCall(0x14656BD, OnSetupCustomPlayerSpecialTextures);
    patch::SetUChar(0x14656CA, 0x8D); // mov ecx, [esi+10h] > lea ecx, [esi+10h]
    patch::SetUChar(0x14656B9 + 1, 0xCE); // mov ecx, [esi] > mov ecx, esi

    // fix player hair
    patch::RedirectCall(0x5980AC, OnRenderRenderSlots);
    patch::RedirectCall(0x59E849, OnRenderRenderSlots);
    patch::RedirectCall(0x59EE02, OnRenderRenderSlots);
    patch::RedirectCall(0x5C15E4, OnPitchAftRender);

    // adboards 16
    //patch::SetUInt(0x146332F + 4, 16); // done

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
