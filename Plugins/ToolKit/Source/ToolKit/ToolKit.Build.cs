// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ToolKit : ModuleRules
{
    public ToolKit(ReadOnlyTargetRules Target) : base(Target)
    {
        bEnforceIWYU = false;

        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        ShadowVariableWarningLevel = WarningLevel.Off;
        bEnableUndefinedIdentifierWarnings = false;

        PrivateIncludePaths.AddRange(
            new string[]
            {
                "ToolKit/Private"
            }
        );

        PublicIncludePathModuleNames.AddRange(
            new string[]
            {
                "ApplicationCore",
                "toolkitlib"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Lua", 
                "Projects",
                "toolkitlib"
            }
        );
        
        // ===== Customizable Variables =====

        // Whether to print internal debug log.
        bool bToolKitPrintInternalLog = false;
        // Whether to send UE log to IDE.
        bool bToolKitRedirectEngineLog = true;
        // Whether to reconnect automatically if network breaks.
        bool bToolKitAutoReconnect = false;
        // Whether to enable UnLua-related features.
        bool bToolKitSupportUnLua = true;
        // Whether to enable the default resource hot-reload mechanism.
        bool bToolKitSupportDefaultResourceHotReload = false;
        // Whether to enable the default lua hot-fix mechanism.
        bool bToolKitSupportDefaultLuaHotFix = false;

        if (bToolKitPrintInternalLog)
        {
            PublicDefinitions.Add("TOOLKIT_PRINT_INTERNAL_LOG=1");
        }
        else
        {
            PublicDefinitions.Add("TOOLKIT_PRINT_INTERNAL_LOG=0");
        }

        if (bToolKitRedirectEngineLog)
        {
            PublicDefinitions.Add("TOOLKIT_REDIRECT_ENGINE_LOG=1");
        }
        else
        {
            PublicDefinitions.Add("TOOLKIT_REDIRECT_ENGINE_LOG=0");
        }

        if (bToolKitAutoReconnect)
        {
            PublicDefinitions.Add("TOOLKIT_AUTO_RECONNECT=1");
        }
        else
        {
            PublicDefinitions.Add("TOOLKIT_AUTO_RECONNECT=0");
        }

        if (bToolKitSupportUnLua)
        {
            // It is necessary to add `UnLua` to the plugin dependency list in `ToolKit.uplugin`.
            // Otherwise, a UHT error may occur.
            PublicDefinitions.Add("TOOLKIT_SUPPORT_UNLUA=1");
            PrivateDependencyModuleNames.Add("UnLua");
        }
        else
        {
            PublicDefinitions.Add("TOOLKIT_SUPPORT_UNLUA=0");
        }

        if (bToolKitSupportDefaultResourceHotReload)
        {
            PublicDefinitions.Add("TOOLKIT_SUPPORT_DEFAULT_RESOURCE_HOT_RELOAD=1");
        }
        else
        {
            PublicDefinitions.Add("TOOLKIT_SUPPORT_DEFAULT_RESOURCE_HOT_RELOAD=0");
        }

        if (bToolKitSupportDefaultLuaHotFix)
        {
            PublicDefinitions.Add("TOOLKIT_SUPPORT_DEFAULT_LUA_HOT_FIX=1");
        }
        else
        {
            PublicDefinitions.Add("TOOLKIT_SUPPORT_DEFAULT_LUA_HOT_FIX=0");
        }

        // ===== Customizable Variables END =====
    }
}