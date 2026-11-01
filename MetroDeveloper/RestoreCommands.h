#pragma once
#include "Patcher.h"

#ifdef _WIN64
typedef int (__fastcall* _erase_civil)(void* container, void* task_id);
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
	static void __fastcall wpn_give_execute(void* _this, const char* args);
	static void __fastcall wpn_give_ak_74_execute(void* _this, const char* args);
	static void __fastcall wpn_give_ashot_execute(void* _this, const char* args);
	static void __fastcall wpn_give_flamethrower_execute(void* _this, const char* args);
	static void __fastcall wpn_give_gatling_execute(void* _this, const char* args);
	static void __fastcall wpn_give_helsing_execute(void* _this, const char* args);
	static void __fastcall wpn_give_revolver_execute(void* _this, const char* args);
	static void __fastcall wpn_give_tihar_execute(void* _this, const char* args);
	static void __fastcall wpn_give_ubludok_execute(void* _this, const char* args);
	static void __fastcall wpn_give_uboynicheg_execute(void* _this, const char* args);
	static void __fastcall wpn_give_ventil_execute(void* _this, const char* args);
	static void __fastcall wpn_give_vyhlop_execute(void* _this, const char* args);
	static void numpad_r_on_key_press(int key);
#else
	static void __thiscall signal_execute(void* _this, const char* name);
	static void __thiscall fly_execute(void* _this, const char* name);
	static void __thiscall refly_execute(void* _this, const char* args);
#endif
};

