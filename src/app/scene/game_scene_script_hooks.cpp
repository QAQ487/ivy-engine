// game_scene_script_hooks.cpp
// App: game scene script hooks implementation.

#include "game_scene_script_hooks.h"

#include "engine/scripting/script_factory.h"

bool GameSceneScript_InvokeBool(const char* exportName) {
	using BoolFn = bool(*)();
	if (BoolFn fn = reinterpret_cast<BoolFn>(ScriptFactory::Instance().ResolveDllExport(exportName)))
		return fn();
	// Optional content hook: missing export is success (samples / games may omit it).
	return true;
}

void GameSceneScript_Invoke(const char* exportName) {
	ScriptFactory::Instance().InvokeExportOnLoadedDlls(exportName);
}

void GameSceneScript_InvokeWithDouble(const char* exportName, double value) {
	using DoubleFn = void(*)(double);
	if (DoubleFn fn = reinterpret_cast<DoubleFn>(ScriptFactory::Instance().ResolveDllExport(exportName)))
		fn(value);
}
