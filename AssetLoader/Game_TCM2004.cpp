#include "plugin.h"
#include "MyFilesystem.h"
#include "CustomCallbacks.h"
#include "GfxCoreHook.h"

using namespace plugin;
using namespace std;
using namespace MyFilesystem;

namespace tcm2004 {
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
    unsigned char NewModelCollections[0x128 * 1024];

    template<int S>
    void *Fifa04NumberCB(int a1, int a2, int a3, int a4, int a5) {
        return CallAndReturnDynGlobal<void *>(GfxCoreAddress(0x36A640), a1, S, S, a4, a5); // done
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
        currentDataSize = currentSection - currentData + CallAndReturnDynGlobal<unsigned int>(GfxCoreAddress(0x2E1F70), currentSection); // GetFshSectionSize() // done
        if (currentDataSize < newDataSize) { // if new data doesn't fit into previously allocated memory
            // temporary remove tex info and clear its memory
            CallDynGlobal(GfxCoreAddress(0x36A900), texInfo); // RemoveTexInfo() // done
            // allocate memory for new texture
            void *newMem = CallAndReturnDynGlobal<void *>(GfxCoreAddress(0x36A5A0), newData); // TexPoolAllocate() // done
            // re-activate tex info and store new memory
            *raw_ptr<unsigned char>(texInfo, 277) = 1; // texInfo.active = true
            *raw_ptr<void *>(texInfo, 0) = newMem; // texInfo.data = newMem
        }
        else // in other case just copy it
            CallDynGlobal(GfxCoreAddress(0x2F4660), currentData, newData, newDataSize); // MemCpy() // done
    }

    void *OnNewModelUserData(bool isOrd, void *sceneEntry, void *block, unsigned int fileSize, const char *fileName) {
        void *oldData = *raw_ptr<void *>(block, 0x48); // block.data // done
        void *modelCollection = nullptr;
        void *mc = (useCustomHeadlods || useCustomBodies) ? NewModelCollections : (void *)GfxCoreAddress(0x580718); // ModelCollections // done
        for (unsigned int i = 0; i < *(unsigned int *)GfxCoreAddress(0x580598); i++) { // NumModelCollections // done
            if (*raw_ptr<void *>(mc, 0x8) == oldData) { // modelCollection.data
                modelCollection = mc;
                break;
            }
            mc = raw_ptr<void>(mc, 296); // mc++
        }
        if (modelCollection) {
            unsigned int newFileSize = max(912u, fileSize);
            if (*raw_ptr<unsigned int>(modelCollection, 0x10) < newFileSize) { // modelCollection.size
                CallDynGlobal(GfxCoreAddress(0x2EF2F0), oldData); // delete() // done
                void *newData = CallAndReturnDynGlobal<void *>(GfxCoreAddress(0x272E0), "SGSM", fileName, fileSize, 0x400, 16, 0); // FifaMemAllocate() // done
                *raw_ptr<void *>(block, 0x48) = newData; // block.data = newData // done
                *raw_ptr<void *>(modelCollection, 0x8) = newData; // modelCollection.data = newData
                *raw_ptr<unsigned int>(modelCollection, 0x10) = newFileSize; // modelCollection.size = newFileSize
                fileSize = newFileSize;
            }
        }
        return CallAndReturnDynGlobal<void *>(GfxCoreAddress(0x355730), isOrd, sceneEntry, block, fileSize, fileName); // NewModelUserData() // done
    }

    void *OnResolveScene(void *scene, void *attributeCallback) {
        void *result = CallAndReturnDynGlobal<void *>(GfxCoreAddress(0x356260), scene, attributeCallback); // done
        void *sceneEntry = *raw_ptr<void *>(scene, 0x44);
        unsigned int numSceneEntries = *raw_ptr<unsigned int>(scene, 0x40);
        for (unsigned int i = 0; i < numSceneEntries; i++) {
            char const *sceneEntryName = *raw_ptr<char const *>(sceneEntry, 0x60); // done
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
                                    if (useCustomHeadlods) {
                                        if (resNameFormatStr == "Player__medhead__model1020311275__.o") {
                                            *raw_ptr<char const *>(modelBlock, 0x4) = HeadLodFormat1;
                                            *raw_ptr<unsigned int>(modelBlock, 0x20) = 1;
                                            PLAYER_HEADLOD1_ACCESSORS[playerIndex].a = &PLAYER_HEADLOD1_ID[playerIndex];
                                            PLAYER_HEADLOD1_ACCESSORS[playerIndex].b = 0;
                                            *raw_ptr<void *>(modelBlock, 0x24) = &PLAYER_HEADLOD1_ACCESSORS[playerIndex];
                                        }
                                        else if (resNameFormatStr == "Player__lowhead__model1020311335__.o") {
                                            *raw_ptr<char const *>(modelBlock, 0x4) = HeadLodFormat2;
                                            *raw_ptr<unsigned int>(modelBlock, 0x20) = 1;
                                            PLAYER_HEADLOD2_ACCESSORS[playerIndex].a = &PLAYER_HEADLOD2_ID[playerIndex];
                                            PLAYER_HEADLOD2_ACCESSORS[playerIndex].b = 0;
                                            *raw_ptr<void *>(modelBlock, 0x24) = &PLAYER_HEADLOD2_ACCESSORS[playerIndex];
                                        }
                                    }
                                    if (useCustomBodies) {
                                        if (resNameFormatStr == "Player__Shadow__model1019510585__.o") {
                                            *raw_ptr<char const *>(modelBlock, 0x4) = BodyFormat6;
                                            *raw_ptr<unsigned int>(modelBlock, 0x20) = 1;
                                            PLAYER_BODY6_ACCESSORS[playerIndex].a = &PLAYER_BODY6_ID[playerIndex];
                                            PLAYER_BODY6_ACCESSORS[playerIndex].b = 0;
                                            *raw_ptr<void *>(modelBlock, 0x24) = &PLAYER_BODY6_ACCESSORS[playerIndex];
                                        }
                                        else if (resNameFormatStr == "Player__Shadow__model324920654296875__.o") {
                                            *raw_ptr<char const *>(modelBlock, 0x4) = BodyFormat7;
                                            *raw_ptr<unsigned int>(modelBlock, 0x20) = 1;
                                            PLAYER_BODY7_ACCESSORS[playerIndex].a = &PLAYER_BODY7_ID[playerIndex];
                                            PLAYER_BODY7_ACCESSORS[playerIndex].b = 0;
                                            *raw_ptr<void *>(modelBlock, 0x24) = &PLAYER_BODY7_ACCESSORS[playerIndex];
                                        }
                                        else if (resNameFormatStr == "Player__Shadow__model110107421875__.o") {
                                            *raw_ptr<char const *>(modelBlock, 0x4) = BodyFormat8;
                                            *raw_ptr<unsigned int>(modelBlock, 0x20) = 1;
                                            PLAYER_BODY8_ACCESSORS[playerIndex].a = &PLAYER_BODY8_ID[playerIndex];
                                            PLAYER_BODY8_ACCESSORS[playerIndex].b = 0;
                                            *raw_ptr<void *>(modelBlock, 0x24) = &PLAYER_BODY8_ACCESSORS[playerIndex];
                                        }
                                        else if (resNameFormatStr == "Player__Shadow__model293365478515625__.o") {
                                            *raw_ptr<char const *>(modelBlock, 0x4) = BodyFormat9;
                                            *raw_ptr<unsigned int>(modelBlock, 0x20) = 1;
                                            PLAYER_BODY9_ACCESSORS[playerIndex].a = &PLAYER_BODY9_ID[playerIndex];
                                            PLAYER_BODY9_ACCESSORS[playerIndex].b = 0;
                                            *raw_ptr<void *>(modelBlock, 0x24) = &PLAYER_BODY9_ACCESSORS[playerIndex];
                                        }
                                    }
                                }
                                modelBlock = raw_ptr<void>(modelBlock, 0x50); // done
                            }
                        }
                    }
                }
            }
            sceneEntry = raw_ptr<void>(sceneEntry, 0x68); // done
        }
        return result;
    }

    int OnFormatModelName1Arg(char *dst, char const *format, int arg) {
        if (arg == 0) {
            if (useCustomHeadlods) {
                if (!strcmp(format, "Player__medhead__model1020311275__%d.o")) {
                    strcpy(dst, "Player__medhead__model1020311275__.o");
                    return 1;
                }
                else if (!strcmp(format, "Player__lowhead__model1020311335__%d.o")) {
                    strcpy(dst, "Player__lowhead__model1020311335__.o");
                    return 1;
                }
            }
            if (useCustomBodies) {
                if (!strcmp(format, "Player__Shadow__model1019510585__%d.o")) {
                    strcpy(dst, "Player__Shadow__model1019510585__.o");
                    return 1;
                }
                else if (!strcmp(format, "Player__Shadow__model324920654296875__%d.o")) {
                    strcpy(dst, "Player__Shadow__model324920654296875__.o");
                    return 1;
                }
                else if (!strcmp(format, "Player__Shadow__model110107421875__%d.o")) {
                    strcpy(dst, "Player__Shadow__model110107421875__.o");
                    return 1;
                }
                else if (!strcmp(format, "Player__Shadow__model293365478515625__%d.o")) {
                    strcpy(dst, "Player__Shadow__model293365478515625__.o");
                    return 1;
                }
            }
        }
        return CallAndReturnDynGlobal<int>(GfxCoreAddress(0x384C63), dst, format, arg); // sprintf // done
    }

    void OnSetupPlayerModel(void *desc) {
        CallDynGlobal(GfxCoreAddress(0x1B7100), desc); // done
        int playerIndex = ((int)desc - GfxCoreAddress(0x5262D0)) / 0x1030; // done
        if (playerIndex >= 0 && playerIndex < NUM_PLAYERS) {
            if (useCustomHeadlods) {
                PLAYER_HEADLOD1_ID[playerIndex] = 0;
                PLAYER_HEADLOD2_ID[playerIndex] = 0;
                bool hasheadlodid = false;
                int headlodid = *raw_ptr<unsigned int>(desc, 0xFE0); // done
                if (headlodid > 0)
                    hasheadlodid = true;
                else if (headlodid < 0) {
                    int hairtypeid = *raw_ptr<unsigned int>(desc, 0xFE4); // done
                    if (hairtypeid >= 0) {
                        headlodid = -hairtypeid;
                        hasheadlodid = true;
                    }
                }
                if (hasheadlodid) {
                    int sizeHeadLod1 = CallMethodAndReturnDynGlobal<int>(GfxCoreAddress(0x3531E0), *(void **)GfxCoreAddress(0x57EFD8), Format(HeadLodFormat1, headlodid).c_str());
                    if (sizeHeadLod1 > 0)
                        PLAYER_HEADLOD1_ID[playerIndex] = headlodid;
                    int sizeHeadLod2 = CallMethodAndReturnDynGlobal<int>(GfxCoreAddress(0x3531E0), *(void **)GfxCoreAddress(0x57EFD8), Format(HeadLodFormat2, headlodid).c_str());
                    if (sizeHeadLod2 > 0)
                        PLAYER_HEADLOD2_ID[playerIndex] = headlodid;
                }
            }
            if (useCustomBodies) {
                PLAYER_BODY6_ID[playerIndex] = 0;
                PLAYER_BODY7_ID[playerIndex] = 0;
                PLAYER_BODY8_ID[playerIndex] = 0;
                PLAYER_BODY9_ID[playerIndex] = 0;
                int playerid = *raw_ptr<unsigned int>(desc, 0xFE0); // changed to headid for TCM // done
                if (playerid > 0) {
                    int sizeBody6 = CallMethodAndReturnDynGlobal<int>(GfxCoreAddress(0x3531E0), *(void **)GfxCoreAddress(0x57EFD8), Format(BodyFormat6, playerid).c_str());
                    if (sizeBody6 > 0)
                        PLAYER_BODY6_ID[playerIndex] = playerid;
                    int sizeBody7 = CallMethodAndReturnDynGlobal<int>(GfxCoreAddress(0x3531E0), *(void **)GfxCoreAddress(0x57EFD8), Format(BodyFormat7, playerid).c_str());
                    if (sizeBody7 > 0)
                        PLAYER_BODY7_ID[playerIndex] = playerid;
                    int sizeBody8 = CallMethodAndReturnDynGlobal<int>(GfxCoreAddress(0x3531E0), *(void **)GfxCoreAddress(0x57EFD8), Format(BodyFormat8, playerid).c_str());
                    if (sizeBody8 > 0)
                        PLAYER_BODY8_ID[playerIndex] = playerid;
                    int sizeBody9 = CallMethodAndReturnDynGlobal<int>(GfxCoreAddress(0x3531E0), *(void **)GfxCoreAddress(0x57EFD8), Format(BodyFormat9, playerid).c_str());
                    if (sizeBody9 > 0)
                        PLAYER_BODY9_ID[playerIndex] = playerid;
                }
            }
        }
    }
}

void InstallGfx_TCM2004() {
    using namespace tcm2004;
    patch::SetUInt(GfxCoreAddress(0x35436E) + 1, 50'000); // fat fix (50000 entries) // done
    patch::RedirectCall(GfxCoreAddress(0x36AFBD), OnTextureCopyData); // done
    patch::SetUShort(GfxCoreAddress(0x36AFB8), 0xC68B); // mov eax, [esi] => mov eax, esi // done
    patch::RedirectCall(GfxCoreAddress(0x355CD0), OnNewModelUserData); // done
    useCustomHeadlods = false; // doesn't work on TCM2004
    useCustomBodies = false;
    if (useCustomHeadlods || useCustomBodies) {
        patch::RedirectCall(GfxCoreAddress(0x1C7DAC), OnResolveScene); // done
        patch::RedirectCall(GfxCoreAddress(0x354AC3), OnFormatModelName1Arg); // done
        patch::RedirectCall(GfxCoreAddress(0x1B368E), OnSetupPlayerModel); // done
        patch::RedirectCall(GfxCoreAddress(0x1B4B00), OnSetupPlayerModel); // done
        patch::RedirectCall(GfxCoreAddress(0x1B5A46), OnSetupPlayerModel); // done
    }
    if (useCustomHeadlods || useCustomBodies) {
        patch::SetPointer(GfxCoreAddress(0x366FE2 + 1), NewModelCollections + 8); // done
        patch::SetPointer(GfxCoreAddress(0x367004 + 2), NewModelCollections); // done
        patch::SetPointer(GfxCoreAddress(0x367036 + 2), NewModelCollections + 4); // done
        patch::SetPointer(GfxCoreAddress(0x36703E + 2), NewModelCollections + 4); // done
        patch::SetPointer(GfxCoreAddress(0x367044 + 2), NewModelCollections); // done
        patch::SetPointer(GfxCoreAddress(0x367F10 + 1), NewModelCollections + 8); // done
        patch::SetPointer(GfxCoreAddress(0x367F2D + 2), NewModelCollections + 0x10); // done
        patch::SetPointer(GfxCoreAddress(0x367F33 + 2), NewModelCollections); // done
        patch::SetPointer(GfxCoreAddress(0x367F76 + 2), NewModelCollections + 8); // done
        patch::SetPointer(GfxCoreAddress(0x367F7C + 2), NewModelCollections + 0x10); // done
        patch::SetPointer(GfxCoreAddress(0x367FB2 + 1), NewModelCollections + 8); // done
        patch::SetPointer(GfxCoreAddress(0x367FDA + 2), NewModelCollections + 0x10); // done
        patch::SetPointer(GfxCoreAddress(0x367FE3 + 2), NewModelCollections); // done
        patch::SetPointer(GfxCoreAddress(0x368CA6 + 2), NewModelCollections); // done
        patch::SetPointer(GfxCoreAddress(0x368DA4 + 2), NewModelCollections); // done
        patch::SetPointer(GfxCoreAddress(0x368DAC + 2), NewModelCollections); // done
    }
    patch::SetUInt(GfxCoreAddress(0x2BEE55 + 1), 32); // done
    patch::SetUChar(GfxCoreAddress(0x1DC908 + 1), 64); // done
    patch::SetUInt(GfxCoreAddress(0x1DC90A + 1), 256); // done
    patch::SetUInt(GfxCoreAddress(0x1DBDF8 + 3), 50); // done
    
    //patch::RedirectCall(GfxCoreAddress(0x1DC967), Fifa04NumberCB<128>); // done
    //patch::RedirectCall(GfxCoreAddress(0x1DC9BB), Fifa04NumberCB<64>); // done

    patch::SetUInt(GfxCoreAddress(0x4BD090), 10485760 * 2); // done
    patch::SetUInt(GfxCoreAddress(0x4BD090 + 4), 62914560 * 2); // done
    patch::SetUInt(GfxCoreAddress(0x1206E + 6), 326492416); // done
    patch::Nop(GfxCoreAddress(0x12087), 5); // done
    patch::Nop(GfxCoreAddress(0x12097), 6); // done
    patch::Nop(GfxCoreAddress(0x120B8), 6); // done
    patch::Nop(GfxCoreAddress(0x120C9), 5); // done
    patch::SetUInt(GfxCoreAddress(0x120A3 + 1), 75497472 * 2); // done
    patch::SetUInt(GfxCoreAddress(0x120E2 + 1), 75497472 * 2); // done

    // adboards 16
    //patch::SetUInt(GfxCoreAddress(0x1C8644) + 4, 16); // done

    if (useCustomHeadlods) {
        strcpy(HeadLodFormat1, "Player__medhead__model1020311275__%d.o");
        strcpy(HeadLodFormat2, "Player__lowhead__model1020311335__%d.o");
    }
    if (useCustomBodies) {
        strcpy(BodyFormat6, "Player__Shadow__model1019510585__%d.o");
        strcpy(BodyFormat7, "Player__Shadow__model324920654296875__%d.o");
        strcpy(BodyFormat8, "Player__Shadow__model110107421875__%d.o");
        strcpy(BodyFormat9, "Player__Shadow__model293365478515625__%d.o");
    }
}
