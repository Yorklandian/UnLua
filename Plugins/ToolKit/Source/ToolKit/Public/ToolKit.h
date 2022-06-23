// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <CoreUObject.h>
#include <Modules/ModuleManager.h>

#if TOOLKIT_SUPPORT_UNLUA
#include <UnLuaEx.h>
#include <UnLuaDelegates.h>
#endif

#include <G6ToolKitInterface.h>

DECLARE_LOG_CATEGORY_EXTERN(LogToolKit, Log, All);

class TOOLKIT_API FToolKitModule : public IModuleInterface, public G6ToolKit::IG6ToolKitCallback
{
public:
	FToolKitModule();

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual bool Tick(float DeltaTime);

	virtual void OnToolKitEvent(G6ToolKit::ToolKit_Event Event, const int Arg1, const std::string Arg2, void* const Arg3 = NULL) override;

	// Enable ToolKit, reading configuration from `Config/DefaultToolKitConfig.ini`.
	void EnableToolKit(G6ToolKit::IG6ToolKitCallback* Callback);

	// Enable ToolKit with arguments.
	void EnableToolKitWithArgs(G6ToolKit::IG6ToolKitCallback* Callback, FString GameId, FString GameKey, FString ProxyServerUrl, FString DisplayName,
		FString DisplayCategory, bool EnableToolKitLog = false, G6ToolKit::ToolKit_LOG_PRIORITY ToolKitLogLevel = G6ToolKit::ToolKit_LOG_PRIORITY::ToolKit_LOG_Pri_None);

	// Disable ToolKit.
	void DisableToolKit();

	// Set lua environment for debugger and profiler.
	void AttachLuaState(void* L);

	// Reset lua environment for debugger and profiler.
	// This function must be called once the lua state is closed.
	void DetachLuaState();

#if TOOLKIT_SUPPORT_UNLUA
	static void EnableToolKitStatic();
	static void DisableToolKitStatic();
#endif

private:
	// Establish network connection to Proxy Server.
	void ConnectToProxyServer();

private:
	FTickerDelegate TickDelegate;
	FDelegateHandle TickDelegateHandle;

	// If our module is enabled.
	bool bIsEnabled;
	// If connecting to network.
	bool bIsConnecting;
	// If connected to network.
	bool bIsConnected;
	// If needed to reconnect to network.
	bool bToReconnect;

	// Current lua environment.
	void* MainLuaState;

#if TOOLKIT_SUPPORT_UNLUA
	// delegate handle for UnLua 
	FDelegateHandle LuaInitializeDelegateHandle;
	FDelegateHandle LuaShutDownDelegateHandle;
#endif
};

#if TOOLKIT_SUPPORT_UNLUA
BEGIN_EXPORT_CLASS(FToolKitModule)
ADD_STATIC_FUNCTION(EnableToolKitStatic)
ADD_STATIC_FUNCTION(DisableToolKitStatic)
END_EXPORT_CLASS(FToolKitModule)
#endif