#pragma once
#include "Patcher.h"

// Modera: 'parent' is used for localus signals (non-interesting for us)
#ifdef _WIN64
typedef void* (__fastcall* _igame_level_signal)(void* _unused, void** str_shared, void* parent, const int relatives);
typedef void (__fastcall* _igame_level_signal_a1)(void* _unused, void** str_shared, void* parent, const int relatives);
typedef void (__fastcall* _igame_level_signal_ex)(void* _unused, void** str_shared, void* parent, const int relatives, void* _unknown);
typedef void* (__fastcall* _unknown_exodus)();
typedef int (__fastcall* _erase_civil)(void* container, void* task_id);

#else

// Modera: static function, so no 'this'
typedef void (__stdcall* _igame_level_signal_2033)(void** str_shared, int parent);
typedef void* (__stdcall* _igame_level_signal_LL)(void** str_shared, void* parent, const int relatives);
#endif

class RestoreCommands : public Patcher
{
public:
	RestoreCommands();
	static void cmd_register_commands();
#ifdef _WIN64
	static void __fastcall signal_execute(void* _this, const char* name);
	static void __fastcall fly_execute(void* _this, const char* name);
	static void __fastcall refly_execute(void* _this, const char* args);
	static void __fastcall civ_off_execute(void* _this, const char* args);
	static void __fastcall civ_restore_execute(void* _this, const char* args);
#else
	static void __thiscall signal_execute(void* _this, const char* name);
	static void __thiscall fly_execute(void* _this, const char* name);
	static void __thiscall refly_execute(void* _this, const char* args);
#endif
};

