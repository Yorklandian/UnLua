#pragma once
#include <cstdint>
#include <string>

#ifdef TOOLKIT_USE_STATIC_LIB
#define TOOLKITCORE_API
#else
#ifdef _WIN32
#if defined TOOLKIT_LIB
#define TOOLKITCORE_API __declspec(dllexport)
#else						/* }{ */
#define TOOLKITCORE_API __declspec(dllimport)
#endif
#else
#define TOOLKITCORE_API __attribute__((visibility("default")))
#endif
#endif

struct lua_State;

namespace G6ToolKit {
	class Variable;

	typedef enum
	{
		ToolKit_LOG_Pri_Trace = 0,
		ToolKit_LOG_Pri_Debug,
		ToolKit_LOG_Pri_Info,
		ToolKit_LOG_Pri_Warning,
		ToolKit_LOG_Pri_Event,
		ToolKit_LOG_Pri_Error,
		ToolKit_LOG_Pri_None,
	} ToolKit_LOG_PRIORITY;

	typedef enum
	{
		// manager
		ToolKit_Event_Enabled = 0,
		ToolKit_Event_Disabled = 1,
		ToolKit_Event_NetworkBreak = 2,
		ToolKit_Event_NativeLog = 3,                        // arg1: log level,  arg2: log detail

		// ide
		ToolKit_Event_IDE_Login = 100,
		ToolKit_Event_IDE_Logout = 101,
		ToolKit_Event_IDE_Error = 102,                      // arg1: errno, arg2: errstr

		// submodule: logger
		ToolKit_Event_Logger_Started = 200,
		ToolKit_Event_Logger_Stopped = 201,
		ToolKit_Event_Logger_Error = 202,                   // arg1: errno, arg2: errstr

		// submodule: debug
		ToolKit_Event_Debug_Started = 300,
		ToolKit_Event_Debug_Stopped = 301,
		ToolKit_Event_Debug_Error = 302,                    // arg1: errno, arg2: errstr

		// submodule: profiler
		ToolKit_Event_Profiler_Started = 400,
		ToolKit_Event_Profiler_Stopped = 401,
		ToolKit_Event_Profiler_Error = 402,                 // arg1: errno, arg2: errstr

		// submodule: filetransfer
		ToolKit_Event_FileTransfer_NewFile_Recieved = 500,  // arg1: errno, arg1: absolute path of recieved file
		ToolKit_Event_FileTransfer_Error = 501,             // arg1: errno, arg2: errstr

	} ToolKit_Event;

	struct G6ToolKitBaseInfo {
		std::string				game_id;			// game id & game key for ide plugin to filter
		std::string				game_key;
		std::string				proxy_server_url;	// proxy server address to connect
		std::string				display_name;		// display name on ide plugin
		std::string				display_category;	// category name on ide plugin
		int 					enable_native_log;	// 0: disable native log, otherwise, enable native log
		ToolKit_LOG_PRIORITY	native_log_level;	// native log level
	};

	class IG6ToolKitCallback {
	public:
		virtual ~IG6ToolKitCallback() {};

		virtual void OnToolKitEvent(ToolKit_Event event, const int arg1, const std::string arg2,  void* const arg3 = nullptr) = 0;
	};

	class IG6ToolKit {
	public:  /**** ToolKit Manager API *****/
		// Init, set base info
		virtual void Init(G6ToolKitBaseInfo base_info, IG6ToolKitCallback* callback) = 0;

		// Start ToolKit to connect with proxy server
		virtual bool Start() = 0;

		// Stop ToolKit, will stop connect with proxy server
		virtual void Stop() = 0;

		// MainThread Tick
		virtual void Tick() = 0;


	public:  /**** LogModule API ****/
		// send local log to G6IDE while log is enabled in G6IDE. if toolkit log is not enable, it will no effect
		virtual void SendLogToIDE(const char* msg, int len, unsigned int level) = 0;

		// level is customize by project >= iLevel will send. default is 0
		virtual void SetSendLogToIDELevel(unsigned int level) = 0;

		// another way send local log to ide, just support linux
		virtual void SetSendLogPathToIDE(const char* file_path_ptr) = 0;


	public:  /**** DebugModule API ****/
		// set lua runtime environment
		// main_lua_state : the lua state will be debuged, if lua_state is destroyed, set main_lua_state to nullptr
		virtual void SetLuaState(lua_State* main_lua_state) = 0;

		virtual void SetUserdataEvaluator(void(*func)(lua_State*, Variable*, int, int)) = 0;

	public:  /**** FileSystemModule API ****/
		// set root path to be displayed in G6IDE
		virtual void SetFileSystemRootPath(const char* key, const char* value) = 0;

	};

	extern "C" {
		TOOLKITCORE_API IG6ToolKit* GetG6ToolKitInstance();
		TOOLKITCORE_API void DestroyG6ToolKitInstance();
	}
}
