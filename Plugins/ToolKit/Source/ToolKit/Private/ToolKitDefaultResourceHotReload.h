// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if TOOLKIT_SUPPORT_DEFAULT_RESOURCE_HOT_RELOAD
#include <CoreMinimal.h>

namespace FToolKitResourceHotReload
{

    FString GetResourceStoredPath();

    // Prepare for loading resource uploaded from remote.
    void PrepareResourceLoad();

}
#endif