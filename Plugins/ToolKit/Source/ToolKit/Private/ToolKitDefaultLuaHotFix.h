// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if TOOLKIT_SUPPORT_DEFAULT_LUA_HOT_FIX
#include <CoreMinimal.h>

namespace FToolKitLuaHotFix
{

    void SetLuaState(void* L);

    // This function should be called at every frame to process received files.
    void Tick();

    FString GetLuaScriptStoredPath();

    // This function should be called once received a new file.
    void ReceiveNewLuaFile(FString AbsFilePath);

}
#endif