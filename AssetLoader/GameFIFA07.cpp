#include "plugin.h"
#include "MyFilesystem.h"
#include "CustomCallbacks.h"
#include "MySettings.h"
#include "MyLog.h"

using namespace plugin;
using namespace std;
using namespace MyFilesystem;

namespace fifa07 {
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
    unsigned char NewModelCollections[0x120 * 1024];
    unsigned char NewTextureCollections[(0x10C + (MAX_TEXTURES_IN_FSH - 64) * 4) * MAX_TEX_COLLECTIONS];
    unsigned char NewTexInfos[0x11C * MAX_TEX_INFOS];
    unsigned char NewTexDynamicDataPtrs[4 * MAX_TEX_DYNAMIC_DATA_PTRS];
    unsigned int NewUsedTags[MAX_TEXTURES_IN_FSH];

    int GetFileSize(std::string const &filename) {
        return CallMethodAndReturn<int, 0x5AF8F0>(*(void **)0xA0BD20, filename.c_str());
    }

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
        return currentSection - (unsigned char *)shp + CallAndReturn<unsigned int, 0x7A946F>(currentSection); // SHAPE_size()
    }

    int OnSHAPE_createsize(int width, int height, int bpp, int clutBpp, int mipmaps, int commentSize, int metalBinSize) {
        int size = CallAndReturn<int, 0x7A6600>(width, height, bpp, clutBpp, mipmaps, commentSize, metalBinSize);
        cSGR_Memory *pool = *(cSGR_Memory **)0xA71438;
        Log(Format("SHAPE_createsize %dx%d %d-bit (%d) %d mipmaps (%d,%d) total %d, pool: start %X end %X free %X used %d size %d, totaltex: %u\n",
            width, height, bpp, clutBpp, mipmaps, commentSize, metalBinSize, size,
            pool->m_pStart, pool->m_pEnd, pool->m_pFree, pool->m_nBytesAllocated, pool->m_pEnd - pool->m_pStart,
            *(unsigned int *)0xA71434));
        return size;
    }

    void OnDeleteTexture(SGR_Texture *tex) {
        SHAPE shp = {};
        bool hasShape = tex->pShape;
        unsigned int shapeSize = 0;
        if (hasShape) {
            memcpy(&shp, tex->pShape, sizeof(SHAPE));
            shapeSize = sgrtex_cloneSize(tex->pShape);
        }
        Call<0x5DE7D0>(tex);
        cSGR_Memory *pool = *(cSGR_Memory **)0xA71438;
        if (!hasShape) {
            Log(Format("deleteTexture (empty), pool: start %X end %X free %X used %d size %d, totaltex: %u\n",
                pool->m_pStart, pool->m_pEnd, pool->m_pFree, pool->m_nBytesAllocated, pool->m_pEnd - pool->m_pStart,
                *(unsigned int *)0xA71434));
        }
        else {
            char texName[5];
            memcpy(texName, &tex->nMaterialID, 4);
            texName[4] = '\0';
            Log(Format("deleteTexture '%s' (%s) %dx%d (%X) %d mipmaps total %d, pool: start %X end %X free %X used %d size %d, totaltex: %u\n",
                texName, tex->pName ? tex->pName : "",
                shp.width, shp.height, shp.type, shp.mipmaps, shapeSize,
                pool->m_pStart, pool->m_pEnd, pool->m_pFree, pool->m_nBytesAllocated, pool->m_pEnd - pool->m_pStart,
                *(unsigned int *)0xA71434));
        }
    }

    SHAPE *OnCloneShape(SHAPE *pShape) {
        SHAPE *result = CallAndReturn<SHAPE *, 0x5DD620>(pShape);
        cSGR_Memory *pool = *(cSGR_Memory **)0xA71438;
        Log(Format("cloneShape %dx%d (%X) %d mipmaps total %d, pool: start %X end %X free %X used %d size %d, totaltex: %u\n",
            pShape->width, pShape->height, pShape->type, pShape->mipmaps, sgrtex_cloneSize(pShape),
            pool->m_pStart, pool->m_pEnd, pool->m_pFree, pool->m_nBytesAllocated, pool->m_pEnd - pool->m_pStart,
            *(unsigned int *)0xA71434));
        return result;
    }

    void OnTextureCopyData(SGR_Texture *tex, SHAPE *srcShape, unsigned int newSize) {
        //SHAPE shp = {};
        //memcpy(&shp, tex->pShape, sizeof(SHAPE));
        //cSGR_Memory *pool = *(cSGR_Memory **)0xA71438;
        unsigned int oldSize = sgrtex_cloneSize(tex->pShape);
        //Log(Format("TextureCopyData before %dx%d (%X) %d mipmaps total %d/%d, pool: start %X end %X free %X used %d size %d, totaltex: %u\n",
        //    shp.width, shp.height, shp.type, shp.mipmaps, oldSize, newSize,
        //    pool->m_pStart, pool->m_pEnd, pool->m_pFree, pool->m_nBytesAllocated, pool->m_pEnd - pool->m_pStart,
        //    *(unsigned int *)0xA71434));
        if (oldSize < newSize) {
            Call<0x5DE7D0>(tex); // deleteTexture()
            Call<0x5DE5F0>(); // unbindTARs()
            tex->pShape = CallAndReturn<SHAPE *, 0x5DD620>(srcShape); // cloneShape()
            tex->bValid = true;
            *(unsigned int *)0xA71434 += 1; // gnNumTextures
            //memcpy(&shp, tex->pShape, sizeof(SHAPE));
        }
        else {
            Call<0x5DE5F0>(); // unbindTARs()
            Call<0x79C672>(tex->pShape, srcShape, newSize); // MemCpy()
            //memcpy(&shp, tex->pShape, sizeof(SHAPE));
        }
        Call<0x5DDA30>(); // rebindTARs()
        //Log(Format("TextureCopyData after %dx%d (%X) %d mipmaps total %d/%d, pool: start %X end %X free %X used %d size %d, totaltex: %u\n",
        //    shp.width, shp.height, shp.type, shp.mipmaps, oldSize, newSize,
        //    pool->m_pStart, pool->m_pEnd, pool->m_pFree, pool->m_nBytesAllocated, pool->m_pEnd - pool->m_pStart,
        //    *(unsigned int *)0xA71434));
    }

    void *OnNewModelUserData(bool isOrd, unsigned char zero, void *sceneEntry, void *block, unsigned int fileSize, const char *fileName) {
        void *oldData = *raw_ptr<void *>(block, 0x50); // block.data
        void *modelCollection = nullptr;
        void *mc = useCustomHeadlods ? NewModelCollections : (void *)0xA0C308; // ModelCollections
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
                                *raw_ptr<int *>(modelBlock, 0x44) = &PLAYER_BODY_DRAWSTATUS[playerIndex];
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
        int playerIndex = ((int)desc - 0x9E8F90) / 0x11D0;
        if (playerIndex >= 0 && playerIndex < NUM_PLAYERS) {
            PLAYER_BODY_DRAWSTATUS[playerIndex] = 1;
            unsigned int playerid = *raw_ptr<unsigned int>(desc, 0xFF4);
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
                //Log(Format("Player %s id %d hair %d headlod %d\n", raw_ptr<char>(desc, 0xFB0), playerid, hairtypeid, PLAYER_HEADLOD1_ID[playerIndex]));
            }
        }
    }

    void OnSetupPlayerModelBE(void *desc) {
        OnSetupPlayerModel(desc);
        Call<0x59BDC0>(desc);
    }

    void OnSetupPlayerModelFE(int a, int playerid) {
        Call<0x586BD0>(a, playerid);
        OnSetupPlayerModel((void *)0x9E8F90);
    }

    void OnSetupPlayerModelFE_CreatePlayer(int a) {
        PLAYER_BODY_DRAWSTATUS[0] = 1;
        Call<0x5868E0>(a);
    }

    void OnLoadTexturesToTexCollection(const char *poolName, int numFshs, void *fshCollection, char bReload, unsigned int maxTextures, unsigned int *outNumTextures, void *texCollection, void *sceneGroup) {
        Call<0x5DEA20>(poolName, numFshs, fshCollection, bReload, MAX_TEXTURES_IN_FSH, outNumTextures, texCollection, sceneGroup);
    }
}

void Install_FIFA07() {
    using namespace fifa07;
    patch::SetUInt(0x5B19CF + 1, settings().FAT_MAX_ENTRIES); // fat fix
    patch::RedirectCall(0x5DEF5D, OnTextureCopyData);
    patch::SetUShort(0x5DEF58, 0xD68B); // mov edx, [esi] => mov edx, esi
    patch::Nop(0x5DEF4F, 5); // unbindTars()
    patch::Nop(0x5DEF65, 5); // rebindTars()
    patch::RedirectCall(0x5E2DA4, OnNewModelUserData);
    useCustomHeadlods = true;
    if (useCustomHeadlods) {
        patch::RedirectCall(0x580744, OnResolveScene);
        patch::RedirectCall(0x587BAF, OnResolveScene);
        patch::RedirectCall(0x5A977E, OnSetupPlayerModelBE);
        patch::RedirectCall(0x5A90D9, OnSetupPlayerModelBE);
        patch::RedirectCall(0x587DC9, OnSetupPlayerModelFE);
        patch::RedirectCall(0x587DDB, OnSetupPlayerModelFE_CreatePlayer);
    }
    // player id 200000
    unsigned int NEW_GEN_PLAYER_ID = 499'000;
    patch::SetUInt(0x4870F2 + 1, NEW_GEN_PLAYER_ID + 999 /*200'000*/);
    patch::SetUInt(0x487110 + 1, NEW_GEN_PLAYER_ID);
    patch::SetUInt(0x49DC72 + 1, NEW_GEN_PLAYER_ID);
    patch::SetUInt(0x49DC8F + 1, NEW_GEN_PLAYER_ID + 484);
    patch::SetUInt(0x49DDF9 + 2, NEW_GEN_PLAYER_ID - 1);
    patch::SetUInt(0x49E00A + 2, NEW_GEN_PLAYER_ID + 484);
    patch::SetUInt(0x4C9514 + 1, NEW_GEN_PLAYER_ID);
    patch::SetUInt(0x4C9885 + 1, NEW_GEN_PLAYER_ID);
    patch::SetUInt(0x4F9257 + 2, NEW_GEN_PLAYER_ID);
    patch::SetUInt(0x50A5BE + 1, NEW_GEN_PLAYER_ID + 16);
    patch::SetUInt(0x50A5D9 + 1, NEW_GEN_PLAYER_ID);
    patch::SetUInt(0x587AD0 + 2, NEW_GEN_PLAYER_ID);
    
    if (useCustomHeadlods) {
        patch::SetPointer(0x5CE632 + 1, NewModelCollections + 8);
        patch::SetPointer(0x5CE642 + 1, NewModelCollections + std::size(NewModelCollections) + 8);
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
    patch::SetUInt(0x8C56F0, settings().SGR_TEXTURE_POOL_SIZE_FE);
    patch::SetUInt(0x8C56F4, settings().SGR_TEXTURE_POOL_SIZE_BE);
    patch::SetUInt(0x5F113A + 6, settings().REAL_MEMORY_HEAP_SIZE);
    patch::Nop(0x5F1165, 6);
    patch::Nop(0x5F114E, 10);
    patch::SetUInt(0x5F1184 + 1, settings().TEXTURE_MEMORY_SIZE);
    patch::SetUInt(0x5F1158 + 1, settings().TEXTURE_MEMORY_SIZE);

    //patch::RedirectCall(0x5DD500, OnSHAPE_createsize);
    //patch::RedirectCall(0x5DEEEA, OnDeleteTexture);
    //patch::RedirectCall(0x5DEF82, OnCloneShape);
    //patch::RedirectCall(0x5DF030, OnCloneShape);

    if (useCustomHeadlods) {
        strcpy(HeadLodFormat1, "m46__%d.o");
        strcpy(HeadLodFormat2, "m47__%d.o");
    }
}
