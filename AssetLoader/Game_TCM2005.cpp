#include "plugin.h"
#include "MyFilesystem.h"
#include "CustomCallbacks.h"
#include "GfxCoreHook.h"
#include "MySettings.h"

using namespace plugin;
using namespace std;
using namespace MyFilesystem;

namespace tcm2005 {
    bool useCustomHeadlods = false;
    const unsigned int NUM_PLAYERS = 25; // 11 * 2 players + 1 referee + 2 linesmen
    struct ResFormatArg { int *a; int b; };
    int PLAYER_HEADLOD1_ID[NUM_PLAYERS];
    int PLAYER_HEADLOD2_ID[NUM_PLAYERS];
    ResFormatArg PLAYER_HEADLOD1_ACCESSORS[NUM_PLAYERS];
    ResFormatArg PLAYER_HEADLOD2_ACCESSORS[NUM_PLAYERS];
    char HeadLodFormat1[64];
    char HeadLodFormat2[64];
    unsigned char NewModelCollections[0x128 * 1024];

    template<int S>
    void *Fifa05NumberCB1(int a1, int a2, int a3, int a4, int a5, int a6, int a7) {
        return CallAndReturnDynGlobal<void *>(GfxCoreAddress(0x3C5B50), a1, S, S, a4, a5, a6, a7); // done
    }

    template<int S>
    void *Fifa05NumberCB2(int a1, int a2, int a3, int a4, int a5, int a6) {
        return CallAndReturnDynGlobal<void *>(GfxCoreAddress(0x3C6820), a1, S, S, a4, a5, a6); // done
    }

    void METHOD MyGetHotspotData(void *t, DUMMY_ARG, int *hs, void *data) {
        CallMethodDynGlobal(GfxCoreAddress(0x202B20), t, hs, data);
        for (int i = 0; i < 12; i++) {
            hs[i + 1] *= 2;
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
        currentDataSize = currentSection - currentData + CallAndReturnDynGlobal<unsigned int>(GfxCoreAddress(0x31E210), currentSection); // GetFshSectionSize() // done
        if (currentDataSize < newDataSize) { // if new data doesn't fit into previously allocated memory
            // temporary remove tex info and clear its memory
            CallDynGlobal(GfxCoreAddress(0x3C5F70), texInfo); // RemoveTexInfo() // done
            // allocate memory for new texture
            void *newMem = CallAndReturnDynGlobal<void *>(GfxCoreAddress(0x3C5AB0), newData); // TexPoolAllocate() // done
            // re-activate tex info and store new memory
            *raw_ptr<unsigned char>(texInfo, 277) = 1; // texInfo.active = true // done
            *raw_ptr<void *>(texInfo, 0) = newMem; // texInfo.data = newMem
        }
        else // in other case just copy it
            CallDynGlobal(GfxCoreAddress(0x338500), currentData, newData, newDataSize); // MemCpy() // done
    }

    void *OnNewModelUserData(bool isOrd, void *sceneEntry, void *block, unsigned int fileSize, const char *fileName) {
        void *oldData = *raw_ptr<void *>(block, 0x50); // block.data
        void *modelCollection = nullptr;
        void *mc = useCustomHeadlods ? NewModelCollections : (void *)GfxCoreAddress(0x721698); // ModelCollections // done
        for (unsigned int i = 0; i < *(unsigned int *)GfxCoreAddress(0x721508); i++) { // NumModelCollections // done
            if (*raw_ptr<void *>(mc, 0x8) == oldData) { // modelCollection.data
                modelCollection = mc;
                break;
            }
            mc = raw_ptr<void>(mc, 296); // mc++ // done
        }
        if (modelCollection) {
            unsigned int newFileSize = max(912u, fileSize);
            if (*raw_ptr<unsigned int>(modelCollection, 0x10) < newFileSize) { // modelCollection.size
                CallDynGlobal(GfxCoreAddress(0x333130), oldData); // delete() // done
                void *newData = CallAndReturnDynGlobal<void *>(GfxCoreAddress(0x26880), "SGSM", fileName, fileSize, 0x400, 16, 0); // FifaMemAllocate() // done
                *raw_ptr<void *>(block, 0x50) = newData; // block.data = newData
                *raw_ptr<void *>(modelCollection, 0x8) = newData; // modelCollection.data = newData
                *raw_ptr<unsigned int>(modelCollection, 0x10) = newFileSize; // modelCollection.size = newFileSize
                fileSize = newFileSize;
            }
        }
        return CallAndReturnDynGlobal<void *>(GfxCoreAddress(0x3BD940), isOrd, sceneEntry, block, fileSize, fileName); // NewModelUserData() // done
    }

    void *OnResolveScene(void *scene, void *attributeCallback) {
        void *result = CallAndReturnDynGlobal<void *>(GfxCoreAddress(0x3BC0B0), scene, attributeCallback); // done
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
                                    if (useCustomHeadlods) {
                                        if (resNameFormatStr == "Player__medhead__model1020311275__.o") {
                                            *raw_ptr<char const *>(modelBlock, 0x4) = HeadLodFormat1;
                                            *raw_ptr<unsigned int>(modelBlock, 0x28) = 1;
                                            PLAYER_HEADLOD1_ACCESSORS[playerIndex].a = &PLAYER_HEADLOD1_ID[playerIndex];
                                            PLAYER_HEADLOD1_ACCESSORS[playerIndex].b = 0;
                                            *raw_ptr<void *>(modelBlock, 0x2C) = &PLAYER_HEADLOD1_ACCESSORS[playerIndex];
                                        }
                                        else if (resNameFormatStr == "Player__lowhead__model1020311335__.o") {
                                            *raw_ptr<char const *>(modelBlock, 0x4) = HeadLodFormat2;
                                            *raw_ptr<unsigned int>(modelBlock, 0x28) = 1;
                                            PLAYER_HEADLOD2_ACCESSORS[playerIndex].a = &PLAYER_HEADLOD2_ID[playerIndex];
                                            PLAYER_HEADLOD2_ACCESSORS[playerIndex].b = 0;
                                            *raw_ptr<void *>(modelBlock, 0x2C) = &PLAYER_HEADLOD2_ACCESSORS[playerIndex];
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
                if (!strcmp(format, "Player__medhead__model1020311275__%d.o")) {
                    strcpy(dst, "Player__medhead__model1020311275__.o");
                    return 1;
                }
                else if (!strcmp(format, "Player__lowhead__model1020311335__%d.o")) {
                    strcpy(dst, "Player__lowhead__model1020311335__.o");
                    return 1;
                }
            }
        }
        return CallAndReturnDynGlobal<int>(GfxCoreAddress(0x3E09F3), dst, format, arg); // sprintf // done
    }

    void OnSetupPlayerModel(void *desc) {
        CallDynGlobal(GfxCoreAddress(0x1CA2C0), desc); // done
        int playerIndex = ((int)desc - GfxCoreAddress(0x59E670)) / 0x11F0; // done
        if (playerIndex >= 0 && playerIndex < NUM_PLAYERS) {
            if (useCustomHeadlods) {
                PLAYER_HEADLOD1_ID[playerIndex] = 0;
                PLAYER_HEADLOD2_ID[playerIndex] = 0;
                bool hasheadlodid = false;
                int headlodid = *raw_ptr<unsigned int>(desc, 0xFDC);
                if (headlodid > 0)
                    hasheadlodid = true;
                else if (headlodid < 0) {
                    int hairtypeid = *raw_ptr<unsigned int>(desc, 0xFE0);
                    if (hairtypeid >= 0) {
                        headlodid = -hairtypeid;
                        hasheadlodid = true;
                    }
                }
                if (hasheadlodid) {
                    int sizeHeadLod1 = CallMethodAndReturnDynGlobal<int>(GfxCoreAddress(0x39F880), *(void **)GfxCoreAddress(0x61FB98), Format(HeadLodFormat1, headlodid).c_str());
                    if (sizeHeadLod1 > 0)
                        PLAYER_HEADLOD1_ID[playerIndex] = headlodid;
                    int sizeHeadLod2 = CallMethodAndReturnDynGlobal<int>(GfxCoreAddress(0x39F880), *(void **)GfxCoreAddress(0x61FB98), Format(HeadLodFormat2, headlodid).c_str());
                    if (sizeHeadLod2 > 0)
                        PLAYER_HEADLOD2_ID[playerIndex] = headlodid;
                }
            }
        }
    }
}

void InstallGfx_TCM2005() {
    using namespace tcm2005;
    patch::SetUInt(GfxCoreAddress(0x3A199A) + 1, settings().FAT_MAX_ENTRIES); // fat fix (50000 entries) // done
    patch::RedirectCall(GfxCoreAddress(0x3C662D), OnTextureCopyData); // done
    patch::SetUShort(GfxCoreAddress(0x3C6628), 0xC68B); // mov eax, [esi] => mov eax, esi // done
    patch::RedirectCall(GfxCoreAddress(0x3BDF7F), OnNewModelUserData); // done
    useCustomHeadlods = true;
    if (useCustomHeadlods) {
        patch::RedirectCall(GfxCoreAddress(0x1DB02C), OnResolveScene); // done
        patch::RedirectCall(GfxCoreAddress(0x3BBB83), OnFormatModelName1Arg); // done
        patch::RedirectCall(GfxCoreAddress(0x1C6EB8), OnSetupPlayerModel); // done
        patch::RedirectCall(GfxCoreAddress(0x1C8420), OnSetupPlayerModel); // done
        patch::RedirectCall(GfxCoreAddress(0x1C9094), OnSetupPlayerModel); // done
    }
    if (useCustomHeadlods) {
        patch::SetPointer(GfxCoreAddress(0x3C04C2 + 1), NewModelCollections + 8); // done
        patch::SetPointer(GfxCoreAddress(0x3C04E4 + 2), NewModelCollections); // done
        patch::SetPointer(GfxCoreAddress(0x3C0516 + 2), NewModelCollections + 4); // done
        patch::SetPointer(GfxCoreAddress(0x3C051E + 2), NewModelCollections + 4); // done
        patch::SetPointer(GfxCoreAddress(0x3C0524 + 2), NewModelCollections); // done
        patch::SetPointer(GfxCoreAddress(0x3C1450 + 1), NewModelCollections + 8); // done
        patch::SetPointer(GfxCoreAddress(0x3C146D + 2), NewModelCollections + 0x10); // done
        patch::SetPointer(GfxCoreAddress(0x3C1473 + 2), NewModelCollections); // done
        patch::SetPointer(GfxCoreAddress(0x3C14B6 + 2), NewModelCollections + 8); // done
        patch::SetPointer(GfxCoreAddress(0x3C14BC + 2), NewModelCollections + 0x10); // done
        patch::SetPointer(GfxCoreAddress(0x3C14F2 + 1), NewModelCollections + 8); // done
        patch::SetPointer(GfxCoreAddress(0x3C151A + 2), NewModelCollections + 0x10); // done
        patch::SetPointer(GfxCoreAddress(0x3C1523 + 2), NewModelCollections); // done
        patch::SetPointer(GfxCoreAddress(0x3C21B3 + 1), NewModelCollections); // done
        patch::SetPointer(GfxCoreAddress(0x3C2A06 + 2), NewModelCollections); // done
        patch::SetPointer(GfxCoreAddress(0x3C2B04 + 2), NewModelCollections); // done
        patch::SetPointer(GfxCoreAddress(0x3C2B0C + 2), NewModelCollections); // done
    }
    patch::SetUInt(GfxCoreAddress(0x2F73F5 + 1), 32); // done
    patch::SetUChar(GfxCoreAddress(0x1F1EA6 + 1), 64); // done
    patch::SetUInt(GfxCoreAddress(0x1F1EA8 + 1), 256); // done
    patch::SetUInt(GfxCoreAddress(0x1F15B1 + 3), 50); // done
    
    patch::RedirectCall(GfxCoreAddress(0x1F1F2E), Fifa05NumberCB1<128>); // done
    patch::RedirectCall(GfxCoreAddress(0x1F2003), Fifa05NumberCB1<64>); // done
    patch::RedirectCall(GfxCoreAddress(0x1F1F68), Fifa05NumberCB2<128>); // done
    patch::RedirectCall(GfxCoreAddress(0x1F203D), Fifa05NumberCB2<64>); // done

    patch::SetUInt(GfxCoreAddress(0x202C69 + 3), 64 * 2);
    patch::SetUInt(GfxCoreAddress(0x202C70 + 3), 64 * 2);
    patch::SetUInt(GfxCoreAddress(0x202CB7 + 1), 64 * 2);
    patch::SetUInt(GfxCoreAddress(0x202D26 + 1), 64 * 2);
    patch::SetUInt(GfxCoreAddress(0x202D7B + 1), 64 * 2);
    patch::SetUInt(GfxCoreAddress(0x202D9E + 1), 64 * 2);

    patch::SetUInt(GfxCoreAddress(0x202C99 + 1), 32 * 2);
    patch::SetUInt(GfxCoreAddress(0x202D1E + 4), 32 * 2);
    
    patch::RedirectCall(GfxCoreAddress(0x202BF1), MyGetHotspotData); // done

    patch::SetUInt(GfxCoreAddress(0x530C58), settings().SGR_TEXTURE_POOL_SIZE_FE); // done
    patch::SetUInt(GfxCoreAddress(0x530C58 + 4), settings().SGR_TEXTURE_POOL_SIZE_BE); // done
    patch::SetUInt(GfxCoreAddress(0x1515E + 6), settings().REAL_MEMORY_HEAP_SIZE); // done
    patch::Nop(GfxCoreAddress(0x15177), 5); // done
    patch::Nop(GfxCoreAddress(0x15187), 6); // done
    patch::Nop(GfxCoreAddress(0x151A8), 6); // done
    patch::Nop(GfxCoreAddress(0x151B9), 5); // done
    patch::SetUInt(GfxCoreAddress(0x15193 + 1), settings().TEXTURE_MEMORY_SIZE); // done
    patch::SetUInt(GfxCoreAddress(0x151D2 + 1), settings().TEXTURE_MEMORY_SIZE); // done

    if (useCustomHeadlods) {
        strcpy(HeadLodFormat1, "Player__medhead__model1020311275__%d.o");
        strcpy(HeadLodFormat2, "Player__lowhead__model1020311335__%d.o");
    }
}
