/**===========================================================================

 File   :  [debug_ofstream.h]

 Author : Terry Yeung
 Date   : 2025/11/16
 -----------------------------------------------------------------------------

 ===========================================================================**/
#ifndef DEBUG_OSTREAM_H
#define DEBUG_OSTREAM_H

#include <Windows.h>
#include <mutex>
#include <sstream>

namespace hal {
	// Optional sink for editor console / tests. Called after OutputDebugStringA.
	// Must be thread-safe / non-blocking; may be invoked from worker threads.
	using DebugLogHook = void (*)(const char* text, void* user);

	inline std::mutex& DebugLogHookMutex()
	{
		static std::mutex m;
		return m;
	}
	inline DebugLogHook& DebugLogHookFn()
	{
		static DebugLogHook hook = nullptr;
		return hook;
	}
	inline void*& DebugLogHookUser()
	{
		static void* user = nullptr;
		return user;
	}

	inline void SetDebugLogHook(DebugLogHook hook, void* user = nullptr)
	{
		std::lock_guard<std::mutex> lock(DebugLogHookMutex());
		DebugLogHookFn() = hook;
		DebugLogHookUser() = user;
	}

	inline void ClearDebugLogHook()
	{
		std::lock_guard<std::mutex> lock(DebugLogHookMutex());
		DebugLogHookFn() = nullptr;
		DebugLogHookUser() = nullptr;
	}

	// OutputDebugStringA + active hook (canonical path for capturable logs).
	inline void DebugWriteA(const char* text)
	{
		if (text && *text)
			OutputDebugStringA(text);

		DebugLogHook hook = nullptr;
		void* user = nullptr;
		{
			std::lock_guard<std::mutex> lock(DebugLogHookMutex());
			hook = DebugLogHookFn();
			user = DebugLogHookUser();
		}
		if (hook && text)
			hook(text, user);
	}

	class debugbuf : public std::basic_stringbuf<char, std::char_traits<char>> {
	public:
		virtual ~debugbuf() {
			sync();
		}

	protected:
		int sync() {
			DebugWriteA(str().c_str());
			str(std::basic_string<char>());
			return 0;
		}
	};

	class debug_ostream : public std::basic_ostream<char, std::char_traits<char>> {
	public:
		debug_ostream() : std::basic_ostream<char, std::char_traits<char>>(new debugbuf()) {}
		~debug_ostream() { delete rdbuf(); }
	};

	extern debug_ostream dout;
}

#endif // !DEBUG_OSTREAM_H
