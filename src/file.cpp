#ifdef SAVEPOINT_SQLITE3
#include <sqlite3.h>
#endif

#include <savepoint/file.hpp>
#include <savepoint/log.hpp>

#include <cstdint>
#include <format>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <vector>

bool SavepointFile::Write(const void* data, int size) const
{
#ifdef SAVEPOINT_SQLITE3
    sqlite3_vfs* vfs = sqlite3_vfs_find(nullptr);
    if (!vfs)
    {
        SavepointLog("Failed to find the default VFS");
        return false;
    }
    std::string fullPath(vfs->mxPathname + 1, '\0');
    if (vfs->xFullPathname(vfs, Path.c_str(), int(fullPath.size()), fullPath.data()) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to get the full path: {}", Path));
        return false;
    }
    std::vector<uint8_t> storage(vfs->szOsFile);
    sqlite3_file* file = reinterpret_cast<sqlite3_file*>(storage.data());
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MAIN_DB;
    if (vfs->xOpen(vfs, fullPath.c_str(), file, flags, &flags) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to open file: {}", Path));
        if (file->pMethods)
        {
            file->pMethods->xClose(file);
        }
        return false;
    }
    bool success = false;
    if (file->pMethods->xTruncate(file, 0) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to truncate file: {}", Path));
    }
    else if (size > 0 && file->pMethods->xWrite(file, data, size, 0) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to write file: {}", Path));
    }
    else if (file->pMethods->xSync(file, SQLITE_SYNC_FULL) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to sync file: {}", Path));
    }
    else
    {
        success = true;
    }
    file->pMethods->xClose(file);
    return success;
#else
    std::ofstream stream{Path, std::ios::binary | std::ios::trunc};
    stream.write(static_cast<const char*>(data), size);
    stream.close();
    if (!stream)
    {
        SavepointLog(std::format("Failed to write file: {}", Path));
        return false;
    }
    return true;
#endif
}
