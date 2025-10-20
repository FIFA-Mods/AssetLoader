#include "plugin.h"
#include "MyFilesystem.h"
#include "CustomCallbacks.h"

using namespace plugin;
using namespace std;
using namespace MyFilesystem;

namespace fifa05 {
    bool const useCustomHeadlods = true;
    const unsigned int NUM_PLAYERS = 25; // 11 * 2 players + 1 referee + 2 linesmen
    struct ResFormatArg { int *a; int b; };
    int PLAYER_HEADLOD1_ID[NUM_PLAYERS];
    int PLAYER_HEADLOD2_ID[NUM_PLAYERS];
    ResFormatArg PLAYER_HEADLOD1_ACCESSORS[NUM_PLAYERS];
    ResFormatArg PLAYER_HEADLOD2_ACCESSORS[NUM_PLAYERS];
    char HeadLodFormat1[64];
    char HeadLodFormat2[64];
    unsigned char NewModelCollections[0x120 * 1024];

    template<int S>
    void *Fifa05NumberCB1(int a1, int a2, int a3, int a4, int a5, int a6, int a7) {
        return CallAndReturn<void *, 0x5DEBC0>(a1, S, S, a4, a5, a6, a7); // done
    }

    template<int S>
    void *Fifa05NumberCB2(int a1, int a2, int a3, int a4, int a5, int a6) {
        return CallAndReturn<void *, 0x5DF960>(a1, S, S, a4, a5, a6); // done
    }

    void METHOD MyGetHotspotData(void *t, DUMMY_ARG, int *hs, void *data) {
        CallMethod<0x59F540>(t, hs, data); // done
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
        currentDataSize = currentSection - currentData + CallAndReturn<unsigned int, 0x68C0A1>(currentSection); // GetFshSectionSize() // done
        if (currentDataSize < newDataSize) { // if new data doesn't fit into previously allocated memory
            // temporary remove tex info and clear its memory
            Call<0x5DF040>(texInfo); // RemoveTexInfo() // done
            // allocate memory for new texture
            void *newMem = CallAndReturn<void *, 0x5DE3A0>(newData); // TexPoolAllocate() // done
            // re-activate tex info and store new memory
            *raw_ptr<unsigned char>(texInfo, 281) = 1; // texInfo.active = true
            *raw_ptr<void *>(texInfo, 0) = newMem; // texInfo.data = newMem
        }
        else // in other case just copy it
            Call<0x67EF00>(currentData, newData, newDataSize); // MemCpy() // done
    }

    void *OnNewModelUserData(bool isOrd, unsigned char zero, void *sceneEntry, void *block, unsigned int fileSize, const char *fileName) {
        void *oldData = *raw_ptr<void *>(block, 0x50); // block.data
        void *modelCollection = nullptr;
        void *mc = useCustomHeadlods ? NewModelCollections : (void *)0x859E78; // ModelCollections // done
        for (unsigned int i = 0; i < *(unsigned int *)0x859CC8; i++) { // NumModelCollections // done
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
                Call<0x67C4D0>(oldData); // delete() // done
                void *newData = CallAndReturn<void *, 0x5BDBE0>(gMemName, fileName, fileSize, 0x400, 16, 0); // FifaMemAllocate() // done
                *raw_ptr<void *>(block, 0x50) = newData; // block.data = newData
                *raw_ptr<void *>(modelCollection, 0x8) = newData; // modelCollection.data = newData
                *raw_ptr<unsigned int>(modelCollection, 0x10) = newFileSize; // modelCollection.size = newFileSize
                fileSize = newFileSize;
            }
        }
        return CallAndReturn<void *, 0x5E0020>(isOrd, zero, sceneEntry, block, fileSize, fileName); // NewModelUserData() // done
    }

    void *OnResolveScene(void *scene, void *attributeCallback) {
        void *result = CallAndReturn<void *, 0x5E1290>(scene, attributeCallback); // done
        void *sceneEntry = *raw_ptr<void *>(scene, 0x44); // done
        unsigned int numSceneEntries = *raw_ptr<unsigned int>(scene, 0x40); // done
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
                            void *modelBlock = *raw_ptr<void *>(sceneEntry, 0x44); // done
                            unsigned int numModelBlocks = *raw_ptr<unsigned int>(sceneEntry, 0x40); // done
                            for (unsigned int m = 0; m < numModelBlocks; m++) {
                                char const *resNameFormat = *raw_ptr<char const *>(modelBlock, 0x4); // done
                                if (resNameFormat) {
                                    string resNameFormatStr = resNameFormat;
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
                                modelBlock = raw_ptr<void>(modelBlock, 0x58); // done
                            }
                        }
                    }
                }
            }
            sceneEntry = raw_ptr<void>(sceneEntry, 0x6C); // done
        }
        return result;
    }

    void OnSetupPlayerModel(void *desc) {
        int playerIndex = ((int)desc - 0x83A620) / 0x1200; // done
        if (playerIndex >= 0 && playerIndex < NUM_PLAYERS) {
            int hairlodid = 0;
            PLAYER_HEADLOD1_ID[playerIndex] = -hairlodid;
            PLAYER_HEADLOD2_ID[playerIndex] = -hairlodid;
            int headid = *raw_ptr<int>(desc, 0xFE0);
            if (headid > 0) {
                int sizeHeadLod1 = CallMethodAndReturn<int, 0x5BE9F0>(*(void **)0x85989C, Format(HeadLodFormat1, headid).c_str());
                if (sizeHeadLod1 > 0)
                    PLAYER_HEADLOD1_ID[playerIndex] = headid;
                int sizeHeadLod2 = CallMethodAndReturn<int, 0x5BE9F0>(*(void **)0x85989C, Format(HeadLodFormat2, headid).c_str());
                if (sizeHeadLod2 > 0)
                    PLAYER_HEADLOD2_ID[playerIndex] = headid;
            }
        }
    }

    void __declspec(naked) OnSetupPlayerModelBE() {
        __asm {
            mov eax, 0x5BA400
            call eax
            push esi
            call OnSetupPlayerModel
            add esp, 4
            retn
        }
    }
}

void Install_FIFA05() {
    using namespace fifa05;
    patch::SetUInt(0x5C0AAF + 1, 50'000); // fat fix (50000 entries) // done
    patch::RedirectCall(0x5DF74F, OnTextureCopyData); // done
    patch::SetUShort(0x5DF74A, 0xC68B); // mov eax, [esi] => mov eax, esi // done
    patch::RedirectCall(0x5E314E, OnNewModelUserData); // done
    if (useCustomHeadlods) {
        patch::RedirectCall(0x585021, OnResolveScene); // done
        patch::RedirectCall(0x5BB1E4, OnSetupPlayerModelBE);
        patch::RedirectCall(0x5BB9BC, OnSetupPlayerModelBE);
        patch::RedirectCall(0x5BD43C, OnSetupPlayerModelBE);
        patch::SetPointer(0x5D6F72 + 1, NewModelCollections + 8); // done
        patch::SetPointer(0x5D6F82 + 1, NewModelCollections + std::size(NewModelCollections) + 8); // done
        patch::SetPointer(0x5D6FA2 + 1, NewModelCollections); // done
        patch::SetPointer(0x5D79A4 + 1, NewModelCollections + 8); // done
        patch::SetPointer(0x5D7A17 + 2, NewModelCollections); // done
        patch::SetPointer(0x5D8A82 + 1, NewModelCollections + 8); // done
        patch::SetPointer(0x5D8AC6 + 1, NewModelCollections); // done
        patch::SetPointer(0x5D9424 + 2, NewModelCollections); // done
        patch::SetPointer(0x5D9534 + 2, NewModelCollections); // done
        patch::SetPointer(0x5D953C + 2, NewModelCollections); // done
        patch::SetPointer(0x5DA134 + 1, NewModelCollections + 8); // done
        patch::SetPointer(0x5DA18F + 2, NewModelCollections); // done
        strcpy(HeadLodFormat1, "Player__medhead__model1020311275__%d.o");
        strcpy(HeadLodFormat2, "Player__lowhead__model1020311335__%d.o");
    }
    patch::SetUInt(0x63B1B5 + 1, 32); // done
    patch::SetUChar(0x596144 + 1, 64); // done
    patch::SetUInt(0x596146 + 1, 256); // done
    patch::SetUChar(0x597635 + 1, 50); // done
    patch::RedirectCall(0x5961C1, Fifa05NumberCB1<128>); // done
    patch::RedirectCall(0x596255, Fifa05NumberCB1<64>); // done
    patch::RedirectCall(0x5961E3, Fifa05NumberCB2<128>); // done
    patch::RedirectCall(0x596283, Fifa05NumberCB2<64>); // done
    patch::SetUInt(0x59F6A5 + 1, 32 * 2); // done
    patch::SetUInt(0x59F6BA + 1, 64 * 2); // done
    patch::SetUInt(0x59F70F + 4, 16 * 2); // done
    patch::RedirectCall(0x59F5F6, MyGetHotspotData); // done
    patch::SetUInt(0x79E618, 10485760 * 2); // done
    patch::SetUInt(0x79E61C, 62914560 * 2); // done
    patch::SetUInt(0x5EE38A + 6, 326492416); // done
    patch::Nop(0x5EE3B5, 6); // done
    patch::Nop(0x5EE39E, 10); // done
    patch::SetUInt(0x5EE3D4 + 1, 75497472 * 2); // done
    patch::SetUInt(0x5EE3A8 + 1, 75497472 * 2); // done

    // adboards 16
    //patch::SetUInt(0x585C70 + 4, 16);
}
