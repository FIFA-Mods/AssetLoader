#include "plugin.h"
#include "MyFilesystem.h"
#include "GfxCoreHook.h"
#include "CustomCallbacks.h"
#include "MyLog.h"
#include <map>

#define ASSET_LOADER_VERSION 101

using namespace plugin;
using namespace std;
using namespace MyFilesystem;

#define ASSETS_DIR "data\\assets"
char const *gAssetPaths[] = { ASSETS_DIR "\\" };

void OnCreateFileIO_FM07_1(void *fileIo) {
    strcpy((char *)fileIo + 4, gAssetPaths[0]);
}

void __declspec(naked) OnCreateFileIO_FM07() {
    __asm {
       //mov dword ptr[esi + 0x824], 1
       //push esi
       //call OnCreateFileIO_FM07_1
       //add esp, 4
        push 1
        lea eax, gAssetPaths
        push eax
        mov ecx, esi
        mov eax, [ecx]
        call [eax + 4]
        pop edi
        mov eax, esi
        pop esi
        pop ebp
        retn
    }
}

void *OnCreateFileIO_FM07_2() {
    void *fileIO = *(void **)GfxCoreAddress(0x5F1078);
    CallVirtualMethod<1>(fileIO, gAssetPaths, 1);
    return CallAndReturnDynGlobal<void *>(GfxCoreAddress(0x1FF580));
}

FIFA::Version &GameVersion() {
    static FIFA::Version version;
    return version;
}

class AssetLoader {
public:
    struct PatchInfo { unsigned int id, editor, gfxHook, gfxFileIo1, gfxFileIo2, strlwrFunc, a1, a2, a3, a4, a5; void(*gameCallback)(); void(*gfxCallback)(); };

    static PatchInfo &GetPatchInfo() {
        static PatchInfo patchInfo;
        return patchInfo;
    }

    static map<string, string> &FilesMap() {
        static map<string, string> filesMap;
        return filesMap;
    }

    struct FindHandle {
        HANDLE h;
        FindHandle(HANDLE hh = INVALID_HANDLE_VALUE) : h(hh) {}
        ~FindHandle() { if (h != INVALID_HANDLE_VALUE) FindClose(h); }
        FindHandle(const FindHandle &) = delete;
        FindHandle &operator=(const FindHandle &) = delete;
        FindHandle(FindHandle &&o) noexcept : h(o.h) { o.h = INVALID_HANDLE_VALUE; }
        FindHandle &operator=(FindHandle &&o) noexcept {
            if (this != &o) { if (h != INVALID_HANDLE_VALUE) FindClose(h); h = o.h; o.h = INVALID_HANDLE_VALUE; }
            return *this;
        }
    };

    static void FindAssets(const std::wstring &rootPath, const std::wstring &startSubpath) {
        std::vector<std::pair<std::wstring, std::wstring>> stack;
        stack.emplace_back(rootPath, startSubpath);
        while (!stack.empty()) {
            auto [dirPath, subpath] = stack.back();
            stack.pop_back();
            std::wstring pattern = dirPath + L"\\*";
            WIN32_FIND_DATAW fd;
            FindHandle fh(FindFirstFileW(pattern.c_str(), &fd));
            if (fh.h == INVALID_HANDLE_VALUE) {
                DWORD err = GetLastError();
                if (err != ERROR_FILE_NOT_FOUND)
                    Log("FindFirstFileW error: " + Format("%08X", err) + "\n");
                continue;
            }
            do {
                const wchar_t *name = fd.cFileName;
                if (wcscmp(name, L".") == 0 || wcscmp(name, L"..") == 0) continue;
                std::wstring entryFullPath = dirPath + L"\\" + name;
                bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                bool isReparse = (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
                if (isDir) {
                    if (isReparse)
                        continue;
                    std::wstring newSub = subpath.empty() ? std::wstring(name) : (subpath + L"\\" + name);
                    stack.emplace_back(entryFullPath, newSub);
                }
                else {
                    std::string fileName = WtoA(name);
                    if (FilesMap().find(fileName) == FilesMap().end()) {
                        FilesMap()[fileName] = WtoA(subpath);
                        Log("Asset: " + fileName + " -> " + FilesMap()[fileName] + "\n");
                    }
                }
            } while (FindNextFileW(fh.h, &fd) != 0);
            DWORD le = GetLastError();
            if (le != ERROR_NO_MORE_FILES)
                Log("FindNextFileW error: " + Format("%08X", le) + "\n");
        }
    }

    static char *OnFileNameGet(char *srcFilePath) {
        //::Warning(srcFilePath);
        if (strlen(srcFilePath) <= 1 || srcFilePath[1] != ':') {
            if (FilesMap().find(srcFilePath) != FilesMap().end()) {
                std::string assetFileName = ASSETS_DIR "\\";
                std::string const &subPath = FilesMap()[srcFilePath];
                if (!subPath.empty())
                    assetFileName += subPath + "\\";
                assetFileName += srcFilePath;
                Log(std::string("Request: ") + srcFilePath + " -> " + assetFileName + "\n");
                strcpy(srcFilePath, assetFileName.c_str());
                //::Warning("ASSET %s", srcFilePath);
            }
        }
        _strlwr(srcFilePath);
        return srcFilePath;
    }

    static void METHOD OnFileIoSetupPaths(void *fileIO, DUMMY_ARG, const char **paths, unsigned int numPaths) {
        if ((GetPatchInfo().id == ID_FM_05_1000_C || GetPatchInfo().id == ID_FM_05_1010_C) && *raw_ptr<unsigned int>(fileIO, 0x824) != 0)
            return;
        std::vector<const char *> myPaths;
        if (GetPatchInfo().id != ID_FM_04_1000_C) {
            if (numPaths > 7)
                numPaths = 7;
        }
        for (unsigned int i = 0; i < numPaths; i++) {
            if (GetPatchInfo().id != ID_FM_04_1000_C || strlen(paths[i]) > 3)
                myPaths.push_back(paths[i]);
        }
        if (GetPatchInfo().id != ID_FM_04_1000_C || myPaths.size() < 3)
            myPaths.push_back(ASSETS_DIR "\\");
        CallMethodDynGlobal(GfxCoreAddress(GetPatchInfo().gfxFileIo2), fileIO, myPaths.data(), myPaths.size());
    }

    static void *METHOD OnCreateFileIO(void *fileIO) {
        CallMethodDynGlobal(GfxCoreAddress(GetPatchInfo().gfxFileIo2), fileIO);
        if (GetPatchInfo().id == ID_ED_04_1016 || GetPatchInfo().id == ID_ED_04_1020)
            CallVirtualMethod<0>(fileIO, gAssetPaths, 1);
        else
            CallVirtualMethod<1>(fileIO, gAssetPaths, 1);
        return fileIO;
    }

    static void GfxCoreCallback() {
        if (GetPatchInfo().strlwrFunc != 0)
            patch::RedirectJump(GfxCoreAddress(GetPatchInfo().strlwrFunc), OnFileNameGet);
        if (GetPatchInfo().a1 != 0)
            patch::Nop(GfxCoreAddress(GetPatchInfo().a1), 2);
        if (GetPatchInfo().a2 != 0)
            patch::Nop(GfxCoreAddress(GetPatchInfo().a2), 2);
        if (GetPatchInfo().a3 != 0)
            patch::Nop(GfxCoreAddress(GetPatchInfo().a3), 2);
        if (GetPatchInfo().a4 != 0)
            patch::Nop(GfxCoreAddress(GetPatchInfo().a4), 6);
        if (GetPatchInfo().a5 != 0)
            patch::Nop(GfxCoreAddress(GetPatchInfo().a5), 6);
        if (GetPatchInfo().gfxFileIo1 != 0) {
            if (GetPatchInfo().editor != 0) {
                patch::RedirectCall(GfxCoreAddress(GetPatchInfo().gfxFileIo1), OnCreateFileIO);
                if (GetPatchInfo().id == ID_ED_05_4000)
                    patch::SetUChar(GfxCoreAddress(0x45733), 0xEB);
            }
            else {
                patch::SetPointer(GfxCoreAddress(GetPatchInfo().gfxFileIo1), OnFileIoSetupPaths);
            }
        }
        if (GetPatchInfo().id == ID_FM_07_1000_C) {
            patch::RedirectCall(GfxCoreAddress(0x2072CA), OnCreateFileIO_FM07_2);
            //patch::RedirectJump(GfxCoreAddress(0x32AC2B), OnCreateFileIO_FM07);
        }
        if (GetPatchInfo().gfxCallback)
            GetPatchInfo().gfxCallback();
    }

    static void Initialize() {
        if (!CheckPluginName(Magic<'A','s','s','e','t','L','o','a','d','e','r','.','a','s','i'>()))
            return;
        static PatchInfo info[] = {
            { ID_CL_04_05_1000_C, 0, 0, 0, 0, 0x72C3FF, 0x5E7BFE, 0, 0, 0x5E8FEE, 0x5E921D, Install_CL0405, nullptr },
            { ID_FIFA_05_1000_C, 0, 0, 0, 0, 0x6E2829, 0, 0x5BEA1E, 0, 0x5BFBAE, 0x5BFDDD, Install_FIFA05, nullptr },
            { ID_FIFA07_1100_RLD, 0, 0, 0, 0, 0x8200ED, 0x5AF89E, 0x5AF91E, 0, 0x5B0ABE, 0x5B0CED, Install_FIFA07, nullptr },
            { ID_WC_06_1000_C, 0, 0, 0, 0, 0x77EBA2, 0x53CD4E, 0, 0, 0x53DF1E, 0x53E14D, Install_WC06, nullptr },
            { ID_FIFA08_1200_VTY, 0, 0, 0, 0, 0x86F35D, 0x5C3E4E, 0x5C3ECE, 0, 0x5C503E, 0x5C527E, Install_FIFA08, nullptr },
            { ID_FIFA08_1200_BFF, 0, 0, 0, 0, 0x86F35D, 0x5C3E4E, 0x5C3ECE, 0, 0x5C503E, 0x5C527E, Install_FIFA08, nullptr },
            { ID_FIFA10_1000_RZR, 0, 0, 0, 0, 0x8E757B, 0x60D3FE, 0x60D47E, 0, 0x60E5CE, 0x60E80E, Install_FIFA10, nullptr },
            { ID_EURO_08_1000_C, 0, 0, 0, 0, 0x80547D, 0x16C85DB, 0x506A30, 0, 0x55AF1E, 0x55B15E, nullptr, nullptr },
            { ID_FM_07_1000_C, 0, 0x4423D6, 0, 0, 0x394F5E, 0x3492DD, 0, 0, 0x34A7E5, 0x34AA13, nullptr, InstallGfx_FM07 },
            { ID_ED_07_7020, 1, 0x48DA50, 0x704FD, 0x85040, 0, 0, 0, 0, 0, 0, nullptr, nullptr },
            { ID_FM_05_1010_C, 0, 0x43B2F6, 0x49961C, 0x3A24F0, 0x3E6E05, 0x39F8AE, 0, 0, 0x3A039E, 0x3A05CD, nullptr, InstallGfx_TCM2005 },
            { ID_ED_05_4000, 1, 0x4AE2D5, 0x109A52, 0x140CA0, 0, 0, 0, 0, 0, 0, nullptr, nullptr },
            { ID_FM_04_1000_C, 0, 0x41860D, 0x431068, 0x357460, 0x389C20, 0x35207E, 0x35232E, 0x353213, 0, 0, nullptr, InstallGfx_TCM2004 },
            { ID_ED_04_1020, 1, 0x4772A5, 0x14FD, 0x62600, 0, 0, 0, 0, 0, 0, nullptr, nullptr },
            { ID_ED_04_1016, 1, 0x4772A5, 0x14FD, 0x62600, 0, 0, 0, 0, 0, 0, nullptr, nullptr }
        };
        auto v = FIFA::GetAppVersion();
        GameVersion() = v;
        if (v.id() == ID_FIFA07_1100_RLD) {
            if (MyFilesystem::exists(FIFA::GameDirPath(L"plugins\\FIFA10KitModel.asi"))) {
                ::Error("FIFA10KitModel.asi plugin is not needed if you're using AssetLoader plugin. "
                        "Please delete FIFA10KitModel.asi file to avoid conflicts with AssetLoader plugin.");
            }
            if (MyFilesystem::exists(FIFA::GameDirPath(L"plugins\\FIFAHDMod.asi"))) {
                ::Error("FIFAHDMod.asi plugin is not needed if you're using AssetLoader plugin. "
                    "Please delete FIFAHDMod.asi file to avoid conflicts with AssetLoader plugin.");
            }
            bool missingFiles = false;
            for (int i = 0; i <= 38; i++) {
                auto headlodPath = FIFA::GameDirPath(L"data\\assets\\genheads\\headlod\\");
                if (!MyFilesystem::exists(headlodPath + Format(L"m46__%d.o", -i))
                    || !MyFilesystem::exists(headlodPath + Format(L"m47__%d.o", -i)))
                {
                    missingFiles = true;
                    break;
                }
            }
            if (missingFiles) {
                ::Error("Missing AssetLoader installation files. Please reinstall AssetLoader.");
                return;
            }
        }
        for (auto const &i : info) {
            if (v.id() == i.id) {
                if (i.gfxHook != 0) {
                    InitGfxCoreHook(i.gfxHook, GfxCoreCallback);
                    GetPatchInfo() = i;
                }
                else {
                    if (i.strlwrFunc != 0)
                        patch::RedirectJump(i.strlwrFunc, OnFileNameGet);
                    if (i.a1 != 0)
                        patch::Nop(i.a1, 2);
                    if (i.a2 != 0)
                        patch::Nop(i.a2, 2);
                    if (i.a3 != 0)
                        patch::Nop(i.a3, 2);
                    if (i.a4 != 0)
                        patch::Nop(i.a4, 6);
                    if (i.a5 != 0)
                        patch::Nop(i.a5, 6);
                }
                if (i.gameCallback)
                    i.gameCallback();
                break;
            }
        }
        FindAssets(FIFA::GameDirPath(AtoW(ASSETS_DIR)), L"");
    }
};

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        OpenLog();
        AssetLoader::Initialize();
        break;
    case DLL_PROCESS_DETACH:
        CloseLog();
        break;
    }
    return TRUE;
}
