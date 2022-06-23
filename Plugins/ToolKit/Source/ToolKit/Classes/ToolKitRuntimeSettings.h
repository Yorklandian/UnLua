// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <UObject/Object.h>
#include <UObject/ObjectMacros.h>

#if WITH_EDITOR
#include <ISettingsModule.h>
#endif

#include "ToolKitRuntimeSettings.generated.h"

UCLASS(config=ToolKitConfig, defaultconfig)
class TOOLKIT_API UToolKitRuntimeSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	// set game id
	UPROPERTY(EditAnywhere, config, Category=Settings)
	FString GameId;

	// set game key
	UPROPERTY(EditAnywhere, config, Category=Settings)
	FString GameKey;

	// set url of proxy server
	UPROPERTY(EditAnywhere, config, Category=Settings)
	FString ProxyServerUrl;

	// set flag for auto start
	UPROPERTY(EditAnywhere, config, Category=Settings)
	bool bAutoStart;
};