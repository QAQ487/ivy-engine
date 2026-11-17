/**===========================================================================

 File   :  [debug_ofstream.h]

 Author : Terry Yeung
 Date   : 2025/11/16
 -----------------------------------------------------------------------------

 ===========================================================================**/
#ifndef DEBUG_OSTREAM_H
#define DEBUG_OSTREAM_H

#include <windows.h>
#include <mutex>
#include <sstream>
#include <string>

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
	// Prefer this over raw OutputDebugStringA so the editor Console can capture.
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

	// Wide → UTF-8 then DebugWriteA (capturable). Prefer DebugWriteA when possible.
	inline void DebugWriteW(const wchar_t* text)
	{
		if (!text || !*text)
			return;
		const int bytes = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
		if (bytes <= 1)
			return;
		std::string utf8(static_cast<size_t>(bytes - 1), '\0');
		WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8.data(), bytes, nullptr, nullptr);
		DebugWriteA(utf8.c_str());
	}

	// Per-thread accumulation for dout. Lifetime is tied to the thread (not to
	// the global debugbuf object), so ~debugbuf never touches a destroyed TLS
	// string. Flushing here also emits a worker's last un-endl'd line at exit.
	struct DebugTlsBuffer {
		std::string text;
		~DebugTlsBuffer()
		{
			if (!text.empty()) {
				DebugWriteA(text.c_str());
				text.clear();
			}
		}
	};

	inline DebugTlsBuffer& DoutTls()
	{
		thread_local DebugTlsBuffer buf;
		return buf;
	}

	// Streambuf with thread_local storage so concurrent `dout <<` from worker
	// threads do not race a shared string buffer. Flush still goes through DebugWriteA.
	// Note: the global `dout` ostream's format state (width/fill/flags) is shared
	// across threads — prefer DebugWriteA from workers when formatting matters.
	class debugbuf : public std::streambuf {
	protected:
		int_type overflow(int_type ch) override
		{
			if (!traits_type::eq_int_type(ch, traits_type::eof()))
				DoutTls().text.push_back(traits_type::to_char_type(ch));
			return traits_type::not_eof(ch);
		}

		std::streamsize xsputn(const char* s, std::streamsize count) override
		{
			if (s && count > 0)
				DoutTls().text.append(s, static_cast<size_t>(count));
			return count;
		}

		int sync() override
		{
			std::string& buf = DoutTls().text;
			if (!buf.empty()) {
				DebugWriteA(buf.c_str());
				buf.clear();
			}
			return 0;
		}

	public:
		// Do not sync() here: at global dout destruction the main thread's
		// thread_local buffer may already be destroyed (use-after-free).
		~debugbuf() override = default;
	};

	class debug_ostream : public std::basic_ostream<char, std::char_traits<char>> {
	public:
		debug_ostream() : std::basic_ostream<char, std::char_traits<char>>(new debugbuf()) {}
		~debug_ostream() { delete rdbuf(); }
	};

	extern debug_ostream dout;
}

#endif // !DEBUG_OSTREAM_H
