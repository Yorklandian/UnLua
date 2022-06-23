// Copyright Epic Games, Inc. All Rights Reserved.

#include "ToolKit.h"

#include <ctime>

#if PLATFORM_ANDROID || PLATFORM_IOS
#include <sys/time.h>
#include <pthread.h>
#elif PLATFORM_WINDOWS
#include <Windows.h>
#include <Winsock.h>
#include <Windows/AllowWindowsPlatformTypes.h>
#include <Windows/PreWindowsApi.h>
#include <Windows/PostWindowsApi.h>
#include <Windows/HideWindowsPlatformTypes.h>
#endif

#include <CoreMinimal.h>
#include <GenericPlatform/GenericPlatformMisc.h>
#include <HAL/PlatformOutputDevices.h>
#include <Interfaces/IPluginManager.h>
#include <Modules/ModuleManager.h>
#include <Misc/App.h>
#include <Misc/Parse.h>
#include <Misc/Paths.h>
#include <RenderCore.h>

#include <lua.hpp>

#include <G6ToolKitInterface.h>

#if WITH_EDITOR
#include "ToolKitRuntimeSettings.h"
#endif

#if TOOLKIT_SUPPORT_UNLUA
#include "ToolKitUnLuaExtension.h"
#endif

#if TOOLKIT_SUPPORT_DEFAULT_LUA_HOT_FIX
#include "ToolKitDefaultLuaHotFix.h"
#endif

#if TOOLKIT_SUPPORT_DEFAULT_RESOURCE_HOT_RELOAD
#include "ToolKitDefaultResourceHotReload.h"
#endif

#define LOCTEXT_NAMESPACE "ToolKit"

DEFINE_LOG_CATEGORY(LogToolKit);

#if PLATFORM_ANDROID
extern FString GPackageName;
#endif

namespace FToolKitPrivate
{
	static FString GToolKitConfigIni = FPaths::ProjectConfigDir().Append(TEXT("DefaultToolKitConfig.ini"));

	static void RegisterSettings()
	{
#if WITH_EDITOR
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->RegisterSettings("Project", "Plugins", "ToolKit",
				LOCTEXT("TileSetEditorSettingsName", "ToolKit"),
				LOCTEXT("TileSetEditorSettingsDescription", "Configure the ToolKit"),
				GetMutableDefault<UToolKitRuntimeSettings>()
			);
		}
#endif
	}

	static void UnregisterSettings()
	{
#if WITH_EDITOR
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "ToolKit");
		}
#endif
	}

	static FString ToolKitLogPrefix(const FName& Category)
	{
#if NO_LOGGING
		return FString(TEXT(""));
#else
		static constexpr int32 BUFFER_SIZE = 4096;

		char Buffer[BUFFER_SIZE] = { 0 };

		time_t Now;
		time(&Now);

		struct tm LocalTime;
		struct timeval TimeValue;

#if PLATFORM_WINDOWS
		localtime_s(&LocalTime, &Now);

		SYSTEMTIME SysTime;
		GetLocalTime(&SysTime);
		TimeValue.tv_sec = mktime(&LocalTime);
		TimeValue.tv_usec = SysTime.wMilliseconds;
#else
		LocalTime = *localtime(&Now);
		gettimeofday(&TimeValue, 0);
#endif

		if (Category == LogToolKit.GetCategoryName())
		{
			snprintf(Buffer, BUFFER_SIZE, "[%04d-%02d-%02d %02d:%02d:%02d.%03d]",
				LocalTime.tm_year + 1900,
				LocalTime.tm_mon + 1,
				LocalTime.tm_mday,
				LocalTime.tm_hour,
				LocalTime.tm_min,
				LocalTime.tm_sec,
				(int)TimeValue.tv_usec
			);
		}
		else
		{
#if PLATFORM_WINDOWS
			snprintf(Buffer, BUFFER_SIZE, "[%04d-%02d-%02d %02d:%02d:%02d.%03d][%s][%u]",
#else
			snprintf(Buffer, BUFFER_SIZE, "[%04d-%02d-%02d %02d:%02d:%02d.%03d][%s][%lu]",
#endif
				LocalTime.tm_year + 1900,
				LocalTime.tm_mon + 1,
				LocalTime.tm_mday,
				LocalTime.tm_hour,
				LocalTime.tm_min,
				LocalTime.tm_sec,
				(int)TimeValue.tv_usec,
				TCHAR_TO_UTF8(*Category.ToString()),
#if PLATFORM_WINDOWS
				(unsigned int)GetCurrentThreadId()
#else
				(unsigned long)pthread_self()
#endif
			);
		}

		return FString(Buffer);
#endif
	}

}

#if TOOLKIT_REDIRECT_ENGINE_LOG
struct FToolKitOutput : public FOutputDevice
{
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
	{
#if !NO_LOGGING
		if (Verbosity == ELogVerbosity::SetColor)
			return;

		int LogLevel = G6ToolKit::ToolKit_LOG_PRIORITY::ToolKit_LOG_Pri_Trace;
		if (Verbosity == ELogVerbosity::Error)
			LogLevel = G6ToolKit::ToolKit_LOG_PRIORITY::ToolKit_LOG_Pri_Error;
		else if (Verbosity == ELogVerbosity::Warning)
			LogLevel = G6ToolKit::ToolKit_LOG_PRIORITY::ToolKit_LOG_Pri_Warning;
		else if (Verbosity == ELogVerbosity::Display || Verbosity == ELogVerbosity::Log)
			LogLevel = G6ToolKit::ToolKit_LOG_PRIORITY::ToolKit_LOG_Pri_Info;

		FString Message = (Category == LogToolKit.GetCategoryName()) ? "" : FToolKitPrivate::ToolKitLogPrefix(Category);
		Message += V;
		Message += TEXT("\n");

		FTCHARToUTF8 Conv(*Message);
		G6ToolKit::GetG6ToolKitInstance()->SendLogToIDE(Conv.Get(), Conv.Length(), LogLevel);
#endif
	}
};

static FToolKitOutput GToolKitOutput;
#endif

FToolKitModule::FToolKitModule() :
	bIsEnabled(false), bIsConnecting(false), bIsConnected(false), bToReconnect(false), MainLuaState(NULL)
{
	if (!FPaths::FileExists(FToolKitPrivate::GToolKitConfigIni))
	{
		FString TemplateConfigIni = IPluginManager::Get().FindPlugin("ToolKit")->GetBaseDir().Append(TEXT("/Config/DefaultToolKitConfig.ini"));
		FPlatformFileManager::Get().GetPlatformFile().CopyFile(*FToolKitPrivate::GToolKitConfigIni, *TemplateConfigIni);
	}

#if TOOLKIT_REDIRECT_ENGINE_LOG
	if (!GLog->IsRedirectingTo(&GToolKitOutput))
	{
		GLog->AddOutputDevice(&GToolKitOutput);
	}
#endif
}

void FToolKitModule::StartupModule()
{
	UE_LOG(LogToolKit, Log, TEXT("FToolKitModule::StartupModule()"));

	FCoreDelegates::OnFEngineLoopInitComplete.AddStatic(&FToolKitPrivate::RegisterSettings);

	TickDelegate = FTickerDelegate::CreateRaw(this, &FToolKitModule::Tick);
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate);

#if TOOLKIT_SUPPORT_UNLUA
	LuaInitializeDelegateHandle = FUnLuaDelegates::OnLuaStateCreated.AddStatic(&FToolKitUnLuaExtension::OnLuaStateCreated);
	LuaShutDownDelegateHandle = FUnLuaDelegates::OnPreLuaContextCleanup.AddStatic(&FToolKitUnLuaExtension::OnPreLuaContextCleanup);

	G6ToolKit::GetG6ToolKitInstance()->SetUserdataEvaluator(FToolKitUnLuaExtension::UnLuaUserdataEvaluator);
#endif

	bool bAutoStart = false;
#if UE_SERVER
	if (FParse::Param(FCommandLine::Get(), TEXT("EnableToolKit")))
	{
		bAutoStart = true;
	}
#else
	GConfig->GetBool(TEXT("/Script/ToolKit.ToolKitRuntimeSettings"), 
		TEXT("bAutoStart"), bAutoStart, FToolKitPrivate::GToolKitConfigIni);
#endif

	if (bAutoStart)
	{
		EnableToolKit(this);
	}
}

void FToolKitModule::ShutdownModule()
{
	UE_LOG(LogToolKit, Log, TEXT("FToolKitModule::ShutdownModule()"));

	DisableToolKit();

	DetachLuaState();

#if TOOLKIT_SUPPORT_UNLUA
	G6ToolKit::GetG6ToolKitInstance()->SetUserdataEvaluator(NULL);

	FUnLuaDelegates::OnLuaStateCreated.Remove(LuaInitializeDelegateHandle);
	FUnLuaDelegates::OnPreLuaContextCleanup.Remove(LuaShutDownDelegateHandle);
#endif

	FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);

	FToolKitPrivate::UnregisterSettings();
}

bool FToolKitModule::Tick(float DeltaTime)
{
	if (bIsConnected)
	{
		G6ToolKit::GetG6ToolKitInstance()->Tick();
#if TOOLKIT_SUPPORT_DEFAULT_LUA_HOT_FIX
		FToolKitLuaHotFix::Tick();
#endif
	}
	else if (bToReconnect)
	{
		static constexpr double RECONNECT_INTERVAL = 5.0;

		static double PreviousTry = FPlatformTime::Seconds();

		if (FPlatformTime::Seconds() - PreviousTry > RECONNECT_INTERVAL)
		{
			UE_LOG(LogToolKit, Log, TEXT("ToolKit tries connect to Proxy Server."));

			PreviousTry = FPlatformTime::Seconds();
			ConnectToProxyServer();
		}
	}
	return true;
}

void FToolKitModule::OnToolKitEvent(G6ToolKit::ToolKit_Event Event, const int Arg1, const std::string Arg2, void* const Arg3)
{
	switch (Event)
	{
	case G6ToolKit::ToolKit_Event::ToolKit_Event_NativeLog:
	{
#if TOOLKIT_PRINT_INTERNAL_LOG
		if (bIsEnabled)
		{
			UE_LOG(LogToolKit, Log, TEXT("%s %s"), *FToolKitPrivate::ToolKitLogPrefix(TEXT("ToolKit")), UTF8_TO_TCHAR(Arg2.c_str()));
		}
#endif
		break;
	}
	case G6ToolKit::ToolKit_Event::ToolKit_Event_NetworkBreak:
	{
		bIsConnected = false;

		if (bIsEnabled)
		{
			UE_LOG(LogToolKit, Warning, TEXT("ToolKit lost connection to network."));
			bToReconnect = TOOLKIT_AUTO_RECONNECT;
		}
		else
		{
			bToReconnect = false;
		}
		break;
	}
	case G6ToolKit::ToolKit_Event::ToolKit_Event_FileTransfer_NewFile_Recieved:
	{
		UE_LOG(LogToolKit, Log, TEXT("ToolKit received a new file: %s."), UTF8_TO_TCHAR(Arg2.c_str()));

#if TOOLKIT_SUPPORT_DEFAULT_LUA_HOT_FIX
		if (FString(Arg2.c_str()).EndsWith(".lua"))
		{
			// Add File to new lua file list.
			if (FString(Arg2.c_str()).StartsWith(*FToolKitLuaHotFix::GetLuaScriptStoredPath()))
			{
				FToolKitLuaHotFix::ReceiveNewLuaFile(FString(Arg2.c_str()));
			}
		}
#endif
		break;
	}
	}
}

void FToolKitModule::EnableToolKit(G6ToolKit::IG6ToolKitCallback* Callback)
{
	FString GameId, GameKey, ProxyServerUrl;

	GConfig->GetString(TEXT("/Script/ToolKit.ToolKitRuntimeSettings"), 
		TEXT("GameId"), GameId, FToolKitPrivate::GToolKitConfigIni);
	GConfig->GetString(TEXT("/Script/ToolKit.ToolKitRuntimeSettings"), 
		TEXT("GameKey"), GameKey, FToolKitPrivate::GToolKitConfigIni);
	GConfig->GetString(TEXT("/Script/ToolKit.ToolKitRuntimeSettings"), 
		TEXT("ProxyServerUrl"), ProxyServerUrl, FToolKitPrivate::GToolKitConfigIni);

	FString DisplayName = FPlatformProcess::ComputerName();

#if UE_SERVER
	FString DisplayCategory = "ueserver";

	if (FParse::Value(FCommandLine::Get(), TEXT("ToolKitDisplayName"), DisplayName))
	{
		DisplayName = DisplayName.Replace(TEXT("="), TEXT(""));
	}
#else
	FString DisplayCategory = "ueclient";
#endif

#if TOOLKIT_PRINT_INTERNAL_LOG
	bool bEnableToolKitLog = true;
	G6ToolKit::ToolKit_LOG_PRIORITY LogLevel = FPlatformMisc::IsDebuggerPresent() ? 
		G6ToolKit::ToolKit_LOG_PRIORITY::ToolKit_LOG_Pri_None : G6ToolKit::ToolKit_LOG_PRIORITY::ToolKit_LOG_Pri_Debug;
#else
	bool bEnableToolKitLog = false;
	G6ToolKit::ToolKit_LOG_PRIORITY LogLevel = G6ToolKit::ToolKit_LOG_PRIORITY::ToolKit_LOG_Pri_None;
#endif

	EnableToolKitWithArgs(Callback, GameId, GameKey, ProxyServerUrl, DisplayName, DisplayCategory, bEnableToolKitLog, LogLevel);
}

void FToolKitModule::EnableToolKitWithArgs(G6ToolKit::IG6ToolKitCallback* Callback, FString GameId, FString GameKey, FString ProxyServerUrl, FString DisplayName,
	FString DisplayCategory, bool EnableToolKitLog, G6ToolKit::ToolKit_LOG_PRIORITY ToolKitLogLevel)
{
	bIsEnabled = true;

	G6ToolKit::G6ToolKitBaseInfo BaseInfo = {
		TCHAR_TO_UTF8(*GameId), 
		TCHAR_TO_UTF8(*GameKey), 
		TCHAR_TO_UTF8(*ProxyServerUrl), 
		TCHAR_TO_UTF8(*DisplayName), 
		TCHAR_TO_UTF8(*DisplayCategory), 
		EnableToolKitLog, 
		ToolKitLogLevel
	};

	G6ToolKit::GetG6ToolKitInstance()->Init(BaseInfo, Callback);

	UE_LOG(LogToolKit, Log, TEXT(
R"(ToolKit is initialized with: 
    Game Id				= %s
    Game Key			= %s
    Proxy Server Url	= %s
    Display Name		= %s
    Display Category	= %s)"
	), *GameId, *GameKey, *ProxyServerUrl, *DisplayName, *DisplayCategory);

	FString DownloadDir = FPaths::ProjectPersistentDownloadDir();
	G6ToolKit::GetG6ToolKitInstance()->SetFileSystemRootPath("UE4-PersistentDownloadDir", 
		TCHAR_TO_UTF8(*IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*DownloadDir)));

#if TOOLKIT_SUPPORT_UNLUA
	G6ToolKit::GetG6ToolKitInstance()->SetUserdataEvaluator(FToolKitUnLuaExtension::UnLuaUserdataEvaluator);
#endif

#if TOOLKIT_SUPPORT_DEFAULT_RESOURCE_HOT_RELOAD
	FToolKitResourceHotReload::PrepareResourceLoad();
	G6ToolKit::GetG6ToolKitInstance()->SetFileSystemRootPath("Resource Hotreload Path", 
		TCHAR_TO_UTF8(*IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FToolKitResourceHotReload::GetResourceStoredPath())));
#endif

#if TOOLKIT_SUPPORT_DEFAULT_LUA_HOT_FIX
	G6ToolKit::GetG6ToolKitInstance()->SetFileSystemRootPath("Lua Hotfix Path", 
		TCHAR_TO_UTF8(*FToolKitLuaHotFix::GetLuaScriptStoredPath()));
#endif

	ConnectToProxyServer();
}

void FToolKitModule::DisableToolKit()
{
	bIsEnabled = false;
	bIsConnecting = false;
	bIsConnected = false;
	bToReconnect = false;

	G6ToolKit::GetG6ToolKitInstance()->Stop();
}

void FToolKitModule::AttachLuaState(void* L)
{
#if TOOLKIT_SUPPORT_DEFAULT_LUA_HOT_FIX
	FToolKitLuaHotFix::SetLuaState(L);
#endif
	G6ToolKit::GetG6ToolKitInstance()->SetLuaState((lua_State*)L);
	MainLuaState = L;
}

void FToolKitModule::DetachLuaState()
{
#if TOOLKIT_SUPPORT_DEFAULT_LUA_HOT_FIX
	FToolKitLuaHotFix::SetLuaState(NULL);
#endif
	G6ToolKit::GetG6ToolKitInstance()->SetLuaState((lua_State*)NULL);
	MainLuaState = NULL;
}

#if TOOLKIT_SUPPORT_UNLUA
void FToolKitModule::EnableToolKitStatic()
{
	FToolKitModule* ToolKitModuleInstance = FModuleManager::GetModulePtr<FToolKitModule>("ToolKit");

	if (ToolKitModuleInstance != NULL)
	{
		ToolKitModuleInstance->EnableToolKit(ToolKitModuleInstance);
	}
}

void FToolKitModule::DisableToolKitStatic()
{
	FToolKitModule* ToolKitModuleInstance = FModuleManager::GetModulePtr<FToolKitModule>("ToolKit");

	if (ToolKitModuleInstance != NULL)
	{
		ToolKitModuleInstance->DisableToolKit();
	}
}

IMPLEMENT_EXPORTED_CLASS(FToolKitModule)
#endif

void FToolKitModule::ConnectToProxyServer()
{
	if (!bIsEnabled)
	{
		UE_LOG(LogToolKit, Warning, TEXT("ToolKit instance has not been enabled yet."));
	}
	else if (bIsConnecting)
	{
		UE_LOG(LogToolKit, Warning, TEXT("Last attempt to connect to Proxy Server is not done."));
	}
	else if (bIsConnected)
	{
		UE_LOG(LogToolKit, Warning, TEXT("Another connection to Proxy Server is already established."));
	}
	else
	{
		bIsConnecting = true;

		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [&]() {
			if (G6ToolKit::GetG6ToolKitInstance()->Start())
			{
				UE_LOG(LogToolKit, Log, TEXT("Connection to Proxy Server is successful."));
				bIsConnected = true;
				bToReconnect = false;
			}
			else
			{
				UE_LOG(LogToolKit, Log, TEXT("Connection to Proxy Server is failed."));
				bIsConnected = false;
				bToReconnect = TOOLKIT_AUTO_RECONNECT;
			}
			bIsConnecting = false;
		});
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FToolKitModule, ToolKit)