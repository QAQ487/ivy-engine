// save_system.h
// Process-wide save store for the script API (ScriptApi_Save*).
//
// Save folder:
//   editor (Play mode)   <content project>/.ivy/saves/
//   runtime / package    <Saved Games>/<save.folderName or name from project.json>/
//                        (falls back to %LOCALAPPDATA%, then <exe dir>/saves)
//
// Lock SaveSystem_Mutex() around SaveSystem_Store() and every call on it.

#pragma once

#include <filesystem>
#include <mutex>

namespace save_store { class Store; }

std::mutex& SaveSystem_Mutex();
// Created on first use (folder resolved then). Caller holds SaveSystem_Mutex().
save_store::Store& SaveSystem_Store();
// Drops the store and its unsaved changes; the next use re-reads the disk and
// re-resolves the folder. The editor calls this when Play stops.
void SaveSystem_Reset();
