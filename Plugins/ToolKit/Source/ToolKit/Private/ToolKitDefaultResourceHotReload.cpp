// Copyright Epic Games, Inc. All Rights Reserved.

#if TOOLKIT_SUPPORT_DEFAULT_RESOURCE_HOT_RELOAD
#include "ToolKitDefaultResourceHotReload.h"

#include "ToolKit.h"

namespace FToolKitResourceHotReload
{

    // Path for storing uploaded resources.
    static FString ResDownloadRootPath = FString("");
    // Path for uploading resource.
    // We first copy files from `ResDownloadRootPath` to `ResTempPath`.
    // Then files in `ResDownloadRootPath` can be replaced by IDE.
    static FString ResTempPath = FString("");

    FString GetResourceStoredPath()
    {
        return ResDownloadRootPath;
    }

    void PrepareResourceLoad()
    {
        ResDownloadRootPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FPaths::Combine(
            FPaths::ProjectPersistentDownloadDir(), TEXT("ToolKitResourceDir")));
        ResTempPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FPaths::Combine(
            FPaths::ProjectPersistentDownloadDir(), TEXT("ToolKitTempDir")));

        if (!FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*ResDownloadRootPath))
        {
            FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*ResDownloadRootPath);
        }

        if (FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*ResTempPath))
        {
            FPlatformFileManager::Get().GetPlatformFile().DeleteDirectory(*ResTempPath);
        }
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*ResTempPath);

        // Mount *.pak file in `ToolKitResourceDir`.
        if (FCoreDelegates::OnMountPak.IsBound())
        {
            TArray<FString> DirectoryNames;
            IFileManager::Get().FindFiles(DirectoryNames, *FPaths::Combine(ResDownloadRootPath, TEXT("*.pak")), true, false);

            for (FString& Name : DirectoryNames)
            {
                UE_LOG(LogToolKit, Display, TEXT("ToolKit's trying to mount %s."), Name.GetCharArray().GetData());

                FPlatformFileManager::Get().GetPlatformFile().CopyFile(*FPaths::Combine(ResTempPath, Name), *FPaths::Combine(ResDownloadRootPath, Name));
                
                // Give +200 priority.
                if (FCoreDelegates::OnMountPak.Execute(FPaths::Combine(ResTempPath, Name), 200, NULL))
                {
                    UE_LOG(LogToolKit, Display, TEXT("`FCoreDelegates::OnMountPak.Execute` is successful."));
                }
                else
                {
                    UE_LOG(LogToolKit, Display, TEXT("FCoreDelegates::OnMountPak.Execute` is failed."));
                }
            }
        }
    }

}
#endif