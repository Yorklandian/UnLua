#pragma once

#if TOOLKIT_SUPPORT_UNLUA
#include <CoreMinimal.h>
#include <UnLuaEx.h>
#include <UnLuaDelegates.h>

#include <G6ToolKitDebugInterface.h>

namespace FToolKitUnLuaExtension
{

    using G6ToolKit::Variable;

    void OnLuaStateCreated(lua_State* L);

    void OnPreLuaContextCleanup(bool bFullClean);

    void UnLuaUserdataEvaluator(lua_State* L, Variable* LuaValue, int32 Index, int32 Depth);

}
#endif