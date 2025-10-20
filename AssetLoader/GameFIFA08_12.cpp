#include "plugin.h"
#include "MyFilesystem.h"
#include "CustomCallbacks.h"
#include "MySettings.h"

using namespace plugin;
using namespace std;
using namespace MyFilesystem;

namespace fifa08_12 {
    const unsigned int MAX_TEX_COLLECTIONS = 200; // default 200
    const unsigned int MAX_TEXTURES_IN_FSH = 1024; // default 64
    const unsigned int MAX_TEX_INFOS = 1280; // default 320
    const unsigned int MAX_TEX_DYNAMIC_DATA_PTRS = 1024; // default 256
    bool useCustomHeadlods = false;
    const unsigned int NUM_PLAYERS = 25; // 11 * 2 players + 1 referee + 2 linesmen
    struct ResFormatArg { int *a; int b; };
    int PLAYER_HEADLOD1_ID[NUM_PLAYERS];
    int PLAYER_HEADLOD2_ID[NUM_PLAYERS];
    int PLAYER_BODY_DRAWSTATUS[NUM_PLAYERS];
    ResFormatArg PLAYER_HEADLOD1_ACCESSORS[NUM_PLAYERS];
    ResFormatArg PLAYER_HEADLOD2_ACCESSORS[NUM_PLAYERS];
    char HeadLodFormat1[64];
    char HeadLodFormat2[64];
    unsigned char NewModelCollections[0x120 * 1024]; // default 256
    unsigned char NewTextureCollections[(0x10C + (MAX_TEXTURES_IN_FSH - 64) * 4) * MAX_TEX_COLLECTIONS];
    unsigned char NewTexInfos[0x11C * MAX_TEX_INFOS];
    unsigned char NewTexDynamicDataPtrs[4 * MAX_TEX_DYNAMIC_DATA_PTRS];
    unsigned int NewUsedTags[MAX_TEXTURES_IN_FSH];

    int GetFileSize(std::string const &filename) {
        return CallMethodAndReturn<int, 0x5C3EA0>(*(void **)0xC965B0, filename.c_str());
    }

    void __declspec(naked) OnCompareTexTag1() {
        __asm {
            cmp NewUsedTags[eax * TYPE int], ecx
            mov edx, 0x5F269B // done
            jmp edx
        }
    }

    void __declspec(naked) OnCompareTexTag2() {
        __asm {
            mov NewUsedTags[esi * TYPE int], edi
            mov edx, 0x5F28F0 // done
            jmp edx
        }
    }

    struct cSGR_Memory {
        unsigned char *m_pStart;
        unsigned char *m_pEnd;
        unsigned char *m_pFree;
        unsigned int m_nBytesAllocated;
    };

    struct SHAPE {
        unsigned int type : 8;
        int next : 24;
        signed short width;
        signed short height;
        signed short centerx;
        signed short centery;
        unsigned int reserved : 1;
        unsigned int transposed : 1;
        unsigned int swizzled : 1;
        unsigned int referenced : 1;
        int shapex : 12;
        unsigned int mipmaps : 4;
        int shapey : 12;
    };
    VALIDATE_SIZE(SHAPE, 0x10);

    struct TAR;

    struct SGR_ShapeID {
        int nShape;
        unsigned int nID;
    };

    struct SGR_Texture {
        SHAPE *pShape;
        unsigned int nMaterialID;
        struct SGR_ShapeID aShapeIDs[32];
        int nNumShapeIDs;
        TAR *pAnimTar;
        int nRefCount;
        char const *pName;
        unsigned char bDynamicallyCreated;
        unsigned char bValid;
        char _pad11A[2];
    };
    VALIDATE_SIZE(SGR_Texture, 0x11C);

    unsigned int sgrtex_cloneSize(SHAPE *shp) {
        unsigned char *currentSection = (unsigned char *)shp;
        if (*(unsigned int *)shp & 0xFFFFFF00) {
            do {
                unsigned int nextOffset = *(unsigned int *)currentSection >> 8;
                if (nextOffset)
                    currentSection += nextOffset;
                else
                    currentSection = 0;
            } while (*(unsigned int *)currentSection & 0xFFFFFF00);
        }
        return currentSection - (unsigned char *)shp + CallAndReturn<unsigned int, 0x7D826F>(currentSection); // SHAPE_size()
    }

    void OnTextureCopyData(SGR_Texture *tex, SHAPE *srcShape, unsigned int newSize) {
        unsigned int oldSize = sgrtex_cloneSize(tex->pShape);
        if (oldSize < newSize) {
            Call<0x5F2390>(tex); // deleteTexture() // done
            Call<0x5DE5F0>(); // unbindTARs()
            tex->pShape = CallAndReturn<SHAPE *, 0x5F11E0>(srcShape); // cloneShape() // done
            tex->bValid = true;
            *(unsigned int *)0xA71434 += 1; // gnNumTextures
        }
        else {
            Call<0x5DE5F0>(); // unbindTARs()
            Call<0x7CB476>(tex->pShape, srcShape, newSize); // MemCpy() // done
        }
        Call<0x5DDA30>(); // rebindTARs()
    }

    void *OnNewModelUserData(bool isOrd, unsigned char zero, void *sceneEntry, void *block, unsigned int fileSize, const char *fileName) {
        void *oldData = *raw_ptr<void *>(block, 0x50); // block.data
        void *modelCollection = nullptr;
        void *mc = useCustomHeadlods ? NewModelCollections : (void *)0xC96B98; // ModelCollections // done
        for (unsigned int i = 0; i < *(unsigned int *)0xC96968; i++) { // NumModelCollections // done
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
                Call<0x7CA279>(oldData); // delete() // done
                void *newData = CallAndReturn<void *, 0x5C2CB0>(gMemName, fileName, fileSize, 0x400, 16, 0); // FifaMemAllocate() // done
                *raw_ptr<void *>(block, 0x50) = newData; // block.data = newData
                *raw_ptr<void *>(modelCollection, 0x8) = newData; // modelCollection.data = newData
                *raw_ptr<unsigned int>(modelCollection, 0x10) = newFileSize; // modelCollection.size = newFileSize
                fileSize = newFileSize;
            }
        }
        return CallAndReturn<void *, 0x5F3E70>(isOrd, zero, sceneEntry, block, fileSize, fileName); // NewModelUserData() // done
    }

    void *OnResolveScene(void *scene, void *attributeCallback) {
        void *result = CallAndReturn<void *, 0x5F5090>(scene, attributeCallback); // done
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
                                || resNameFormatStr == "m335__.o")
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
                        }
                        modelBlock = raw_ptr<void>(modelBlock, 0x58);
                    }
                }
            }
            sceneEntry = raw_ptr<void>(sceneEntry, 0x6C);
        }
        return result;
    }

    void OnSetupPlayerModel(void *desc) {
        int playerIndex = ((int)desc - 0xC73300) / 0x11F0; // done
        if (playerIndex >= 0 && playerIndex < NUM_PLAYERS) {
            PLAYER_BODY_DRAWSTATUS[playerIndex] = 1;
            unsigned int playerid = *raw_ptr<unsigned int>(desc, 0x1008); // done
            if (playerid > 0) {
                if (GetFileSize(Format("custom_body_%d", playerid)) > 0)
                    PLAYER_BODY_DRAWSTATUS[playerIndex] = 0;
            }
            if (useCustomHeadlods) {
                PLAYER_HEADLOD1_ID[playerIndex] = 0;
                PLAYER_HEADLOD2_ID[playerIndex] = 0;
                int hairtypeid = *raw_ptr<unsigned int>(desc, 0x1028);
                if (GetFileSize(Format(HeadLodFormat1, playerid)) > 0)
                    PLAYER_HEADLOD1_ID[playerIndex] = playerid;
                else if (hairtypeid >= 0)
                    PLAYER_HEADLOD1_ID[playerIndex] = -hairtypeid;
                if (GetFileSize(Format(HeadLodFormat2, playerid)) > 0)
                    PLAYER_HEADLOD2_ID[playerIndex] = playerid;
                else if (hairtypeid >= 0)
                    PLAYER_HEADLOD2_ID[playerIndex] = -hairtypeid;
            }
        }
    }

    void OnSetupPlayerModelBE(void *desc) {
        OnSetupPlayerModel(desc);
        Call<0x5B0820>(desc); // done
    }

    void OnSetupPlayerModelFE_CreatePlayer(int a) {
        PLAYER_BODY_DRAWSTATUS[0] = 1;
        Call<0x597100>(a);
    }

    void OnSetupPlayerModelFE(int a, int playerid) {
        Call<0x5973E0>(a, playerid); // done
        OnSetupPlayerModel((void *)0xC73300); // done
    }

    void OnLoadTexturesToTexCollection(const char *poolName, int numFshs, void *fshCollection, char bReload, unsigned int maxTextures, unsigned int *outNumTextures, void *texCollection, void *sceneGroup) {
        Call<0x5F25E0>(poolName, numFshs, fshCollection, bReload, MAX_TEXTURES_IN_FSH, outNumTextures, texCollection, sceneGroup); // done
    }

    template<int S>
    void *Fifa08NumberCB1(int a1, int a2, int a3, int a4, int a5, int a6, int a7) {
        return CallAndReturn<void *, 0x5F19E0>(a1, S, S, a4, a5, a6, a7); // done
    }

    template<int S>
    void *Fifa08NumberCB2(int a1, int a2, int a3, int a4, int a5, int a6) {
        return CallAndReturn<void *, 0x5F2D30>(a1, S, S, a4, a5, a6); // done
    }

    void METHOD MyGetHotspotData(void *t, DUMMY_ARG, int *hs, void *data) {
        CallMethod<0x5B3CD0>(t, hs, data); // done
        for (int i = 0; i < 12; i++) {
            hs[i + 1] *= 2;
        }
    }
}

void Install_FIFA08_12() {
    using namespace fifa08_12;
    patch::SetUInt(0x5C5EEF + 1, settings().FAT_MAX_ENTRIES); // fat fix (50000 entries) // done
    patch::RedirectCall(0x5F2B1D, OnTextureCopyData); // done
    patch::SetUShort(0x5F2B18, 0xD68B); // mov edx, [esi] => mov edx, esi // done
    patch::Nop(0x5DEF4F, 5); // unbindTars()
    patch::Nop(0x5DEF65, 5); // rebindTars()
    patch::RedirectCall(0x5F6964, OnNewModelUserData); // done
    useCustomHeadlods = true;
    if (useCustomHeadlods) {
        patch::RedirectCall(0x590724, OnResolveScene); // done
        patch::RedirectCall(0x59813F, OnResolveScene); // done
        patch::RedirectCall(0x5BE801, OnSetupPlayerModelBE); // done
        patch::RedirectCall(0x5BDF52, OnSetupPlayerModelBE); // done
        patch::RedirectCall(0x598354, OnSetupPlayerModelFE); // done
        patch::RedirectCall(0x598366, OnSetupPlayerModelFE_CreatePlayer);
    }

    // player id 200000 - done
    unsigned int NEW_GEN_PLAYER_ID = 499'000;
    patch::SetUInt(0x489D48 + 1, NEW_GEN_PLAYER_ID + 999);  // 0x4870F2
    patch::SetUInt(0x489D66 + 1, NEW_GEN_PLAYER_ID);        // 0x487110
    patch::SetUInt(0x4A1C82 + 1, NEW_GEN_PLAYER_ID);        // 0x49DC72
    patch::SetUInt(0x4A1C9F + 1, NEW_GEN_PLAYER_ID + 484);  // 0x49DC8F
    patch::SetUInt(0x4A1E09 + 2, NEW_GEN_PLAYER_ID - 1);    // 0x49DDF9
    patch::SetUInt(0x4A201A + 2, NEW_GEN_PLAYER_ID + 484);  // 0x49E00A
    patch::SetUInt(0x4C4B65 + 1, NEW_GEN_PLAYER_ID);        // 0x4C9514
    patch::SetUInt(0x4C4E49 + 1, NEW_GEN_PLAYER_ID);        // 0x4C9885
    patch::SetUInt(0x4D72E0 + 2, NEW_GEN_PLAYER_ID);        // 0x4F9257
    patch::SetUInt(0x4DBA9E + 1, NEW_GEN_PLAYER_ID + 16);   // 0x50A5BE
    patch::SetUInt(0x4DBAB9 + 1, NEW_GEN_PLAYER_ID);        // 0x50A5D9
    patch::SetUInt(0x598060 + 2, NEW_GEN_PLAYER_ID);        // 0x587AD0
    patch::SetUInt(0x54D582 + 2, NEW_GEN_PLAYER_ID); // not sure

    if (useCustomHeadlods) { // done
        patch::SetPointer(0x5E2B92 + 1, NewModelCollections + 8);                                  // 0x5CE632
        patch::SetPointer(0x5E2BA2 + 1, NewModelCollections + std::size(NewModelCollections) + 8); // 0x5CE642
        patch::SetPointer(0x5E2BC2 + 1, NewModelCollections);                                      // 0x5CE662
        patch::SetPointer(0x5E3A34 + 1, NewModelCollections + 8);                                  // 0x5CF4C4
        patch::SetPointer(0x5E3AA7 + 2, NewModelCollections);                                      // 0x5CF537
        patch::SetPointer(0x5E4772 + 1, NewModelCollections + 8);                                  // 0x5D01D2
        patch::SetPointer(0x5E47B6 + 1, NewModelCollections);                                      // 0x5D0216
        patch::SetPointer(0x5E51E4 + 2, NewModelCollections);                                      // 0x5D0C44
        patch::SetPointer(0x5E52F4 + 2, NewModelCollections);                                      // 0x5D0D54
        patch::SetPointer(0x5E52FC + 2, NewModelCollections);                                      // 0x5D0D5C
        patch::SetPointer(0x5E5DC7 + 1, NewModelCollections + 8);                                  // 0x5D1827
        patch::SetPointer(0x5E5E1F + 2, NewModelCollections);                                      // 0x5D187F
    }
    // textures in fsh (texture collection) - done
    patch::RedirectCall(0x5F2DC5, OnLoadTexturesToTexCollection); // done
    patch::RedirectCall(0x5F2E15, OnLoadTexturesToTexCollection); // done
    patch::SetPointer(0x5F2D6F + 2, NewTextureCollections); // done
    patch::SetUInt(0x5F2D69 + 2, 0x10C + (MAX_TEXTURES_IN_FSH - 64) * 4); // done
    patch::SetUInt(0x5F2D76 + 1, 0x10C + (MAX_TEXTURES_IN_FSH - 64) * 4); // done
    patch::SetUInt(0x5F2D93 + 2, MAX_TEXTURES_IN_FSH * 4 + 4); // 0x5DF1D3
    patch::SetUInt(0x5F2DB7 + 2, MAX_TEXTURES_IN_FSH * 4);     // 0x5DF1F7
    patch::SetUInt(0x5F2E07 + 1, MAX_TEXTURES_IN_FSH * 4);     // 0x5DF247
    patch::SetUInt(0x5E38F8 + 2, MAX_TEXTURES_IN_FSH * 4);     // 0x5CF388
    patch::SetUInt(0x5E395B + 2, MAX_TEXTURES_IN_FSH * 4);     // 0x5CF3EB
    patch::SetUInt(0x5F17F4 + 2, MAX_TEXTURES_IN_FSH * 4);     // 0x5DDC34
    patch::SetUInt(0x59E862 + 2, MAX_TEXTURES_IN_FSH * 4);     // 0x5894E9
    patch::SetUInt(0x59E875 + 2, MAX_TEXTURES_IN_FSH * 4);     // 0x5894FC
    patch::SetUInt(0x59E890 + 2, MAX_TEXTURES_IN_FSH * 4);     // 0x589518
    patch::SetUInt(0x5F17E4 + 2, MAX_TEXTURES_IN_FSH * 4);     // 0x5DDC24
    patch::SetUInt(0x5F1830 + 2, MAX_TEXTURES_IN_FSH * 4);     // 0x5DDC70
    patch::SetUInt(0x5F1A2D + 2, MAX_TEXTURES_IN_FSH * 4);     // 0x5DDE6D
    patch::SetUInt(0x5F1A36 + 2, MAX_TEXTURES_IN_FSH * 4);     // 0x5DDE76
    // maybe more patches are required?
    //

    // textures in scene - done
    patch::SetPointer(0x5F14C4 + 1, NewTexInfos);                                 // 0x5DD904
    patch::SetUInt   (0x5F1537 + 2, MAX_TEX_INFOS);                               // 0x5DD977
    patch::SetPointer(0x1644D60, NewTexInfos + 0x119);                         // 0x5DD9A3
    patch::SetPointer(0x5F1573 + 1, NewTexInfos + 0x119 + MAX_TEX_INFOS * 0x11C); // 0x5DD9B3
    patch::SetPointer(0x5F1585 + 2, NewTexInfos);                                 // 0x5DD9C5
    patch::SetPointer(0x5F159F + 2, NewTexInfos + 0x119);                         // 0x5DD9DF
    patch::SetPointer(0x5F15A5 + 2, NewTexInfos + 0x110);                         // 0x5DD9E5
    patch::SetPointer(0x5F15AB + 2, NewTexInfos + 0x118);                         // 0x5DD9EB
    patch::SetPointer(0x5F15B5 + 2, NewTexInfos + 0x4);                           // 0x5DD9F5
    patch::SetPointer(0x5F15CC + 2, NewTexInfos + 0x114);                         // 0x5DDA0C
    patch::SetPointer(0x5F2103 + 1, NewTexInfos + 0x10C);                         // 0x5DE543
    patch::SetPointer(0x5F2131 + 2, NewTexInfos + 0x10C + MAX_TEX_INFOS * 0x11C); // 0x5DE571
    patch::SetPointer(0x5F2140 + 1, NewTexInfos);                                 // 0x5DE580
    patch::SetUInt   (0x5F213B + 1, MAX_TEX_INFOS * 0x11C / 4);                   // 0x5DE57B
    patch::SetPointer(0x5F23B9 + 1, NewTexInfos + 0x118);                         // 0x5DE7F9
    patch::SetPointer(0x5F2478 + 1, NewTexInfos + 0x118 + MAX_TEX_INFOS * 0x11C); // 0x5DE8B8
    patch::SetPointer(0x5F24D7 + 1, NewTexInfos);                                 // 0x5DE917
    patch::SetPointer(0x5F259B + 1, NewTexInfos + MAX_TEX_INFOS * 0x11C);         // 0x5DE9DB
    
    patch::SetPointer(0x5F2CE1 + 3, NewTexDynamicDataPtrs); // done
    patch::SetPointer(0x5F2CF6 + 3, NewTexDynamicDataPtrs); // done
    patch::SetPointer(0x5F1775 + 3, NewTexDynamicDataPtrs); // done

    patch::RedirectJump(0x5F2694, OnCompareTexTag1); // done
    patch::RedirectJump(0x5F28E9, OnCompareTexTag2); // done

    patch::SetUInt(0x778B85 + 1, 32); // done
    patch::SetUChar(0x1434EE2 + 1, 64); // done
    patch::SetUInt(0x1434EE4 + 1, 256); // done
    patch::SetUChar(0x1677E40 + 1, 50); // done

    patch::RedirectJump(0x1434FB3, Fifa08NumberCB1<128>); // done
    patch::SetPointer(0x14350E5 + 1, Fifa08NumberCB1<64>); // done
    patch::SetPointer(0x143500D + 1, Fifa08NumberCB2<128>); // done
    patch::RedirectJump(0x1435172, Fifa08NumberCB2<64>); // done
    patch::SetUInt(0x5B3E35 + 1, 32 * 2); // done
    patch::SetUInt(0x5B3E4A + 1, 64 * 2); // done
    patch::SetUInt(0x5B3E9F + 4, 16 * 2); // done
    patch::RedirectCall(0x5B3D86, MyGetHotspotData); // done

    patch::SetUInt(0x947988, settings().SGR_TEXTURE_POOL_SIZE_FE); // done
    patch::SetUInt(0x94798C, settings().SGR_TEXTURE_POOL_SIZE_BE); // done
    patch::SetUInt(0x60557A + 6, settings().REAL_MEMORY_HEAP_SIZE); // done
    patch::Nop(0x6055A5, 6); // done
    patch::Nop(0x60558E, 10); // done
    patch::SetUInt(0x6055C4 + 1, settings().TEXTURE_MEMORY_SIZE); // done
    patch::SetUInt(0x605598 + 1, settings().TEXTURE_MEMORY_SIZE); // done

    // adboards 16
    //patch::SetUInt(0x1433B6E + 4, 16); // done

    if (useCustomHeadlods) {
        strcpy(HeadLodFormat1, "m46__%d.o");
        strcpy(HeadLodFormat2, "m47__%d.o");
    }
}
