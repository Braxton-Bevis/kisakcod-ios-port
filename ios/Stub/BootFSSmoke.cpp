// Phase 3 Wave 1: drive the real filesystem startup and earn a marker only
// after its iOS sandbox paths and real stdio-backed file operations pass.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include <ios/sys_ios.h>

const char *Dvar_GetString(const char *dvarName);

void FS_iOS_SetHeadlessNoAssets(bool enabled);
bool FS_iOS_HeadlessNoAssetsActive();
void FS_InitFilesystem();
bool FS_Initialized();
int FS_WriteFile(char *filename, char *buffer, uint32_t size);
int FS_ReadFile(const char *qpath, void **buffer);
void FS_FreeFile(char *buffer);
bool FS_Delete(const char *filename);

static bool s_fsSmokeSucceeded;

extern "C" const char *kisak_fs_smoke(void)
{
    static char status[512];
    static char filename[] = "bmk4_wave1_fs.tmp";
    static char payload[] = "BMK4 Wave 1 filesystem round trip";
    void *readBuffer = nullptr;

    // This opt-in is deliberately separate from an empty sandbox. The real
    // FS validation bypass is active only while useFastFile is also disabled.
    FS_iOS_SetHeadlessNoAssets(true);
    if (!FS_iOS_HeadlessNoAssetsActive())
    {
        snprintf(status, sizeof(status), "filesystem FAIL: headless/no-fastfile policy inactive");
        return status;
    }

    FS_InitFilesystem();
    if (!FS_Initialized())
    {
        snprintf(status, sizeof(status), "filesystem FAIL: FS_Initialized false");
        return status;
    }

    const char *basePath = Dvar_GetString("fs_basepath");
    const char *homePath = Dvar_GetString("fs_homepath");
    if (!basePath || strcmp(basePath, Sys_iOS_BundlePath()) != 0)
    {
        snprintf(status, sizeof(status), "filesystem FAIL: fs_basepath is not bundle");
        return status;
    }
    if (!homePath || strcmp(homePath, Sys_iOS_DocumentsPath()) != 0)
    {
        snprintf(status, sizeof(status), "filesystem FAIL: fs_homepath is not Documents");
        return status;
    }
    if (FS_ReadFile("fileSysCheck.cfg", nullptr) > 0)
    {
        snprintf(status, sizeof(status), "filesystem FAIL: asset sentinel unexpectedly present");
        return status;
    }

    // Ignore a stale failed-run file, then require the engine's own
    // FS_WriteFile/FS_ReadFile/FS_FreeFile/FS_Delete sequence to succeed.
    FS_Delete(filename);
    if (!FS_WriteFile(filename, payload, sizeof(payload)))
    {
        snprintf(status, sizeof(status), "filesystem FAIL: write");
        return status;
    }
    const int bytesRead = FS_ReadFile(filename, &readBuffer);
    if (bytesRead != static_cast<int>(sizeof(payload)) || !readBuffer
        || memcmp(readBuffer, payload, sizeof(payload)) != 0)
    {
        if (readBuffer)
            FS_FreeFile(static_cast<char *>(readBuffer));
        FS_Delete(filename);
        snprintf(status, sizeof(status), "filesystem FAIL: readback");
        return status;
    }
    FS_FreeFile(static_cast<char *>(readBuffer));
    if (!FS_Delete(filename) || FS_ReadFile(filename, nullptr) > 0)
    {
        snprintf(status, sizeof(status), "filesystem FAIL: delete");
        return status;
    }

    s_fsSmokeSucceeded = true;
    snprintf(status, sizeof(status),
             "FS_InitFilesystem OK — bundle base, Documents home, write/read/delete OK, no assets");
    return status;
}

extern "C" bool kisak_fs_smoke_succeeded(void)
{
    return s_fsSmokeSucceeded;
}
