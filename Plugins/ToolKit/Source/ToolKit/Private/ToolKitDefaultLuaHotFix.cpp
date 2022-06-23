// Copyright Epic Games, Inc. All Rights Reserved.

#if TOOLKIT_SUPPORT_DEFAULT_LUA_HOT_FIX
#include "ToolKitDefaultLuaHotFix.h"

#include <CoreMinimal.h>
#include <GenericPlatform/GenericPlatformMisc.h>
#include <HAL/PlatformOutputDevices.h>
#include <Misc/App.h>
#include <Misc/Parse.h>
#include <Misc/Paths.h>
#include <Modules/ModuleManager.h>
#include <RenderCore.h>

#include <lua.hpp>

#include "ToolKit.h"

namespace FToolKitLuaHotFix
{

    // Pointer to lua main state.
    static lua_State* MainLuaState = NULL;

    // List of lua script files.
    static TArray<FString> LuaFiles;
    // Mutex lock for lua file list.
    static FCriticalSection MutexLuaFiles;

    static void ProcessLuaFiles()
    {
        if (MainLuaState == NULL)
            return;

        MutexLuaFiles.Lock();
        if (LuaFiles.Num() != 0)
        {
            char RunChunk[1024] = { 0 };
            snprintf(RunChunk, sizeof(RunChunk), 
R"(if G6HotFix and G6HotFix.HotFixModifyFile then
    pcall(G6HotFix.HotFixModifyFile, true)
end)"
            );

            if (luaL_dostring(MainLuaState, RunChunk) != LUA_OK)
            {
                const char* ErrStr = lua_tostring(MainLuaState, -1);
                UE_LOG(LogToolKit, Error, TEXT("Failed to hotfix %d lua files: %s."), LuaFiles.Num(), ErrStr);
            }
            LuaFiles.Empty();
        }
        MutexLuaFiles.Unlock();
    }

    void SetLuaState(void* L)
    {
        MainLuaState = (lua_State*)L;
    }

    void Tick()
    {
        ProcessLuaFiles();
    }

    FString GetLuaScriptStoredPath()
    {
        static FString StoredPath = FString("");

        if (StoredPath.IsEmpty())
        {
            StoredPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FPaths::Combine(
                FPaths::ProjectPersistentDownloadDir(), TEXT("Content"), TEXT("Script")));

            // Create parent directory for the path to hotfix.
            if (!FPaths::DirectoryExists(FPaths::Combine(FPaths::ProjectPersistentDownloadDir(), TEXT("Content"))))
            {
                FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::Combine(
                    FPaths::ProjectPersistentDownloadDir(), TEXT("Content")));
            }
        }
        return StoredPath;
    }

    void ReceiveNewLuaFile(FString AbsFilePath)
    {
        MutexLuaFiles.Lock();
        LuaFiles.Add(AbsFilePath);
        MutexLuaFiles.Unlock();
    }

}
#endif