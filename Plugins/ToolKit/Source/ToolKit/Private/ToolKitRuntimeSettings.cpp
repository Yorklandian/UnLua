// Copyright Epic Games, Inc. All Rights Reserved.

#include "ToolKitRuntimeSettings.h"

//////////////////////////////////////////////////////////////////////////
// UToolKitRuntimeSettings

UToolKitRuntimeSettings::UToolKitRuntimeSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, GameId("123456")
	, GameKey("abcdef")
	, ProxyServerUrl("127.0.0.1:16688")
	, bAutoStart(true)
{
}