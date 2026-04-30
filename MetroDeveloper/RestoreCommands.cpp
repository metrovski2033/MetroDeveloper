#include "RestoreCommands.h"
#include "Utils.h"
#include "uconsole.h"
#include "Fly.h"
#include "Hooks.h"

void* cmd_mask_Address = nullptr;
unsigned int* ps_actor_flags = nullptr;
unsigned int* igame_hud__flags = nullptr;

uconsole_command_vtbl signal_vftable;
uconsole_command_vtbl fly_vftable;
uconsole_command_vtbl refly_vftable;

cmd_mask_struct_ll g_god_old;
cmd_mask_struct_ll g_global_god_old;
cmd_mask_struct_ll g_notarget_old;
cmd_mask_struct_ll g_unlimitedammo_old;
cmd_mask_struct_ll g_unlimitedfilters_old;
cmd_mask_struct_ll r_hud_weapon_old;
cmd_float_struct_ll r_base_fov; // LL no patches
cmd_executor_struct_ll fly_old;
cmd_executor_struct_ll refly_old;
cmd_executor_struct_ll signal_old;

#ifdef _WIN64
uconsole_command_exodus_vtbl signal_vftable_exodus;
uconsole_command_exodus_vtbl fly_vftable_exodus;
uconsole_command_exodus_vtbl refly_vftable_exodus;
uconsole_command_exodus_vtbl civ_off_vftable_exodus;
uconsole_command_exodus_vtbl civ_restore_vftable_exodus;
uconsole_command_exodus_vtbl wpn_give_vftable_exodus;
uconsole_command_exodus_vtbl wpn_give_pickup_hook_vftable_exodus;
uconsole_command_exodus_vtbl wpn_give_attach_hook_vftable_exodus;

cmd_mask_struct_a1 g_god_new;
cmd_mask_struct_a1 g_global_god_new;
cmd_mask_struct_a1 g_notarget_new;
cmd_mask_struct_a1 g_unlimitedammo_new;
cmd_mask_struct_a1 g_unlimitedfilters_new;
cmd_mask_struct_a1 g_godbless_new;
cmd_executor_struct_a1 fly_new;
cmd_executor_struct_a1 refly_new;
cmd_executor_struct_a1 signal_new;
cmd_executor_struct_a1 civ_off_new;
cmd_executor_struct_a1 civ_restore_new;
cmd_executor_struct_a1 wpn_give_new;
cmd_executor_struct_a1 wpn_give_pickup_hook_new;
cmd_executor_struct_a1 wpn_give_attach_hook_new;

DWORD64 igame_level_signal;
_unknown_exodus unknown_exodus;
int* refly_default_cycles_exodus = nullptr;
float* refly_default_speed_exodus = nullptr;

static void** civ_global_ptr_addr = nullptr;
static _erase_civil erase_civil_fn = nullptr;

// civ save/restore 用構造体
struct civ_saved_entry1 {
	BYTE data[0x28];
	uint16_t index;
};
struct civ_saved_entry2 {
	BYTE data[0x30];
	uint16_t index;
};
typedef void* (__fastcall* _entity_find_by_name)(void* output, const char* name, int flags);
static _entity_find_by_name entity_find_by_name_fn = nullptr;

static void** g_str_table = nullptr;
static _unknown_exodus signal_context_fn = nullptr;

typedef void (__fastcall* _pickup_wrapper)(void* player_entity, void* weapon_entity);
static _pickup_wrapper pickup_wrapper_fn = nullptr;

typedef void (__fastcall* _entity_attach)(void* parent_component, void** output_ptr, void** new_entity_ptr, void* context);
static _entity_attach entity_attach_fn = nullptr;

typedef int (__fastcall* _queue_add_weapon)(void* actor, unsigned short holder_idx, void* unused, unsigned short weapon_idx, int flag);
static _queue_add_weapon queue_add_weapon_fn = nullptr;
static _queue_add_weapon queue_add_weapon_orig = nullptr;
static bool queue_add_spy = false;

static void** g_equip_actor_ptr = nullptr;

static bool wpn_signal_monitor = false;
static bool wpn_callstack_dump = false;
static _igame_level_signal_ex orig_igame_level_signal_ex_fn = nullptr;

static _entity_attach entity_attach_orig = nullptr;
static bool entity_attach_monitor = false;

static _pickup_wrapper pickup_wrapper_orig = nullptr;
static bool pickup_monitor = false;
static bool pickup_in_progress = false;

struct attach_log_entry {
	void* parent_component;
	void* output_ptr;
	void* entity;
	BYTE ctx_byte;
	void* pre_180;
	void* post_180;
};
static const int ATTACH_LOG_SIZE = 32;
static attach_log_entry attach_log[ATTACH_LOG_SIZE];
static int attach_log_idx = 0;
static int attach_log_count = 0;

static bool wpn_give_in_progress = false;


struct runtime_slot {
	void* parent_component;
	BYTE context_byte;
	char entity_name[64];
};
static const int MAX_RUNTIME_SLOTS = 64;
static runtime_slot runtime_slots[MAX_RUNTIME_SLOTS];
static int runtime_slot_count = 0;

static const char* resolve_str_handle(DWORD handle);
static void* resolve_actor();

int __fastcall hook_queue_add_weapon(void* actor, unsigned short holder_idx, void* unused, unsigned short weapon_idx, int flag)
{
	if (queue_add_spy) {
		char buf[512];
		const char* holder_name = "(?)";
		const char* weapon_name = "(?)";
		__try {
			DWORD64 em = Utils::GetGEntities();
			if (em != 0) {
				void** ea = *(void***)(em + 0x10);
				if (ea != nullptr) {
					if (holder_idx < 0xFFFF && ea[holder_idx] != nullptr) {
						DWORD h = *(DWORD*)((BYTE*)ea[holder_idx] + 0x238);
						if (h != 0) holder_name = resolve_str_handle(h);
					}
					if (weapon_idx < 0xFFFF && ea[weapon_idx] != nullptr) {
						DWORD h = *(DWORD*)((BYTE*)ea[weapon_idx] + 0x238);
						if (h != 0) weapon_name = resolve_str_handle(h);
					}
				}
			}
		} __except(EXCEPTION_EXECUTE_HANDLER) {}
		if (holder_name == nullptr) holder_name = "(null)";
		if (weapon_name == nullptr) weapon_name = "(null)";

		void* equip_actor = (g_equip_actor_ptr != nullptr) ? *g_equip_actor_ptr : nullptr;
		sprintf(buf, "[MetroDev] SPY queue_add: actor=%p%s holder=%u[%s] weapon=%u[%s] flag=0x%08X\n",
			actor, (actor == equip_actor) ? " [PLAYER]" : "",
			holder_idx, holder_name, weapon_idx, weapon_name, (unsigned int)flag);
		OutputDebugStringA(buf);
	}
	return queue_add_weapon_orig(actor, holder_idx, unused, weapon_idx, flag);
}

void __fastcall hook_igame_level_signal_ex(void* _unused, void** str_shared, void* parent, const int relatives, void* _unknown)
{
	if (wpn_signal_monitor && str_shared != nullptr) {
		void* sig = *str_shared;
		const char* name = nullptr;
		if (sig != nullptr) {
			name = resolve_str_handle((DWORD)(DWORD64)sig);
		}
		char buf[512];
		sprintf(buf, "[MetroDev] SIG: name='%s' handle=%p parent=%p rel=%d ctx=%p\n",
			name ? name : "(null)", sig, parent, relatives, _unknown);
		OutputDebugStringA(buf);

		if (wpn_callstack_dump && name != nullptr && strstr(name, "trade_enable") != nullptr) {
			MODULEINFO mod;
			GetModuleInformation(GetCurrentProcess(), GetModuleHandle(NULL), &mod, sizeof(mod));
			DWORD64 base = (DWORD64)mod.lpBaseOfDll;

			void* stack[64];
			USHORT frames = RtlCaptureStackBackTrace(0, 64, stack, NULL);
			sprintf(buf, "[MetroDev] CALLSTACK for '%s' (%d frames, base=%p):\n", name, frames, (void*)base);
			OutputDebugStringA(buf);
			for (USHORT f = 0; f < frames; f++) {
				DWORD64 addr = (DWORD64)stack[f];
				DWORD64 rva = addr - base;
				bool in_module = (addr >= base && addr < base + mod.SizeOfImage);
				sprintf(buf, "[MetroDev]   [%02d] %p  RVA=0x%llX%s\n",
					f, stack[f], (unsigned long long)rva, in_module ? "" : " (external)");
				OutputDebugStringA(buf);
			}
			OutputDebugStringA("[MetroDev] CALLSTACK END\n");
		}
	}
	orig_igame_level_signal_ex_fn(_unused, str_shared, parent, relatives, _unknown);
}

void __fastcall hook_pickup_wrapper(void* player_entity, void* weapon_entity)
{
	if (pickup_monitor) {
		char buf[512];
		const char* name = nullptr;
		__try {
			DWORD handle = *(DWORD*)((BYTE*)weapon_entity + 0x238);
			if (handle != 0) name = resolve_str_handle(handle);
		} __except(EXCEPTION_EXECUTE_HANDLER) {}

		sprintf(buf, "[MetroDev] PICKUP_BEGIN: player=%p weapon=%p name='%s'\n",
			player_entity, weapon_entity, name ? name : "(unknown)");
		OutputDebugStringA(buf);

		sprintf(buf, "[MetroDev] === Recent %d entity_attach calls (before pickup) ===\n", attach_log_count);
		OutputDebugStringA(buf);
		int start = (attach_log_count < ATTACH_LOG_SIZE) ? 0 : (attach_log_idx % ATTACH_LOG_SIZE);
		for (int j = 0; j < attach_log_count; j++) {
			__try {
				attach_log_entry* e = &attach_log[(start + j) % ATTACH_LOG_SIZE];
				const char* parent_name = nullptr;
				const char* ent_name = nullptr;
				__try {
					DWORD h = *(DWORD*)((BYTE*)e->parent_component + 0x238);
					if (h != 0) parent_name = resolve_str_handle(h);
				} __except(EXCEPTION_EXECUTE_HANDLER) {}
				if (e->entity != nullptr) {
					__try {
						DWORD h = *(DWORD*)((BYTE*)e->entity + 0x238);
						if (h != 0) ent_name = resolve_str_handle(h);
					} __except(EXCEPTION_EXECUTE_HANDLER) {}
				}
				sprintf(buf, "[MetroDev]   [%d] parent=%p('%s') ent=%p('%s') ctx=0x%02X [+180]=%p->%p\n",
					j, e->parent_component, parent_name ? parent_name : "?",
					e->entity, ent_name ? ent_name : "?",
					e->ctx_byte, e->pre_180, e->post_180);
				OutputDebugStringA(buf);
			} __except(EXCEPTION_EXECUTE_HANDLER) {
				sprintf(buf, "[MetroDev]   [%d] EXCEPTION reading entry\n", j);
				OutputDebugStringA(buf);
			}
		}
		OutputDebugStringA("[MetroDev] === End recent attach log ===\n");
		attach_log_count = 0;
		attach_log_idx = 0;
	}
	pickup_in_progress = pickup_monitor;
	pickup_wrapper_orig(player_entity, weapon_entity);
	pickup_in_progress = false;
	if (pickup_monitor) {
		OutputDebugStringA("[MetroDev] PICKUP_END\n");
	}
}

void __fastcall hook_entity_attach(void* parent_component, void** output_ptr, void** new_entity_ptr, void* context)
{
	if (entity_attach_monitor) {
		char buf[512];
		void* new_ent = (new_entity_ptr != nullptr) ? *new_entity_ptr : nullptr;
		BYTE ctx_byte = 0;
		__try { if (context) ctx_byte = *(BYTE*)context; } __except(EXCEPTION_EXECUTE_HANDLER) {}
		void* pre_180 = nullptr;
		__try { pre_180 = *(void**)((BYTE*)parent_component + 0x180); } __except(EXCEPTION_EXECUTE_HANDLER) {}

		sprintf(buf, "[MetroDev] ATTACH: parent=%p out=%p ent=%p ctx=0x%02X [parent+180]=%p\n",
			parent_component, output_ptr, new_ent, ctx_byte, pre_180);
		OutputDebugStringA(buf);
	}

	if (pickup_monitor) {
		void* new_ent = (new_entity_ptr != nullptr) ? *new_entity_ptr : nullptr;
		BYTE ctx_byte = 0;
		__try { if (context) ctx_byte = *(BYTE*)context; } __except(EXCEPTION_EXECUTE_HANDLER) {}
		void* pre_180 = nullptr;
		__try { pre_180 = *(void**)((BYTE*)parent_component + 0x180); } __except(EXCEPTION_EXECUTE_HANDLER) {}

		int idx = attach_log_idx % ATTACH_LOG_SIZE;
		attach_log_idx++;
		if (attach_log_count < ATTACH_LOG_SIZE) attach_log_count++;

		attach_log_entry* e = &attach_log[idx];
		e->parent_component = parent_component;
		e->output_ptr = output_ptr;
		e->entity = new_ent;
		e->ctx_byte = ctx_byte;
		e->pre_180 = pre_180;
		e->post_180 = nullptr;

		entity_attach_orig(parent_component, output_ptr, new_entity_ptr, context);

		__try { e->post_180 = *(void**)((BYTE*)parent_component + 0x180); } __except(EXCEPTION_EXECUTE_HANDLER) {}
		return;
	}

	entity_attach_orig(parent_component, output_ptr, new_entity_ptr, context);
}

static const int CIV_MAX_SAVE = 64;
static civ_saved_entry1 civ_save1[CIV_MAX_SAVE];
static civ_saved_entry2 civ_save2[CIV_MAX_SAVE];
static int civ_save1_count = 0;
static int civ_save2_count = 0;
static bool civ_has_saved = false;

#else

DWORD igame_level_signal;

cmd_executor_struct_2033 signal_2033;
cmd_executor_struct_ll signal_ll;
#endif

RestoreCommands::RestoreCommands()
{
	if (Utils::GetBool("other", "restore_deleted_commands", false)) {
#ifndef _WIN64
		if (Utils::isLL()) {
			// c7 05 ? ? ? ? ? ? ? ? 89 1d ? ? ? ? 89 1d ? ? ? ? 89 1d ? ? ? ? e8 ? ? ? ? 83 c4 ? e8 - Last Light
			ps_actor_flags = *(unsigned int**)(FindPatternInEXE(
				"\xc7\x05\x00\x00\x00\x00\x00\x00\x00\x00\x89\x1d\x00\x00\x00\x00\x89\x1d\x00\x00\x00\x00\x89\x1d\x00\x00\x00\x00\xe8\x00\x00\x00\x00\x83\xc4\x00\xe8",
				"xx????????xx????xx????xx????x????xx?x") + 6);

			// 8B 0D ? ? ? ? C1 E9 03 - Last Light
			igame_hud__flags = *(unsigned int**)(FindPatternInEXE("\x8B\x0D\x00\x00\x00\x00\xC1\xE9\x03", "xx????xxx") + 2);

			// 53 55 8B 6C 24 0C 56 8B 35 - LL
			igame_level_signal = FindPatternInEXE(
				"\x53\x55\x8B\x6C\x24\x0C\x56\x8B\x35",
				"xxxxxxxxx"
			);
		} else {
			// 83 EC 14 53 55 56 8B 35 DC 0D A1 00 8B 86 F0 01 00 00 57 8D BE F0 01 00 00 0F B6 C8 - orig 2033
			igame_level_signal = FindPatternInEXE(
				"\x83\xEC\x14\x53\x55\x56\x8B\x35\xDC\x0D\xA1\x00\x8B\x86\xF0\x01\x00\x00\x57\x8D\xBE\xF0\x01\x00\x00\x0F\xB6\xC8",
				"xx?xxxxx????xx????xxx????xxx"
			);
		}
#else
		if (Utils::isRedux()) {
			// 8B 05 ? ? ? ? C1 E8 03 - Redux
			DWORD64 mov = FindPatternInEXE("\x8B\x05\x00\x00\x00\x00\xC1\xE8\x03", "xx????xxx");

			igame_hud__flags = (unsigned int*)Utils::GetAddrFromRelativeInstr(mov, 6, 2);

			// 48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 41 56 41 57 48 83 EC 30 48 8B 2D ? ? ? ? 45 8B F1 4D 8B F8 48 8B FA FF 15 - Redux STEAM
			igame_level_signal = FindPatternInEXE(
				"\x48\x89\x5C\x24\x00\x48\x89\x6C\x24\x00\x48\x89\x74\x24\x00\x57\x41\x56\x41\x57\x48\x83\xEC\x30\x48\x8B\x2D\x00\x00\x00\x00\x45\x8B\xF1\x4D\x8B\xF8\x48\x8B\xFA\xFF\x15",
				"xxxx?xxxx?xxxx?xxxxxxxxxxxx????xxxxxxxxxxx");

			if (igame_level_signal == NULL) {
				// 48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 41 56 48 83 EC 30 48 8B 3D ? ? ? ? 41 8B E9 4D 8B F0 48 8B F2 FF 15 - Redux EGS
				igame_level_signal = FindPatternInEXE(
					"\x48\x89\x5C\x24\x00\x48\x89\x6C\x24\x00\x48\x89\x74\x24\x00\x48\x89\x7C\x24\x00\x41\x56\x48\x83\xEC\x30\x48\x8B\x3D\x00\x00\x00\x00\x41\x8B\xE9\x4D\x8B\xF0\x48\x8B\xF2\xFF\x15",
					"xxxx?xxxx?xxxx?xxxx?xxxxxxxxx????xxxxxxxxxxx");
			}
		}
		else if (Utils::isArktika()) {
			// 48 89 5C 24 ? 48 89 6C 24 ? 56 57 41 56 48 83 EC 30 C7 44 24 ? ? ? ? ? 41 8B F1 4C 8B 35 ? ? ? ? 49 8B E8 48 8B FA FF 15 - Arktika
			igame_level_signal = FindPatternInEXE(
				"\x48\x89\x5C\x24\x00\x48\x89\x6C\x24\x00\x56\x57\x41\x56\x48\x83\xEC\x30\xC7\x44\x24\x00\x00\x00\x00\x00\x41\x8B\xF1\x4C\x8B\x35\x00\x00\x00\x00\x49\x8B\xE8\x48\x8B\xFA\xFF\x15",
				"xxxx?xxxx?xxxxxxxxxxx?????xxxxxx????xxxxxxxx");
		}
		else if (Utils::isExodus()) {
			DWORD64 call_igame_level_signal;
			DWORD64 call_unknown_exodus;

			if (Utils::isExodusPatched) {
				// E8 ? ? ? ? 40 88 35
				call_igame_level_signal = FindPatternInEXE("\xE8\x00\x00\x00\x00\x40\x88\x35", "x????xxx");

				// E8 ? ? ? ? 4C 8B 38
				call_unknown_exodus = FindPatternInEXE("\xE8\x00\x00\x00\x00\x4C\x8B\x38", "x????xxx");
			} else {
				// E8 ? ? ? ? E9 ? ? ? ? 48 8D 51 14
				call_igame_level_signal = FindPatternInEXE("\xE8\x00\x00\x00\x00\xE9\x00\x00\x00\x00\x48\x8D\x51\x14", "x????x????xxxx");

				// E8 ? ? ? ? 4C 89 65 58
				call_unknown_exodus = FindPatternInEXE("\xE8\x00\x00\x00\x00\x4C\x89\x65\x58", "x????xxxx");
			}

			if (call_igame_level_signal != NULL)
				igame_level_signal = Utils::GetAddrFromRelativeInstr(call_igame_level_signal, 5, 1);
			if (call_unknown_exodus != NULL)
				unknown_exodus = (_unknown_exodus) Utils::GetAddrFromRelativeInstr(call_unknown_exodus, 5, 1);

			// civ_off: erase_wrapper 関数本体を探す (Exodus/EE共通で試行)
			// 40 53 48 83 EC 20 48 8B 05 ? ? ? ? 48 8B D9 48 8B 48 28
			// 48 85 C9 74 ? 48 8B 01 FF 90 60 08 00 00 48 85 C0 74 ?
			// 48 8B 88 F0 10 00 00 48 8B D3 E8 ? ? ? ? 48 83 C4 20 5B C3
			DWORD64 erase_wrapper = FindPatternInEXE(
				"\x40\x53\x48\x83\xEC\x20\x48\x8B\x05\x00\x00\x00\x00\x48\x8B\xD9\x48\x8B\x48\x28"
				"\x48\x85\xC9\x74\x00\x48\x8B\x01\xFF\x90\x60\x08\x00\x00\x48\x85\xC0\x74\x00\x48"
				"\x8B\x88\xF0\x10\x00\x00\x48\x8B\xD3\xE8\x00\x00\x00\x00\x48\x83\xC4\x20\x5B\xC3",
				"xxxxxxxxx????xxxxxxxxxxx?xxxxxxxxxxxxx?xxxxxxxxxxx????xxxxxx");
			if (erase_wrapper != NULL) {
				// +6: mov rax, [rip+disp32] (7バイト命令, disp は +3)
				civ_global_ptr_addr = (void**)Utils::GetAddrFromRelativeInstr(erase_wrapper + 6, 7, 3);
				// +49: call rel32 → erase_civil 関数本体
				erase_civil_fn = (_erase_civil)Utils::GetAddrFromRelativeInstr(erase_wrapper + 49, 5, 1);
				OutputDebugStringA("[MetroDev] civ_off: pattern found\n");
			}

			// wpn_give: entity_find_by_name を探す
			// 48 89 5C 24 18 55 56 57 41 56 41 57 48 8D AC 24 C0 FE FF FF 48 81 EC 40 02 00 00 45
			DWORD64 efbn = FindPatternInEXE(
				"\x48\x89\x5C\x24\x18\x55\x56\x57\x41\x56\x41\x57\x48\x8D\xAC\x24\xC0\xFE\xFF\xFF\x48\x81\xEC\x40\x02\x00\x00\x45",
				"xxxxxxxxxxxxxxxxxxxxxxxxxxxx");
			if (efbn != NULL) {
				entity_find_by_name_fn = (_entity_find_by_name)efbn;
				OutputDebugStringA("[MetroDev] wpn_give: entity_find_by_name found\n");
			}

			// wpn_give: 文字列解決テーブルを探す
			// entity登録関数内の: 4C 8B 15 ?? ?? ?? ?? 4C 8D 35 ?? ?? ?? ?? 48 85 F6
			{
				DWORD64 str_table_pat = FindPatternInEXE(
					"\x4C\x8B\x15\x00\x00\x00\x00\x4C\x8D\x35\x00\x00\x00\x00\x48\x85\xF6",
					"xxx????xxx????xxx");
				if (str_table_pat != NULL) {
					g_str_table = (void**)Utils::GetAddrFromRelativeInstr(str_table_pat + 7, 7, 3);
					char buf[128];
					sprintf(buf, "[MetroDev] wpn_give: str_table=%p\n", g_str_table);
					OutputDebugStringA(buf);
				}
			}

			// wpn_give: シグナルコンテキスト関数を探す
			// igame_level_signal 呼び出し直前パターン:
			// E8 ?? ?? ?? ?? 45 33 C9 48 89 44 24 20 45 33 C0
			{
				DWORD64 sig_ctx_pat = FindPatternInEXE(
					"\xE8\x00\x00\x00\x00\x45\x33\xC9\x48\x89\x44\x24\x20\x45\x33\xC0",
					"x????xxxxxxxxxxx");
				if (sig_ctx_pat != NULL) {
					signal_context_fn = (_unknown_exodus)Utils::GetAddrFromRelativeInstr(sig_ctx_pat, 5, 1);
					char buf[128];
					sprintf(buf, "[MetroDev] wpn_give: signal_context_fn=%p\n", signal_context_fn);
					OutputDebugStringA(buf);
				}
			}

			// wpn_give: pickup_wrapper (武器ピックアップ関数)
			// Pattern: mov rbx,rdx; mov rsi,rcx; call ??; mov rax,[rbx]; mov rcx,rbx; call [rax+0x878]
			{
				DWORD64 pickup_pat = FindPatternInEXE(
					"\x48\x8B\xDA\x48\x8B\xF1\xE8\x00\x00\x00\x00\x48\x8B\x03\x48\x8B\xCB\xFF\x90\x78\x08\x00\x00",
					"xxxxxxx????xxxxxxxxxxxx");
				if (pickup_pat != NULL) {
					pickup_wrapper_fn = (_pickup_wrapper)(pickup_pat - 0xF);
					char buf[128];
					sprintf(buf, "[MetroDev] wpn_give: pickup_wrapper=%p\n", pickup_wrapper_fn);
					OutputDebugStringA(buf);
				}
			}

			// wpn_give: entity_attach (エンティティアタッチ関数)
			// 関数プロローグ: push rbx; push rbp; push r12; push r14; push r15; sub rsp,40h; mov rbx,rcx
			{
				DWORD64 attach_pat = FindPatternInEXE(
					"\x40\x53\x55\x41\x54\x41\x56\x41\x57\x48\x83\xEC\x40\x48\x8B\xD9"
					"\x33\xED\x49\x8B\x08\x4D\x8B\xF1\x4D\x8B\xF8\x4C\x8B\xE2",
					"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
				if (attach_pat != NULL) {
					entity_attach_fn = (_entity_attach)attach_pat;
					char buf[128];
					sprintf(buf, "[MetroDev] wpn_give: entity_attach=%p\n", entity_attach_fn);
					OutputDebugStringA(buf);
				}
			}

			// wpn_give: queue_add_weapon (pending queueにエントリ追加)
			// mov rbx,rcx; movzx esi,r9w; mov ecx,-1; movzx ebp,dx; xor eax,eax; lock cmpxchg [rbx+0x281C],ecx
			{
				DWORD64 qa_pat = FindPatternInEXE(
					"\x48\x8B\xD9\x41\x0F\xB7\xF1\xB9\xFF\xFF\xFF\xFF\x0F\xB7\xEA\x33\xC0"
					"\xF0\x0F\xB1\x8B\x1C\x28\x00\x00",
					"xxxxxxxxxxxxxxxxxxxxxxxxx");
				if (qa_pat != NULL) {
					queue_add_weapon_fn = (_queue_add_weapon)(qa_pat - 0x1A);
					char buf[128];
					sprintf(buf, "[MetroDev] wpn_give: queue_add_weapon=%p\n", queue_add_weapon_fn);
					OutputDebugStringA(buf);

					MH_STATUS mhs = MH_CreateHook((void*)queue_add_weapon_fn, (void*)&hook_queue_add_weapon, (void**)&queue_add_weapon_orig);
					if (mhs == MH_OK) {
						MH_EnableHook((void*)queue_add_weapon_fn);
						OutputDebugStringA("[MetroDev] wpn_give: queue_add_weapon hooked\n");
					}
				}
			}

			// wpn_give: equip_function内のグローバルactorポインタ (RVA 0x15E8520)
			// equip_function内: mov rcx,[rip+disp32]; test rcx,rcx; jz +5; call rel32
			// 48 8B 0D ?? ?? ?? ?? 48 85 C9 74 05 E8 ?? ?? ?? ??
			// process_pending_attachments呼び出し直前のパターン
			{
				DWORD64 equip_pat = FindPatternInEXE(
					"\x48\x8B\x0D\x00\x00\x00\x00\x48\x85\xC9\x74\x05\xE8\x00\x00\x00\x00",
					"xxx????xxxxxx????");
				if (equip_pat != NULL) {
					g_equip_actor_ptr = (void**)Utils::GetAddrFromRelativeInstr(equip_pat, 7, 3);
					char buf[128];
					sprintf(buf, "[MetroDev] wpn_give: g_equip_actor_ptr=%p\n", g_equip_actor_ptr);
					OutputDebugStringA(buf);
				}
			}

		}
#endif
	}
}

#ifdef _WIN64
void __fastcall RestoreCommands::signal_execute(void* _this, const char* name)
#else
void __thiscall RestoreCommands::signal_execute(void* _this, const char* name)
#endif
{
	void* s = Utils::str_shared(name);

#ifdef _WIN64
	if (Utils::isRedux()) {
		((_igame_level_signal)igame_level_signal)(NULL, &s, NULL, 0);
	} else if (Utils::isArktika()) {
		((_igame_level_signal_a1)igame_level_signal)(NULL, &s, NULL, 0);
	} else {
		((_igame_level_signal_ex)igame_level_signal)(NULL, &s, NULL, 0, unknown_exodus());
	}
	
#else

	if (Utils::is2033()) {
		((_igame_level_signal_2033)igame_level_signal)(&s, 0);
	} else {
		((_igame_level_signal_LL)igame_level_signal)(&s, NULL, 0);
	}
#endif
}

#ifdef _WIN64
void __fastcall RestoreCommands::fly_execute(void* _this, const char* name)
#else
void __thiscall RestoreCommands::fly_execute(void* _this, const char* name)
#endif
{
#ifdef _WIN64
	if (Utils::isExodus()) {
		uconsole_server_exodus** console = (uconsole_server_exodus**)Utils::GetConsole();
		(*console)->hide(console);
	} else {
		uconsole_server** console = (uconsole_server**)Utils::GetConsole();
		(*console)->hide(console);
	}
#else
	uconsole_server** console = (uconsole_server**)Utils::GetConsole();
	(*console)->hide(console);
#endif

	Fly::fly(name, false, 0, 0);
}

#ifdef _WIN64
static void* resolve_actor()
{
	if (civ_global_ptr_addr == nullptr) return nullptr;
	void* g = *civ_global_ptr_addr;
	if (g == nullptr) return nullptr;
	void* mgr = *(void**)((char*)g + 0x28);
	if (mgr == nullptr) return nullptr;
	void** vtable = *(void***)mgr;
	if (vtable == nullptr) return nullptr;
	typedef void* (__fastcall *vmeth_t)(void*);
	vmeth_t get_actor = (vmeth_t)(*(void**)((char*)vtable + 0x860));
	if (get_actor == nullptr) return nullptr;
	return get_actor(mgr);
}

static void* civ_resolve_container()
{
	void* actor = resolve_actor();
	if (actor == nullptr) return nullptr;
	return *(void**)((char*)actor + 0x10F0);
}

void __fastcall RestoreCommands::civ_off_execute(void* _this, const char* args)
{
	if (erase_civil_fn == nullptr) return;

	void* container = civ_resolve_container();
	if (container == nullptr) {
		OutputDebugStringA("[MetroDev] civ_off: container unresolved\n");
		return;
	}

	uint16_t count1 = *(uint16_t*)((char*)container + 0x4A);
	void* base1 = *(void**)((char*)container + 0x40);
	if (base1 == nullptr || count1 == 0) {
		OutputDebugStringA("[MetroDev] civ_off: no active civil tasks\n");
		return;
	}

	// 消去前にエントリを保存 (第1配列: offset 0x40, count at 0x4A, entry 0x28)
	civ_save1_count = 0;
	for (uint16_t i = 0; i < count1 && civ_save1_count < CIV_MAX_SAVE; ++i) {
		void* entry = (char*)base1 + i * 0x28;
		void* id = *(void**)((char*)entry + 0x10);
		if (id != nullptr) {
			memcpy(civ_save1[civ_save1_count].data, entry, 0x28);
			civ_save1[civ_save1_count].index = i;
			civ_save1_count++;
		}
	}

	// 第2配列: offset 0x30, count at 0x3A, entry 0x30, task_id at entry+0x18
	civ_save2_count = 0;
	uint16_t count2 = *(uint16_t*)((char*)container + 0x3A);
	void* base2 = *(void**)((char*)container + 0x30);
	if (base2 != nullptr) {
		for (uint16_t i = 0; i < count2 && civ_save2_count < CIV_MAX_SAVE; ++i) {
			void* entry = (char*)base2 + i * 0x30;
			void* id = *(void**)((char*)entry + 0x18);
			if (id != nullptr) {
				memcpy(civ_save2[civ_save2_count].data, entry, 0x30);
				civ_save2[civ_save2_count].index = i;
				civ_save2_count++;
			}
		}
	}
	civ_has_saved = (civ_save1_count > 0 || civ_save2_count > 0);

	// task_id を収集してから消去
	const int MAX_IDS = 256;
	void* ids[MAX_IDS];
	int n = 0;
	for (uint16_t i = 0; i < count1 && n < MAX_IDS; ++i) {
		void* entry = (char*)base1 + i * 0x28;
		void* id = *(void**)((char*)entry + 0x10);
		if (id != nullptr) ids[n++] = id;
	}

	char buf[128];
	sprintf(buf, "[MetroDev] civ_off: clearing %d task(s), saved %d+%d entries\n",
		n, civ_save1_count, civ_save2_count);
	OutputDebugStringA(buf);

	for (int i = 0; i < n; ++i) {
		erase_civil_fn(container, ids[i]);
	}
}

void __fastcall RestoreCommands::civ_restore_execute(void* _this, const char* args)
{
	if (!civ_has_saved || (civ_save1_count == 0 && civ_save2_count == 0)) {
		OutputDebugStringA("[MetroDev] civ_restore: no saved data (run civ_off first)\n");
		return;
	}

	void* container = civ_resolve_container();
	if (container == nullptr) {
		OutputDebugStringA("[MetroDev] civ_restore: container unresolved\n");
		return;
	}

	// atomic lock 取得 (erase_civil と同じパターン)
	DWORD tid = GetCurrentThreadId();
	volatile LONG* lock_ptr = (volatile LONG*)((char*)container + 0x270);
	bool lock_owned = false;
	for (;;) {
		LONG prev = InterlockedCompareExchange(lock_ptr, (LONG)tid, 0);
		if (prev == 0) { lock_owned = true; break; }
		if (prev == (LONG)tid) break;
		YieldProcessor();
	}

	// 第1配列を復元
	void* base1 = *(void**)((char*)container + 0x40);
	uint16_t count1 = *(uint16_t*)((char*)container + 0x4A);
	int restored1 = 0;
	if (base1 != nullptr) {
		for (int i = 0; i < civ_save1_count; ++i) {
			if (civ_save1[i].index < count1) {
				void* entry = (char*)base1 + civ_save1[i].index * 0x28;
				memcpy(entry, civ_save1[i].data, 0x28);
				restored1++;
			}
		}
	}

	// 第2配列を復元
	void* base2 = *(void**)((char*)container + 0x30);
	uint16_t count2 = *(uint16_t*)((char*)container + 0x3A);
	int restored2 = 0;
	if (base2 != nullptr) {
		for (int i = 0; i < civ_save2_count; ++i) {
			if (civ_save2[i].index < count2) {
				void* entry = (char*)base2 + civ_save2[i].index * 0x30;
				memcpy(entry, civ_save2[i].data, 0x30);
				restored2++;
			}
		}
	}

	// lock 解放
	if (lock_owned) {
		*lock_ptr = 0;
	}

	char buf[128];
	sprintf(buf, "[MetroDev] civ_restore: restored %d+%d entries\n", restored1, restored2);
	OutputDebugStringA(buf);
}


static const char* resolve_str_handle(DWORD handle)
{
	if (handle == 0 || g_str_table == nullptr) return nullptr;
	DWORD bucket = handle >> 24;
	DWORD offset = handle & 0x00FFFFFF;
	void* base = g_str_table[bucket];
	if (base == nullptr) return nullptr;
	return (const char*)((BYTE*)base + offset + 0x14);
}

void __fastcall RestoreCommands::wpn_give_attach_hook_execute(void* _this, const char* args)
{
	if (entity_attach_fn == nullptr) {
		OutputDebugStringA("[MetroDev] wpn_give_attach_hook: entity_attach not found\n");
		return;
	}
	entity_attach_monitor = !entity_attach_monitor;
	char buf[128];
	sprintf(buf, "[MetroDev] entity_attach monitor %s\n", entity_attach_monitor ? "ON" : "OFF");
	OutputDebugStringA(buf);
}

void __fastcall RestoreCommands::wpn_give_pickup_hook_execute(void* _this, const char* args)
{
	pickup_monitor = !pickup_monitor;
	char buf[128];
	sprintf(buf, "[MetroDev] pickup monitor %s\n", pickup_monitor ? "ON" : "OFF");
	OutputDebugStringA(buf);
}

void __fastcall RestoreCommands::wpn_give_execute(void* _this, const char* args)
{
	while (*args == ' ') args++;
	if (*args == '\0') {
		OutputDebugStringA("[MetroDev] wpn_give: usage: wpn_give <name> | list [prefix] | scan | dump | debug\n");
		return;
	}

	if (strcmp(args, "scan") == 0) {
		void* actor_old = resolve_actor();
		void* actor_new = (g_equip_actor_ptr != nullptr) ? *g_equip_actor_ptr : nullptr;
		void* actor = (actor_new != nullptr) ? actor_new : actor_old;
		if (actor == nullptr) {
			OutputDebugStringA("[MetroDev] scan: actor is null\n");
			return;
		}
		char buf[256];
		sprintf(buf, "[MetroDev] scan: actor=%p (equip=%p, civ=%p), entity_attach=%p\n",
			actor, actor_new, actor_old, entity_attach_fn);
		OutputDebugStringA(buf);

		// actor のメモリをスキャンして、ホルダーコンポーネント（+0x180にエンティティポインタを持つオブジェクト）を探す
		// パターン: *(actor+offset) はポインタ P を保持、P-0xE0+0x180 = P+0xA0 にエンティティがある
		BYTE* ab = (BYTE*)actor;
		int found = 0;
		for (DWORD off = 0x2700; off < 0x2C00; off += 8) {
			__try {
				void* holder_ptr = *(void**)(ab + off);
				if (holder_ptr == nullptr) continue;
				DWORD64 hp = (DWORD64)holder_ptr;
				if (hp < 0x10000) continue;

				// holder_ptr - 0xE0 = parent_component
				// parent_component + 0x180 = 装着済みエンティティ
				void* parent = (void*)(hp - 0xE0);
				void* attached_ent = nullptr;
				attached_ent = *(void**)((BYTE*)parent + 0x180);

				if (attached_ent == nullptr) continue;
				DWORD64 ae = (DWORD64)attached_ent;
				if (ae < 0x10000) continue;

				// エンティティ名を解決
				DWORD handle = *(DWORD*)((BYTE*)attached_ent + 0x238);
				const char* name = nullptr;
				if (handle != 0) name = resolve_str_handle(handle);

				if (name != nullptr) {
					sprintf(buf, "[MetroDev] scan: actor+0x%X -> holder=%p parent=%p attached='%s' (%p)\n",
						off, holder_ptr, parent, name, attached_ent);
					OutputDebugStringA(buf);
					found++;
				}
			} __except(EXCEPTION_EXECUTE_HANDLER) {}
		}
		sprintf(buf, "[MetroDev] scan: done, %d attachment slots found\n", found);
		OutputDebugStringA(buf);
		return;
	}

	if (strcmp(args, "slots") == 0) {
		DWORD64 em = Utils::GetGEntities();
		if (em == 0) { OutputDebugStringA("[MetroDev] slots: g_entities is null\n"); return; }
		void** ea = nullptr;
		__try { ea = *(void***)(em + 0x10); } __except(EXCEPTION_EXECUTE_HANDLER) {}
		if (ea == nullptr) { OutputDebugStringA("[MetroDev] slots: entity_array is null\n"); return; }

		runtime_slot_count = 0;
		char buf[512];
		for (int i = 0; i < 0xFFFF && runtime_slot_count < MAX_RUNTIME_SLOTS; i++) {
			void* ent = ea[i];
			if (ent == nullptr) continue;
			__try {
				DWORD handle = *(DWORD*)((BYTE*)ent + 0x238);
				if (handle == 0) continue;
				const char* name = resolve_str_handle(handle);
				if (name == nullptr || name[0] == '\0') continue;

				bool found_pc = false;

				// method 1: エンティティ内ポインタで [ptr+0x180]==ent を検索（広範囲）
				for (DWORD off = 0; off < 0x800 && !found_pc; off += 8) {
					void* ptr = *(void**)((BYTE*)ent + off);
					if (ptr == nullptr || (DWORD64)ptr < 0x10000) continue;
					__try {
						void* val = *(void**)((BYTE*)ptr + 0x180);
						if (val == ent) {
							runtime_slots[runtime_slot_count].parent_component = ptr;
							runtime_slots[runtime_slot_count].context_byte = 0x09;
							strncpy(runtime_slots[runtime_slot_count].entity_name, name, 63);
							runtime_slots[runtime_slot_count].entity_name[63] = '\0';
							runtime_slot_count++;
							found_pc = true;
						}
					} __except(EXCEPTION_EXECUTE_HANDLER) {}
				}

				// method 2: entity+0xE0 (parent) 付近を逆方向スキャン
				if (!found_pc) {
					void* parent = *(void**)((BYTE*)ent + 0xE0);
					if (parent != nullptr && (DWORD64)parent > 0x10000) {
						for (DWORD back = 0; back <= 0x200 && !found_pc; back += 8) {
							void* candidate = (void*)((BYTE*)parent - back);
							__try {
								void* val = *(void**)((BYTE*)candidate + 0x180);
								if (val == ent) {
									runtime_slots[runtime_slot_count].parent_component = candidate;
									runtime_slots[runtime_slot_count].context_byte = 0x09;
									strncpy(runtime_slots[runtime_slot_count].entity_name, name, 63);
									runtime_slots[runtime_slot_count].entity_name[63] = '\0';
									runtime_slot_count++;
									found_pc = true;
								}
							} __except(EXCEPTION_EXECUTE_HANDLER) {}
						}
					}
				}
			} __except(EXCEPTION_EXECUTE_HANDLER) {}
		}

		sprintf(buf, "[MetroDev] wpn_give slots: %d found\n", runtime_slot_count);
		OutputDebugStringA(buf);
		for (int i = 0; i < runtime_slot_count; i++) {
			sprintf(buf, "[MetroDev]   [%d] '%s' parent=%p ctx=0x%02X\n",
				i, runtime_slots[i].entity_name, runtime_slots[i].parent_component, runtime_slots[i].context_byte);
			OutputDebugStringA(buf);
		}
		return;
	}

	if (strcmp(args, "spy") == 0) {
		queue_add_spy = !queue_add_spy;
		char buf[128];
		sprintf(buf, "[MetroDev] wpn_give: queue_add spy %s\n", queue_add_spy ? "ON" : "OFF");
		OutputDebugStringA(buf);
		return;
	}

	if (strcmp(args, "monitor") == 0) {
		wpn_signal_monitor = !wpn_signal_monitor;
		char buf[128];
		sprintf(buf, "[MetroDev] wpn_give: signal monitor %s\n", wpn_signal_monitor ? "ON" : "OFF");
		OutputDebugStringA(buf);
		return;
	}

	if (strcmp(args, "callstack") == 0) {
		wpn_signal_monitor = true;
		wpn_callstack_dump = !wpn_callstack_dump;
		char buf[128];
		sprintf(buf, "[MetroDev] wpn_give: callstack dump %s (monitor=ON)\n", wpn_callstack_dump ? "ON" : "OFF");
		OutputDebugStringA(buf);
		return;
	}

	DWORD64 entities_mgr = Utils::GetGEntities();
	if (entities_mgr == 0) {
		OutputDebugStringA("[MetroDev] wpn_give: g_entities is null\n");
		return;
	}

	void** entity_array = *(void***)(entities_mgr + 0x10);
	if (entity_array == nullptr) {
		OutputDebugStringA("[MetroDev] wpn_give: entity_array is null\n");
		return;
	}

	char buf[512];

	if (strcmp(args, "dump") == 0) {
		void* actor_old = resolve_actor();
		void* actor_new = (g_equip_actor_ptr != nullptr) ? *g_equip_actor_ptr : nullptr;
		void* actor = (actor_new != nullptr) ? actor_new : actor_old;
		if (actor == nullptr) {
			OutputDebugStringA("[MetroDev] dump: actor is null\n");
			return;
		}
		BYTE* ab = (BYTE*)actor;
		sprintf(buf, "[MetroDev] dump: actor=%p (equip=%p, civ=%p)\n", actor, actor_new, actor_old);
		OutputDebugStringA(buf);

		struct batch_info {
			const char* name;
			DWORD data_off;
			DWORD unk_off;
			DWORD count_off;
		};
		batch_info batches[] = {
			{ "batch1", 0x2820, 0x2828, 0x282A },
			{ "batch2", 0x2838, 0x2840, 0x2842 },
			{ "batch3", 0x2880, 0x2888, 0x288A },
			{ "batch4", 0x2850, 0x2858, 0x285A },
			{ "batch5", 0x00F0, 0x00F8, 0x00FA },
		};

		for (int b = 0; b < 5; b++) {
			__try {
				void* data_ptr = *(void**)(ab + batches[b].data_off);
				uint16_t unk = *(uint16_t*)(ab + batches[b].unk_off);
				uint16_t count = *(uint16_t*)(ab + batches[b].count_off);
				sprintf(buf, "[MetroDev] dump: %s(+0x%04X): data=%p unk=%d count=%d\n",
					batches[b].name, batches[b].data_off, data_ptr, unk, count);
				OutputDebugStringA(buf);

				if (data_ptr != nullptr && count > 0 && count < 64) {
					for (uint16_t e = 0; e < count; e++) {
						BYTE* entry = (BYTE*)data_ptr + e * 0x28;
						sprintf(buf, "[MetroDev]   entry[%d]: holder=%04X weapon=%04X flag=%02X\n",
							e, *(uint16_t*)(entry + 0x20), *(uint16_t*)(entry + 0x22), *(uint8_t*)(entry + 0x24));
						OutputDebugStringA(buf);
						char hex[256] = "[MetroDev]     ";
						int pos = (int)strlen(hex);
						for (int i = 0; i < 0x28; i++) {
							pos += sprintf(hex + pos, "%02X ", entry[i]);
							if (i == 0x1F) {
								strcat(hex, "\n");
								OutputDebugStringA(hex);
								strcpy(hex, "[MetroDev]     ");
								pos = (int)strlen(hex);
							}
						}
						strcat(hex, "\n");
						OutputDebugStringA(hex);
					}
				}
			} __except(EXCEPTION_EXECUTE_HANDLER) {
				sprintf(buf, "[MetroDev] dump: %s EXCEPTION\n", batches[b].name);
				OutputDebugStringA(buf);
			}
		}

		__try {
			uint8_t trigger = *(uint8_t*)(ab + 0x28B8);
			uint8_t lock2 = *(uint8_t*)(ab + 0x2890);
			void* slot_ptr = *(void**)(ab + 0x2898);
			uint16_t slot_count = *(uint16_t*)(ab + 0x28A2);
			uint8_t flag_bb = *(uint8_t*)(ab + 0x28BB);
			sprintf(buf, "[MetroDev] dump: trigger(+0x28B8)=%d lock2(+0x2890)=%d slots(+0x2898)=%p slot_count=%d flag_bb=%d\n",
				trigger, lock2, slot_ptr, slot_count, flag_bb);
			OutputDebugStringA(buf);
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			OutputDebugStringA("[MetroDev] dump: flags EXCEPTION\n");
		}

		__try {
			char hex[512] = "[MetroDev] dump raw +0x2800: ";
			int pos = (int)strlen(hex);
			for (int i = 0; i < 0xC0; i++) {
				pos += sprintf(hex + pos, "%02X ", ab[0x2800 + i]);
				if ((i + 1) % 32 == 0) {
					strcat(hex, "\n");
					OutputDebugStringA(hex);
					sprintf(hex, "[MetroDev] dump raw +0x%04X: ", 0x2800 + i + 1);
					pos = (int)strlen(hex);
				}
			}
		} __except(EXCEPTION_EXECUTE_HANDLER) {}

		return;
	}

	if (strcmp(args, "debug") == 0) {
		sprintf(buf, "[MetroDev] wpn_give debug: mgr=%p array=%p str_table=%p pickup=%p\n",
			(void*)entities_mgr, entity_array, g_str_table, pickup_wrapper_fn);
		OutputDebugStringA(buf);
		int total = 0;
		for (int i = 0; i < 0xFFFF; i++) {
			if (entity_array[i] != nullptr) total++;
		}
		sprintf(buf, "[MetroDev] wpn_give debug: total entities=%d\n", total);
		OutputDebugStringA(buf);
		return;
	}

	if (g_str_table == nullptr) {
		OutputDebugStringA("[MetroDev] wpn_give: str_table not initialized\n");
		return;
	}

	// list サブコマンド: エンティティを列挙
	if (strncmp(args, "list", 4) == 0) {
		const char* prefix = "wpn_";
		const char* sp = args + 4;
		while (*sp == ' ') sp++;
		if (*sp != '\0') prefix = sp;

		int count = 0;
		int free_count = 0;
		for (int i = 0; i < 0xFFFF; i++) {
			void* ent = entity_array[i];
			if (ent == nullptr) continue;
			__try {
				DWORD handle = *(DWORD*)((BYTE*)ent + 0x238);
				if (handle == 0) continue;
				const char* name = resolve_str_handle(handle);
				if (name != nullptr && strstr(name, prefix) != nullptr) {
					void* parent = *(void**)((BYTE*)ent + 0xE0);
					if (parent == nullptr) {
						sprintf(buf, "[MetroDev]   [%d] %s  parent=NULL [FREE]\n", i, name);
						OutputDebugStringA(buf);
						free_count++;
					} else {
						sprintf(buf, "[MetroDev]   [%d] %s  ent=%p parent(+E0)=%p\n", i, name, ent, parent);
						OutputDebugStringA(buf);

						// 方法1: entity自身の中をスキャンして[ptr+0x180]==entになるポインタを探す
						__try {
							for (DWORD off = 0; off < 0x300; off += 8) {
								void* ptr = *(void**)((BYTE*)ent + off);
								if (ptr == nullptr || (DWORD64)ptr < 0x10000) continue;
								__try {
									void* val = *(void**)((BYTE*)ptr + 0x180);
									if (val == ent) {
										sprintf(buf, "[MetroDev]     -> ent+0x%X=%p  [ptr+180]==ent!\n", off, ptr);
										OutputDebugStringA(buf);
									}
								} __except(EXCEPTION_EXECUTE_HANDLER) {}
							}
						} __except(EXCEPTION_EXECUTE_HANDLER) {}

						// 方法2: parent(+E0)から逆方向にスキャンしてparent_componentを探す
						__try {
							for (DWORD back = 0; back <= 0x200; back += 8) {
								void* candidate = (void*)((BYTE*)parent - back);
								__try {
									void* val = *(void**)((BYTE*)candidate + 0x180);
									if (val == ent) {
										sprintf(buf, "[MetroDev]     -> parent-0x%X=%p  [cand+180]==ent!\n", back, candidate);
										OutputDebugStringA(buf);
									}
								} __except(EXCEPTION_EXECUTE_HANDLER) {}
							}
						} __except(EXCEPTION_EXECUTE_HANDLER) {}
					}
					count++;
				}
			} __except(EXCEPTION_EXECUTE_HANDLER) {}
		}
		sprintf(buf, "[MetroDev] wpn_give list: %d entities matching '%s' (%d free)\n", count, prefix, free_count);
		OutputDebugStringA(buf);
		return;
	}

	// エンティティ名を取得
	char entity_name[256];
	strncpy(entity_name, args, sizeof(entity_name) - 1);
	entity_name[sizeof(entity_name) - 1] = '\0';
	char* sp = strchr(entity_name, ' ');
	if (sp) *sp = '\0';

	// エンティティ検索
	void* found_entity = nullptr;
	int found_index = -1;
	for (int i = 0; i < 0xFFFF && found_entity == nullptr; i++) {
		void* ent = entity_array[i];
		if (ent == nullptr) continue;
		__try {
			DWORD handle = *(DWORD*)((BYTE*)ent + 0x238);
			if (handle == 0) continue;
			const char* name = resolve_str_handle(handle);
			if (name != nullptr && strcmp(name, entity_name) == 0) {
				found_entity = ent;
				found_index = i;
			}
		} __except(EXCEPTION_EXECUTE_HANDLER) {}
	}

	if (found_entity == nullptr) {
		sprintf(buf, "[MetroDev] wpn_give: '%s' not found\n", entity_name);
		OutputDebugStringA(buf);
		return;
	}

	if (g_equip_actor_ptr == nullptr) {
		OutputDebugStringA("[MetroDev] wpn_give: g_equip_actor_ptr not initialized\n");
		return;
	}
	void* actor = *g_equip_actor_ptr;
	if (actor == nullptr) {
		OutputDebugStringA("[MetroDev] wpn_give: equip actor is null (not in game?)\n");
		return;
	}

	if (queue_add_weapon_fn == nullptr) {
		OutputDebugStringA("[MetroDev] wpn_give: queue_add not found\n");
		return;
	}

	sprintf(buf, "[MetroDev] wpn_give: queuing '%s' entity_idx=%d actor=%p\n",
		entity_name, found_index, actor);
	OutputDebugStringA(buf);

	DWORD exc_code = 0;
	__try {
		// holder=装備するアイテムのentity index, weapon=0(player宛), flag=0
		int result = queue_add_weapon_fn(actor, (unsigned short)found_index, nullptr, 0, 0);
		sprintf(buf, "[MetroDev] wpn_give: queue_add result=%d\n", result);
		OutputDebugStringA(buf);
	} __except(exc_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
		sprintf(buf, "[MetroDev] wpn_give: EXCEPTION 0x%08X in queue_add\n", exc_code);
		OutputDebugStringA(buf);
	}
}
#endif

#ifdef _WIN64
void __fastcall RestoreCommands::refly_execute(void* _this, const char* args)
#else
void __thiscall RestoreCommands::refly_execute(void* _this, const char* args)
#endif
{
	if (args[1] != 0) {
#ifdef _WIN64
		if (Utils::isExodus()) {
			uconsole_server_exodus** console = (uconsole_server_exodus**)Utils::GetConsole();
			(*console)->hide(console);
		} else {
			uconsole_server** console = (uconsole_server**)Utils::GetConsole();
			(*console)->hide(console);
		}
#else
		uconsole_server** console = (uconsole_server**)Utils::GetConsole();
		(*console)->hide(console);
#endif
		char name[272];

#ifdef _WIN64
		if (!Utils::isExodus()) {
			goto suda;
		} else {
			sscanf(args, "%[^,],%f,%d", name, refly_default_speed_exodus, refly_default_cycles_exodus);
			Fly::fly(name, true, *refly_default_speed_exodus, *refly_default_cycles_exodus);
			return;
		}
#endif

	suda:
		Fly::refly_speed = 1.f;
		Fly::refly_cycles = 2;

		sscanf(args, "%[^,],%f,%d", name, &Fly::refly_speed, &Fly::refly_cycles);
		Fly::fly(name, true, Fly::refly_speed, Fly::refly_cycles);
	}
}

void RestoreCommands::cmd_register_commands() {
	uconsole cu = uconsole::uconsole(Utils::GetConsole());

#ifdef _WIN64
	// 1. find constant string
	DWORD64 str_g_toggle_aim = FindPatternInEXE("g_toggle_aim\0", "xxxxxxxxxxxxx");
	DWORD64 str_save_player = FindPatternInEXE("save_player\0", "xxxxxxxxxxxx");

	if (str_g_toggle_aim == NULL || str_save_player == NULL) return;

	// 2. find reference to that string
	DWORD64 xref = FindPatternInEXE((char*)&str_g_toggle_aim, "xxxxxxxx");
	DWORD64 xref1 = FindPatternInEXE((char*)&str_save_player, "xxxxxxxx");

	if (xref == NULL || xref1 == NULL) return;

	if (Utils::GetGame() == GAME::REDUX) {
		// 3. find pointer to existing command object based on xref
		cmd_mask_struct_ll* pCmd = (cmd_mask_struct_ll*)(xref - offsetof(cmd_mask_struct_ll, _name));
		uconsole_command_ll* pCmd1 = (uconsole_command_ll*)(xref1 - offsetof(uconsole_command_ll, _name));

		// Validate vtable is within the EXE image range
		{
			MODULEINFO mod = GetModuleData(NULL);
			if ((DWORD64)pCmd->__vftable < (DWORD64)mod.lpBaseOfDll ||
				(DWORD64)pCmd->__vftable >= (DWORD64)mod.lpBaseOfDll + mod.SizeOfImage) return;
		}

		ps_actor_flags = pCmd->value;

		// 4. register new commands
		g_god_old.construct(pCmd->__vftable, "g_god", ps_actor_flags, 1, true);
		cu.command_add(&g_god_old);

		g_global_god_old.construct(pCmd->__vftable, "g_global_god", ps_actor_flags, 2, true);
		cu.command_add(&g_global_god_old);

		g_notarget_old.construct(pCmd->__vftable, "g_notarget", ps_actor_flags, 4, true);
		cu.command_add(&g_notarget_old);

		g_unlimitedammo_old.construct(pCmd->__vftable, "g_unlimitedammo", ps_actor_flags, 8, true);
		cu.command_add(&g_unlimitedammo_old);

		g_unlimitedfilters_old.construct(pCmd->__vftable, "g_unlimitedfilters", ps_actor_flags, 128, true);
		cu.command_add(&g_unlimitedfilters_old);

		r_hud_weapon_old.construct(pCmd->__vftable, "r_hud_weapon", igame_hud__flags, 3, false);
		cu.command_add(&r_hud_weapon_old);

		fly_old.construct(&fly_vftable, pCmd1->__vftable, "fly", &fly_execute);
		cu.command_add(&fly_old);

		refly_old.construct(&refly_vftable, pCmd1->__vftable, "refly", &refly_execute);
		cu.command_add(&refly_old);

		signal_old.construct(&signal_vftable, pCmd1->__vftable, "signal", &signal_execute);
		cu.command_add(&signal_old);
	} else {
		// 3. find pointer to existing command object based on xref
		cmd_mask_struct_a1* pCmd = (cmd_mask_struct_a1*)(xref - offsetof(cmd_mask_struct_a1, _name));
		uconsole_command_a1* pCmd1 = (uconsole_command_a1*)(xref1 - offsetof(uconsole_command_a1, _name));

		// Validate vtable is within the EXE image range
		{
			MODULEINFO mod = GetModuleData(NULL);
			if ((DWORD64)pCmd->__vftable < (DWORD64)mod.lpBaseOfDll ||
				(DWORD64)pCmd->__vftable >= (DWORD64)mod.lpBaseOfDll + mod.SizeOfImage) return;
		}

		if (Utils::isExodus()) {
			DWORD64 str_refly_default_cycles = FindPatternInEXE("refly_default_cycles\0", "xxxxxxxxxxxxxxxxxxxxx");
			DWORD64 xref2 = FindPatternInEXE((char*)&str_refly_default_cycles, "xxxxxxxx");
			cmd_integer_struct_a1* pCmd2 = (cmd_integer_struct_a1*)(xref2 - offsetof(cmd_integer_struct_a1, _name));

			DWORD64 str_refly_default_speed = FindPatternInEXE("refly_default_speed\0", "xxxxxxxxxxxxxxxxxxxx");
			DWORD64 xref3 = FindPatternInEXE((char*)&str_refly_default_speed, "xxxxxxxx");
			cmd_float_struct_a1* pCmd3 = (cmd_float_struct_a1*)(xref3 - offsetof(cmd_float_struct_a1, _name));

			refly_default_cycles_exodus = pCmd2->value;
			refly_default_speed_exodus = pCmd3->value;
		}

		ps_actor_flags = pCmd->value;

		// 4. register new commands
		g_god_new.construct(pCmd->__vftable, "g_god", ps_actor_flags, 1, uconsole_cm_type_a1::cm_user);
		cu.command_add(&g_god_new);

		g_global_god_new.construct(pCmd->__vftable, "g_global_god", ps_actor_flags, 2, uconsole_cm_type_a1::cm_user);
		cu.command_add(&g_global_god_new);

		g_notarget_new.construct(pCmd->__vftable, "g_notarget", ps_actor_flags, 4, uconsole_cm_type_a1::cm_user);
		cu.command_add(&g_notarget_new);

		g_unlimitedammo_new.construct(pCmd->__vftable, "g_unlimitedammo", ps_actor_flags, 8, uconsole_cm_type_a1::cm_user);
		cu.command_add(&g_unlimitedammo_new);

		g_unlimitedfilters_new.construct(pCmd->__vftable, "g_unlimitedfilters", ps_actor_flags, 128, uconsole_cm_type_a1::cm_user);
		cu.command_add(&g_unlimitedfilters_new);

		g_godbless_new.construct(pCmd->__vftable, "g_godbless", ps_actor_flags, 1024, uconsole_cm_type_a1::cm_user);
		cu.command_add(&g_godbless_new);

		fly_new.construct(Utils::isExodus() ? &fly_vftable_exodus : &fly_vftable, pCmd1->__vftable, "fly", &fly_execute);
		cu.command_add(&fly_new);

		refly_new.construct(Utils::isExodus() ? &refly_vftable_exodus : &refly_vftable, pCmd1->__vftable, "refly", &refly_execute);
		cu.command_add(&refly_new);

		signal_new.construct(Utils::isExodus() ? &signal_vftable_exodus : &signal_vftable, pCmd1->__vftable, "signal", &signal_execute);
		cu.command_add(&signal_new);

		if (Utils::isExodus() && erase_civil_fn != nullptr) {
			civ_off_new.construct(&civ_off_vftable_exodus, pCmd1->__vftable, "civ_off", &civ_off_execute);
			cu.command_add(&civ_off_new);
			civ_restore_new.construct(&civ_restore_vftable_exodus, pCmd1->__vftable, "civ_restore", &civ_restore_execute);
			cu.command_add(&civ_restore_new);
		}

		if (Utils::isExodus() && Utils::g_entities != nullptr) {
			wpn_give_new.construct(&wpn_give_vftable_exodus, pCmd1->__vftable, "wpn_give", &wpn_give_execute);
			cu.command_add(&wpn_give_new);

			if (orig_igame_level_signal_ex_fn == nullptr && igame_level_signal != 0) {
				Hooks::SetHook("igame_level_signal_ex_monitor",
					(void*)igame_level_signal,
					(void*)&hook_igame_level_signal_ex,
					(void**)&orig_igame_level_signal_ex_fn);
			}

			if (entity_attach_fn != nullptr && entity_attach_orig == nullptr) {
				Hooks::SetHook("entity_attach_monitor",
					(void*)entity_attach_fn,
					(void*)&hook_entity_attach,
					(void**)&entity_attach_orig);
				wpn_give_attach_hook_new.construct(&wpn_give_attach_hook_vftable_exodus, pCmd1->__vftable, "wpn_give_attach_hook", &wpn_give_attach_hook_execute);
				cu.command_add(&wpn_give_attach_hook_new);
			}

			if (pickup_wrapper_fn != nullptr && pickup_wrapper_orig == nullptr) {
				Hooks::SetHook("pickup_wrapper_monitor",
					(void*)pickup_wrapper_fn,
					(void*)&hook_pickup_wrapper,
					(void**)&pickup_wrapper_orig);
				wpn_give_pickup_hook_new.construct(&wpn_give_pickup_hook_vftable_exodus, pCmd1->__vftable, "wpn_give_pickup_hook", &wpn_give_pickup_hook_execute);
				cu.command_add(&wpn_give_pickup_hook_new);
			}

		}
	}
#else
	if (Utils::isLL())
	{
		cu.cmd_mask(&g_god_old, "g_god", ps_actor_flags, 1, true);
		cu.command_add(&g_god_old);

		cu.cmd_mask(&g_global_god_old, "g_global_god", ps_actor_flags, 2, true);
		cu.command_add(&g_global_god_old);

		cu.cmd_mask(&g_notarget_old, "g_notarget", ps_actor_flags, 4, true);
		cu.command_add(&g_notarget_old);

		cu.cmd_mask(&g_unlimitedammo_old, "g_unlimitedammo", ps_actor_flags, 8, true);
		cu.command_add(&g_unlimitedammo_old);

		cu.cmd_mask(&g_unlimitedfilters_old, "g_unlimitedfilters", ps_actor_flags, 128, true);
		cu.command_add(&g_unlimitedfilters_old);

		cu.cmd_mask(&r_hud_weapon_old, "r_hud_weapon", igame_hud__flags, 3, false);
		cu.command_add(&r_hud_weapon_old);

		// LL restore fly cmd
		// ��������� ��������� ucmd_save_player save_player ���-�� �������� �� ���� __vftable
		// C7 05 ? ? ? ? ? ? ? ? E8 ? ? ? ? 83 C4 04 E8 ? ? ? ? 8B 10 8B C8 8B 42 08 68 ? ? ? ? FF D0 84 1D ? ? ? ? 75 39 8A 0D ? ? ? ? 09 1D ? ? ? ? 80 E1 E7 80 C9 07 68 ? ? ? ? C7 05 ? ? ? ? ? ? ? ? 88 0D ? ? ? ? C7 05 ? ? ? ? ? ? ? ? E8 ? ? ? ? 83 C4 04 E8 ? ? ? ? 8B 10 8B C8 8B 42 08 68 ? ? ? ? FF D0 5B C3
		DWORD mov = FindPatternInEXE(
			"\xC7\x05\x00\x00\x00\x00\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x83\xC4\x04\xE8\x00\x00\x00\x00\x8B\x10\x8B\xC8\x8B\x42\x08\x68\x00\x00\x00\x00\xFF\xD0\x84\x1D\x00\x00\x00\x00\x75\x39\x8A\x0D\x00\x00\x00\x00\x09\x1D\x00\x00\x00\x00\x80\xE1\xE7\x80\xC9\x07\x68\x00\x00\x00\x00\xC7\x05\x00\x00\x00\x00\x00\x00\x00\x00\x88\x0D\x00\x00\x00\x00\xC7\x05\x00\x00\x00\x00\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x83\xC4\x04\xE8\x00\x00\x00\x00\x8B\x10\x8B\xC8\x8B\x42\x08\x68\x00\x00\x00\x00\xFF\xD0\x5B\xC3",
			"xx????????x????xxxx????xxxxxxxx????xxxx????xxxx????xx????xxxxxxx????xx????????xx????xx????????x????xxxx????xxxxxxxx????xxxx");

		uconsole_command_ll* pCmd1 = (uconsole_command_ll*)(*(DWORD*)(mov + 2));

		fly_old.construct(&fly_vftable, pCmd1->__vftable, "fly", &fly_execute);
		cu.command_add(&fly_old);

		refly_old.construct(&refly_vftable, pCmd1->__vftable, "refly", &refly_execute);
		cu.command_add(&refly_old);

		signal_ll.construct(&signal_vftable, pCmd1->__vftable, "signal", &signal_execute);
		cu.command_add(&signal_ll);

		// LL no patches restore r_base_fov
		DWORD str_r_base_fov = FindPatternInEXE("r_base_fov\0", "xxxxxxxxxxx");
		if (str_r_base_fov == NULL)
		{
			// 89 35 ? ? ? ? C7 05 ? ? ? ? ? ? ? ? F3 0F 11 05 ? ? ? ? E8 ? ? ? ? 59 E8 ? ? ? ? 8B 10 68 ? ? ? ? 8B C8 FF 52 08 B8 ? ? ? ? 85 05 ? ? ? ? 75 59 09 05
			mov = FindPatternInEXE(
				"\x89\x35\x00\x00\x00\x00\xC7\x05\x00\x00\x00\x00\x00\x00\x00\x00\xF3\x0F\x11\x05\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x59\xE8\x00\x00\x00\x00\x8B\x10\x68\x00\x00\x00\x00\x8B\xC8\xFF\x52\x08\xB8\x00\x00\x00\x00\x85\x05\x00\x00\x00\x00\x75\x59\x09\x05",
				"xx????xx????????xxxx????x????xx????xxx????xxxxxx????xx????xxxx");

			// ��������� ��������� cmd_float r_sun_tsm_projection ���-�� �������� �� ���� __vftable
			cmd_float_struct_ll* pCmd = (cmd_float_struct_ll*)(*(DWORD*)(mov + 2));

			// F3 0F 5E 05 ? ? ? ? 0F 28 CC
			DWORD divss = FindPatternInEXE("\xF3\x0F\x5E\x05\x00\x00\x00\x00\x0F\x28\xCC", "xxxx????xxx");

			// ��������� ����� fov
			float* r_base_fov_Address = (float*)(*(DWORD*)(divss + 4));

			// ������ �������
			r_base_fov.construct(pCmd->__vftable, "r_base_fov", r_base_fov_Address, 45.0f, 90.0f, true);
			cu.command_add(&r_base_fov);
		}
	} else {
		// ��������� ��������� ucmd_save_player save_player ���-�� �������� �� ���� __vftable
		// c7 05 ? ? ? ? ? ? ? ? e8 ? ? ? ? 83 c4 ? 8b 0d ? ? ? ? 3b cb 75 ? e8 ? ? ? ? 3b c3 74 ? 8b f8 e8 ? ? ? ? eb ? 33 c0 8b f0 a3 ? ? ? ? e8 ? ? ? ? 8b 0d ? ? ? ? 8b 01 8b 50 ? 68 ? ? ? ? ff d2 5f
		DWORD mov = FindPatternInEXE(
			"\xc7\x05\x00\x00\x00\x00\x00\x00\x00\x00\xe8\x00\x00\x00\x00\x83\xc4\x00\x8b\x0d\x00\x00\x00\x00\x3b\xcb\x75\x00\xe8\x00\x00\x00\x00\x3b\xc3\x74\x00\x8b\xf8\xe8\x00\x00\x00\x00\xeb\x00\x33\xc0\x8b\xf0\xa3\x00\x00\x00\x00\xe8\x00\x00\x00\x00\x8b\x0d\x00\x00\x00\x00\x8b\x01\x8b\x50\x00\x68\x00\x00\x00\x00\xff\xd2\x5f",
			"xx????????x????xx?xx????xxx?x????xxx?xxx????x?xxxxx????x????xx????xxxx?x????xxx");

		uconsole_command_2033* pCmd1 = (uconsole_command_2033*)(*(DWORD*)(mov + 2));

		signal_2033.construct(&signal_vftable, pCmd1->__vftable, "signal", &signal_execute);
		cu.command_add(&signal_2033);
	}
#endif
}
