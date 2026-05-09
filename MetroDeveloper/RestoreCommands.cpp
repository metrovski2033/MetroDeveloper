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
uconsole_command_exodus_vtbl find_str_vftable_exodus;
uconsole_command_exodus_vtbl dump_mem_vftable_exodus;

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
cmd_executor_struct_a1 find_str_new;
cmd_executor_struct_a1 dump_mem_new;

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

// DEZP handler hook for save preset injection
typedef int(__fastcall* _dezp_handler)(void* output, void* stream, int flag);
static _dezp_handler dezp_handler_orig = nullptr;
static BYTE* full_preset_block_data = nullptr;
static SIZE_T full_preset_block_size = 0;
static bool dezp_preset_patched = false;
static const BYTE PRESET_MARKER[8] = { 0xA8, 0x61, 0x00, 0x00, 0xA8, 0x61, 0x00, 0x00 };

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

		if (wpn_callstack_dump && name != nullptr &&
			(strstr(name, "trade_enable") != nullptr ||
			 strstr(name, "show_hint_menu") != nullptr ||
			 strstr(name, "pickup") != nullptr ||
			 strstr(name, "acquire") != nullptr ||
			 strstr(name, "got_") != nullptr)) {
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
		char buf[1024];
		void* new_ent = (new_entity_ptr != nullptr) ? *new_entity_ptr : nullptr;
		BYTE ctx_byte = 0;
		__try { if (context) ctx_byte = *(BYTE*)context; } __except(EXCEPTION_EXECUTE_HANDLER) {}
		void* pre_180 = nullptr;
		__try { pre_180 = *(void**)((BYTE*)parent_component + 0x180); } __except(EXCEPTION_EXECUTE_HANDLER) {}

		const char* parent_name = nullptr;
		const char* ent_name = nullptr;
		const char* ent_type = nullptr;
		const char* pre180_name = nullptr;
		const char* pre180_type = nullptr;
		__try {
			DWORD h = *(DWORD*)((BYTE*)parent_component + 0x238);
			if (h != 0) parent_name = resolve_str_handle(h);
		} __except(EXCEPTION_EXECUTE_HANDLER) {}
		if (new_ent != nullptr) {
			__try {
				DWORD h = *(DWORD*)((BYTE*)new_ent + 0x238);
				if (h != 0) ent_name = resolve_str_handle(h);
			} __except(EXCEPTION_EXECUTE_HANDLER) {}
			__try {
				void* def = *(void**)((BYTE*)new_ent + 0x240);
				if (def != nullptr) {
					DWORD th = *(DWORD*)((BYTE*)def + 0x008);
					if (th != 0) ent_type = resolve_str_handle(th);
				}
			} __except(EXCEPTION_EXECUTE_HANDLER) {}
		}
		if (pre_180 != nullptr) {
			__try {
				DWORD h = *(DWORD*)((BYTE*)pre_180 + 0x238);
				if (h != 0) pre180_name = resolve_str_handle(h);
			} __except(EXCEPTION_EXECUTE_HANDLER) {}
			__try {
				void* def = *(void**)((BYTE*)pre_180 + 0x240);
				if (def != nullptr) {
					DWORD th = *(DWORD*)((BYTE*)def + 0x008);
					if (th != 0) pre180_type = resolve_str_handle(th);
				}
			} __except(EXCEPTION_EXECUTE_HANDLER) {}
		}

		sprintf(buf, "[MetroDev] ATTACH: parent=%p('%s') ent=%p('%s' t='%s') ctx=0x%02X [+180]=%p('%s' t='%s')\n",
			parent_component, parent_name ? parent_name : "?",
			new_ent, ent_name ? ent_name : "?", ent_type ? ent_type : "?",
			ctx_byte,
			pre_180, pre180_name ? pre180_name : "?", pre180_type ? pre180_type : "?");
		OutputDebugStringA(buf);

		static void* last_scanned_ent = nullptr;
		if (new_ent != nullptr && (ctx_byte == 0x04 || ctx_byte == 0x08) && new_ent != last_scanned_ent) {
			last_scanned_ent = new_ent;
			sprintf(buf, "[MetroDev] === ATTACH ENT SCAN (ctx=0x%02X, ent=%p, parent=%p) ===\n", ctx_byte, new_ent, parent_component);
			OutputDebugStringA(buf);
			OutputDebugStringA("[MetroDev] --- ENT string handles 0x000-0x400 ---\n");
			for (int off = 0; off < 0x400; off += 4) {
				__try {
					DWORD h = *(DWORD*)((BYTE*)new_ent + off);
					if (h != 0 && h < 0x100000) {
						const char* s = resolve_str_handle(h);
						if (s != nullptr && s[0] != '\0') {
							sprintf(buf, "[MetroDev]   ent+0x%03X: handle=0x%08X -> '%s'\n", off, h, s);
							OutputDebugStringA(buf);
						}
					}
				} __except(EXCEPTION_EXECUTE_HANDLER) {}
			}
			OutputDebugStringA("[MetroDev] --- ENT pointer deref scan ---\n");
			for (int off = 0; off < 0x400; off += 8) {
				__try {
					void* ptr = *(void**)((BYTE*)new_ent + off);
					if (ptr != nullptr && (DWORD64)ptr > 0x10000 && (DWORD64)ptr < 0x00007FFFFFFFFFFF) {
						for (int sub = 0; sub <= 0x10; sub += 4) {
							__try {
								DWORD h = *(DWORD*)((BYTE*)ptr + sub);
								if (h != 0 && h < 0x100000) {
									const char* s = resolve_str_handle(h);
									if (s != nullptr && s[0] != '\0') {
										sprintf(buf, "[MetroDev]   ent+0x%03X->+0x%02X: handle=0x%08X -> '%s'\n", off, sub, h, s);
										OutputDebugStringA(buf);
									}
								}
							} __except(EXCEPTION_EXECUTE_HANDLER) {}
						}
					}
				} __except(EXCEPTION_EXECUTE_HANDLER) {}
			}
			// === PARENT (weapon) scan: 武器の attach 子リストを探す ===
			if (parent_component != nullptr) {
				OutputDebugStringA("[MetroDev] --- PARENT pointer scan (weapon -> attach children list candidates) ---\n");
				// 親の +0x000-0x800 範囲で 8byte 整列ポインタを走査
				for (int off = 0; off < 0x800; off += 8) {
					__try {
						void* ptr = *(void**)((BYTE*)parent_component + off);
						if (ptr == nullptr) continue;
						if ((DWORD64)ptr <= 0x10000 || (DWORD64)ptr >= 0x00007FFFFFFFFFFF) continue;
						// ptr が指す先の +0x238 (entity 名ハンドル) で entity か判定
						__try {
							DWORD h0 = *(DWORD*)((BYTE*)ptr + 0x238);
							if (h0 != 0 && h0 < 0x100000) {
								const char* s0 = resolve_str_handle(h0);
								if (s0 != nullptr && s0[0] == '$') {
									sprintf(buf, "[MetroDev]   parent+0x%03X -> ENTITY %p name='%s'\n", off, ptr, s0);
									OutputDebugStringA(buf);
								}
							}
						} __except(EXCEPTION_EXECUTE_HANDLER) {}
						// ptr が array なら最初のいくつかのポインタも entity 化判定
						for (int idx = 0; idx < 8; idx++) {
							__try {
								void* arr_ptr = *(void**)((BYTE*)ptr + idx * 8);
								if (arr_ptr == nullptr) continue;
								if ((DWORD64)arr_ptr <= 0x10000 || (DWORD64)arr_ptr >= 0x00007FFFFFFFFFFF) continue;
								DWORD h1 = *(DWORD*)((BYTE*)arr_ptr + 0x238);
								if (h1 != 0 && h1 < 0x100000) {
									const char* s1 = resolve_str_handle(h1);
									if (s1 != nullptr && s1[0] == '$') {
										sprintf(buf, "[MetroDev]   parent+0x%03X[arr+%d] -> ENTITY %p name='%s'\n", off, idx, arr_ptr, s1);
										OutputDebugStringA(buf);
									}
								}
							} __except(EXCEPTION_EXECUTE_HANDLER) { break; }
						}
					} __except(EXCEPTION_EXECUTE_HANDLER) {}
				}
			}
			OutputDebugStringA("[MetroDev] === END ATTACH ENT SCAN ===\n");
		}
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

static void load_full_preset_block()
{
	FILE* f = fopen("presets_full.bin", "rb");
	if (f == nullptr) {
		OutputDebugStringA("[MetroDev] DEZP: presets_full.bin not found\n");
		return;
	}
	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (fsize < 12 || fsize > 1024 * 1024) {
		OutputDebugStringA("[MetroDev] DEZP: presets_full.bin invalid size\n");
		fclose(f);
		return;
	}
	full_preset_block_data = (BYTE*)malloc(fsize);
	if (full_preset_block_data == nullptr) { fclose(f); return; }
	fread(full_preset_block_data, 1, fsize, f);
	fclose(f);
	full_preset_block_size = (SIZE_T)fsize;

	if (memcmp(full_preset_block_data, PRESET_MARKER, 8) != 0) {
		OutputDebugStringA("[MetroDev] DEZP: presets_full.bin bad marker\n");
		free(full_preset_block_data);
		full_preset_block_data = nullptr;
		return;
	}
	char msg[128];
	sprintf(msg, "[MetroDev] DEZP: loaded presets_full.bin (%llu bytes, count=%u)\n",
		(unsigned long long)full_preset_block_size, *(DWORD*)(full_preset_block_data + 8));
	OutputDebugStringA(msg);
}

static int dezp_call_count = 0;

int __fastcall hook_dezp_handler(void* output, void* stream, int flag)
{
	int result = dezp_handler_orig(output, stream, flag);
	int call_num = ++dezp_call_count;

	__try {
		if (result != 1 || *(DWORD*)output != 2) return result;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return result; }

	__try {
		char dbg[512];
		sprintf(dbg, "[MetroDev] DEZP #%d: [+10]=%p [+18]=0x%X [+20]=%p [+28]=0x%X [+30]=%p [+38]=0x%X flag=%d patched=%d\n",
			call_num,
			*(void**)((BYTE*)output + 0x10),
			*(DWORD*)((BYTE*)output + 0x18),
			*(void**)((BYTE*)output + 0x20),
			*(DWORD*)((BYTE*)output + 0x28),
			*(void**)((BYTE*)output + 0x30),
			*(DWORD*)((BYTE*)output + 0x38),
			flag, (int)dezp_preset_patched);
		OutputDebugStringA(dbg);
	} __except (EXCEPTION_EXECUTE_HANDLER) {}

	if (full_preset_block_data == nullptr || dezp_preset_patched)
		return result;

	int slots[] = { 0x10, 0x20, 0x30 };
	int size_offsets[] = { 0x18, 0x28, 0x38 };
	for (int si = 0; si < 3; si++) {
		int slot_off = slots[si];
		BYTE* meta_struct = nullptr;
		__try { meta_struct = *(BYTE**)((BYTE*)output + slot_off); } __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
		if (meta_struct == nullptr) continue;

		// 実データバッファは構造体[+0x18]に格納されている (struct allocated by 0x547830)
		BYTE* buf = nullptr;
		__try { buf = *(BYTE**)(meta_struct + 0x18); } __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
		if (buf == nullptr) continue;

		DWORD chunk_size = 0;
		__try { chunk_size = *(DWORD*)((BYTE*)output + size_offsets[si]); } __except (EXCEPTION_EXECUTE_HANDLER) { continue; }

		SIZE_T buf_size = (SIZE_T)chunk_size;
		if (buf_size < 16) continue;

		char smsg[512];
		sprintf(smsg, "[MetroDev] DEZP #%d: scan slot+0x%X meta=%p data=%p chunk_size=0x%X\n",
			call_num, slot_off, meta_struct, buf, chunk_size);
		OutputDebugStringA(smsg);

		// chunk1の先頭128バイトをHEXダンプ（LZ4展開結果がPythonと一致するか確認用）
		if (slot_off == 0x20 && buf_size >= 128) {
			char hex[400];
			for (int row = 0; row < 8; row++) {
				int off = 0;
				off += sprintf(hex + off, "[MetroDev] DEZP head[0x%02X]: ", row * 16);
				for (int c = 0; c < 16; c++) {
					off += sprintf(hex + off, "%02X ", buf[row * 16 + c]);
				}
				sprintf(hex + off, "\n");
				OutputDebugStringA(hex);
			}
		}

		// chunk1 全体をファイルに書き出す（FULL/EMPTY save の差分比較用）
		// ファイル名にサイズ+先頭8バイトを含めて、同じデータなら上書きされる
		if (buf_size >= 16) {
			char fn[256];
			int chunk_idx = (slot_off == 0x10) ? 0 : (slot_off == 0x20) ? 1 : 2;
			sprintf(fn, "dump_chunk%d_size%X_h%02X%02X%02X%02X%02X%02X%02X%02X.bin",
				chunk_idx, chunk_size,
				buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
			// 既存ファイルがあればスキップ（同じデータの再ダンプを避ける）
			DWORD attrs = GetFileAttributesA(fn);
			if (attrs == INVALID_FILE_ATTRIBUTES) {
				HANDLE hf = CreateFileA(fn, GENERIC_WRITE, 0, nullptr,
					CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
				if (hf != INVALID_HANDLE_VALUE) {
					DWORD written = 0;
					WriteFile(hf, buf, chunk_size, &written, nullptr);
					CloseHandle(hf);
					char msg[400];
					sprintf(msg, "[MetroDev] DEZP: dumped chunk%d -> %s (%u/%u bytes)\n",
						chunk_idx, fn, written, chunk_size);
					OutputDebugStringA(msg);
				} else {
					char msg[400];
					sprintf(msg, "[MetroDev] DEZP: dump CreateFile failed err=%lu fn=%s\n",
						GetLastError(), fn);
					OutputDebugStringA(msg);
				}
			} else {
				char msg[400];
				sprintf(msg, "[MetroDev] DEZP: chunk%d dump skipped (file exists): %s\n", chunk_idx, fn);
				OutputDebugStringA(msg);
			}
		}

		// 文字列ベースの検索: "weapon_item_preset" を chunk1 全域で探す
		if (slot_off == 0x20) {
			static const char kStrA[] = "weapon_item_preset";
			static const char kStrB[] = "weapon_silencer";
			static const char kStrC[] = "weapon_collimator";
			int hits_a = 0, hits_b = 0, hits_c = 0;
			SIZE_T first_a = (SIZE_T)-1, first_b = (SIZE_T)-1, first_c = (SIZE_T)-1;
			__try {
				for (SIZE_T i = 0; i + sizeof(kStrA) <= buf_size; i++) {
					if (buf[i] == 'w') {
						if (memcmp(buf + i, kStrA, sizeof(kStrA) - 1) == 0) {
							if (first_a == (SIZE_T)-1) first_a = i;
							hits_a++;
						}
						if (i + sizeof(kStrB) <= buf_size && memcmp(buf + i, kStrB, sizeof(kStrB) - 1) == 0) {
							if (first_b == (SIZE_T)-1) first_b = i;
							hits_b++;
						}
						if (i + sizeof(kStrC) <= buf_size && memcmp(buf + i, kStrC, sizeof(kStrC) - 1) == 0) {
							if (first_c == (SIZE_T)-1) first_c = i;
							hits_c++;
						}
					}
				}
			} __except (EXCEPTION_EXECUTE_HANDLER) {
				OutputDebugStringA("[MetroDev] DEZP string search: exception\n");
			}
			sprintf(smsg, "[MetroDev] DEZP str: weapon_item_preset hits=%d first=0x%llX  silencer hits=%d first=0x%llX  collimator hits=%d first=0x%llX\n",
				hits_a, (unsigned long long)first_a,
				hits_b, (unsigned long long)first_b,
				hits_c, (unsigned long long)first_c);
			OutputDebugStringA(smsg);

			// 最初の "weapon_item_preset" 周辺 ±256 バイトをダンプ（マーカー探索のヒント）
			if (first_a != (SIZE_T)-1) {
				SIZE_T from = (first_a >= 64) ? first_a - 64 : 0;
				SIZE_T to = first_a + 64;
				if (to > buf_size) to = buf_size;
				char hex[400];
				int off = 0;
				off += sprintf(hex + off, "[MetroDev] DEZP near-str@0x%llX: ", (unsigned long long)from);
				for (SIZE_T p = from; p < to && off < 350; p++) {
					off += sprintf(hex + off, "%02X ", buf[p]);
				}
				sprintf(hex + off, "\n");
				OutputDebugStringA(hex);
			}
		}

		// バッファ全域でマーカーのバイトパターンを検索 (chunk_size のみ使用)
		SIZE_T scan_size = buf_size;

		__try {
			int a8_count = 0;
			for (SIZE_T i = 0; i + 12 <= scan_size; i++) {
				if (buf[i] != 0xA8) continue;
				a8_count++;
				if (memcmp(buf + i, PRESET_MARKER, 8) != 0) continue;

				DWORD old_count = *(DWORD*)(buf + i + 8);
				if (old_count > 50000) continue;

				SIZE_T old_block_end = i + 12;
				for (DWORD j = 0; j < old_count && old_block_end < buf_size; j++) {
					while (old_block_end < buf_size && buf[old_block_end] != 0) old_block_end++;
					old_block_end++;
				}
				SIZE_T old_block_size = old_block_end - i;

				char msg[256];
				sprintf(msg, "[MetroDev] DEZP #%d: FOUND marker at slot+0x%X off=0x%llX count=%u oldblock=%llu\n",
					call_num, slot_off, (unsigned long long)i, old_count, (unsigned long long)old_block_size);
				OutputDebugStringA(msg);

				if (old_block_size == full_preset_block_size) {
					memcpy(buf + i, full_preset_block_data, full_preset_block_size);
					sprintf(msg, "[MetroDev] DEZP: in-place replaced (%llu bytes)\n",
						(unsigned long long)full_preset_block_size);
					OutputDebugStringA(msg);
				} else {
					SIZE_T data_after_old = buf_size - (i + old_block_size);
					SIZE_T new_buf_size = i + full_preset_block_size + data_after_old;
					BYTE* new_buf = (BYTE*)VirtualAlloc(NULL, new_buf_size,
						MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
					if (new_buf == nullptr) {
						OutputDebugStringA("[MetroDev] DEZP: VirtualAlloc failed\n");
						return result;
					}
					memcpy(new_buf, buf, i);
					memcpy(new_buf + i, full_preset_block_data, full_preset_block_size);
					memcpy(new_buf + i + full_preset_block_size,
						buf + i + old_block_size, data_after_old);

					*(BYTE**)((BYTE*)output + slot_off) = new_buf;
					*(DWORD*)((BYTE*)output + size_offsets[si]) = (DWORD)new_buf_size;

					sprintf(msg, "[MetroDev] DEZP: realloc %llu -> %llu (+%lld) new_buf=%p\n",
						(unsigned long long)buf_size, (unsigned long long)new_buf_size,
						(long long)(new_buf_size - buf_size), new_buf);
					OutputDebugStringA(msg);
				}

				dezp_preset_patched = true;
				return result;
			}
			char done[256];
			sprintf(done, "[MetroDev] DEZP #%d: scan done slot+0x%X a8_count=%d (no marker)\n",
				call_num, slot_off, a8_count);
			OutputDebugStringA(done);
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			char smsg[256];
			sprintf(smsg, "[MetroDev] DEZP #%d: exception scanning slot+0x%X\n", call_num, slot_off);
			OutputDebugStringA(smsg);
		}
	}

	return result;
}

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

			// DEZP handler: セーブデータ展開時のプリセット注入フック
			{
				DWORD64 dezp_body = FindPatternInEXE(
					"\x48\x63\x42\x20\x48\x8B\xDA\x48\x8B\xF9\x45\x8B\xF8"
					"\x8D\x48\x04\x89\x4A\x20\x48\x8B\xD0\x48\x03\x53\x18"
					"\x81\x3A\x44\x45\x5A\x50",
					"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
				if (dezp_body != NULL) {
					DWORD64 dezp_fn = dezp_body - 0x1C;
					load_full_preset_block();

					MH_STATUS mhs = MH_CreateHook((void*)dezp_fn, (void*)&hook_dezp_handler, (void**)&dezp_handler_orig);
					if (mhs == MH_OK) {
						MH_EnableHook((void*)dezp_fn);
						char msg[128];
						sprintf(msg, "[MetroDev] DEZP: handler hooked at %p\n", (void*)dezp_fn);
						OutputDebugStringA(msg);
					} else {
						char msg[128];
						sprintf(msg, "[MetroDev] DEZP: MH_CreateHook failed (%d)\n", mhs);
						OutputDebugStringA(msg);
					}
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
		OutputDebugStringA("[MetroDev] wpn_give: usage: wpn_give <name> | list [prefix] | presets [filter] | presets give_all | presetscan [N|all] | defscan [N] | scan | dump | debug\n");
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

	// deepscan サブコマンド: エンティティ内のポインタを辿り参照先の文字列ハンドルを探索
	if (strncmp(args, "deepscan", 8) == 0) {
		const char* target = args + 8;
		while (*target == ' ') target++;
		if (*target == '\0') {
			OutputDebugStringA("[MetroDev] deepscan: usage: wpn_give deepscan <name>\n");
			return;
		}

		void* found_ent = nullptr;
		int found_idx = -1;
		const char* found_name = nullptr;
		for (int i = 0; i < 0xFFFF && found_ent == nullptr; i++) {
			void* ent = entity_array[i];
			if (ent == nullptr) continue;
			__try {
				DWORD handle = *(DWORD*)((BYTE*)ent + 0x238);
				if (handle == 0) continue;
				const char* name = resolve_str_handle(handle);
				if (name != nullptr && strstr(name, target) != nullptr) {
					found_ent = ent;
					found_idx = i;
					found_name = name;
				}
			} __except(EXCEPTION_EXECUTE_HANDLER) {}
		}
		if (found_ent == nullptr) {
			sprintf(buf, "[MetroDev] deepscan: '%s' not found\n", target);
			OutputDebugStringA(buf);
			return;
		}

		sprintf(buf, "[MetroDev] deepscan: [%d] '%s' (%p)\n", found_idx, found_name, found_ent);
		OutputDebugStringA(buf);

		BYTE* base = (BYTE*)found_ent;
		MODULEINFO mod;
		GetModuleInformation(GetCurrentProcess(), GetModuleHandle(NULL), &mod, sizeof(mod));
		DWORD64 exe_start = (DWORD64)mod.lpBaseOfDll;
		DWORD64 exe_end = exe_start + mod.SizeOfImage;

		int hit_count = 0;
		for (DWORD off = 0; off < 0x800; off += 8) {
			__try {
				DWORD64 ptr_val = *(DWORD64*)(base + off);
				if (ptr_val < 0x10000 || ptr_val == (DWORD64)found_ent) continue;
				// vtableポインタ（exe範囲内）はスキップ
				if (off == 0) continue;
				// ヒープ上のオブジェクトを想定（exe範囲外）
				BYTE* target_obj = (BYTE*)ptr_val;

				// 参照先オブジェクト内を文字列ハンドルスキャン
				bool found_any = false;
				for (DWORD t_off = 0; t_off < 0x400; t_off += 4) {
					__try {
						DWORD val = *(DWORD*)(target_obj + t_off);
						if (val == 0) continue;
						const char* s = resolve_str_handle(val);
						if (s != nullptr && s[0] != '\0') {
							if (!found_any) {
								sprintf(buf, "[MetroDev]   +0x%03X -> %p:\n", off, target_obj);
								OutputDebugStringA(buf);
								found_any = true;
							}
							sprintf(buf, "[MetroDev]     [+0x%03X] \"%s\"\n", t_off, s);
							OutputDebugStringA(buf);
							hit_count++;
						}
					} __except(EXCEPTION_EXECUTE_HANDLER) { break; }
				}
			} __except(EXCEPTION_EXECUTE_HANDLER) {}
		}
		sprintf(buf, "[MetroDev] deepscan: %d strings found via indirection\n", hit_count);
		OutputDebugStringA(buf);
		return;
	}

	// compare サブコマンド: 2つのエンティティのメモリを比較し差分を表示
	if (strncmp(args, "compare", 7) == 0) {
		const char* a = args + 7;
		while (*a == ' ') a++;
		if (*a == '\0') {
			OutputDebugStringA("[MetroDev] compare: usage: wpn_give compare <name1> <name2>\n");
			return;
		}
		char name1[64], name2[64];
		if (sscanf(a, "%63s %63s", name1, name2) != 2) {
			OutputDebugStringA("[MetroDev] compare: need two names\n");
			return;
		}

		void* ent1 = nullptr; void* ent2 = nullptr;
		int idx1 = -1, idx2 = -1;
		const char* n1 = nullptr; const char* n2 = nullptr;
		for (int i = 0; i < 0xFFFF; i++) {
			void* ent = entity_array[i];
			if (ent == nullptr) continue;
			__try {
				DWORD handle = *(DWORD*)((BYTE*)ent + 0x238);
				if (handle == 0) continue;
				const char* name = resolve_str_handle(handle);
				if (name == nullptr) continue;
				if (ent1 == nullptr && strstr(name, name1) != nullptr) { ent1 = ent; idx1 = i; n1 = name; }
				else if (ent2 == nullptr && strstr(name, name2) != nullptr) { ent2 = ent; idx2 = i; n2 = name; }
			} __except(EXCEPTION_EXECUTE_HANDLER) {}
			if (ent1 != nullptr && ent2 != nullptr) break;
		}
		if (ent1 == nullptr || ent2 == nullptr) {
			sprintf(buf, "[MetroDev] compare: not found (ent1=%p ent2=%p)\n", ent1, ent2);
			OutputDebugStringA(buf);
			return;
		}

		sprintf(buf, "[MetroDev] compare: [%d]'%s' vs [%d]'%s'\n", idx1, n1, idx2, n2);
		OutputDebugStringA(buf);

		BYTE* b1 = (BYTE*)ent1;
		BYTE* b2 = (BYTE*)ent2;
		int diff_count = 0;
		for (DWORD off = 0; off < 0x800; off += 8) {
			__try {
				DWORD64 v1 = *(DWORD64*)(b1 + off);
				DWORD64 v2 = *(DWORD64*)(b2 + off);
				if (v1 == v2) continue;

				// DWORD上位・下位それぞれ文字列ハンドル解決を試みる
				const char* s1_lo = nullptr; const char* s1_hi = nullptr;
				const char* s2_lo = nullptr; const char* s2_hi = nullptr;
				DWORD lo1 = (DWORD)(v1 & 0xFFFFFFFF);
				DWORD hi1 = (DWORD)(v1 >> 32);
				DWORD lo2 = (DWORD)(v2 & 0xFFFFFFFF);
				DWORD hi2 = (DWORD)(v2 >> 32);
				if (lo1 != 0) s1_lo = resolve_str_handle(lo1);
				if (hi1 != 0) s1_hi = resolve_str_handle(hi1);
				if (lo2 != 0) s2_lo = resolve_str_handle(lo2);
				if (hi2 != 0) s2_hi = resolve_str_handle(hi2);

				if (s1_lo || s1_hi || s2_lo || s2_hi) {
					sprintf(buf, "[MetroDev]   +0x%03X: %016llX vs %016llX", off, (unsigned long long)v1, (unsigned long long)v2);
					OutputDebugStringA(buf);
					if (s1_lo || s2_lo) {
						sprintf(buf, "[MetroDev]     +0x%03X(lo): \"%s\" vs \"%s\"", off, s1_lo ? s1_lo : "-", s2_lo ? s2_lo : "-");
						OutputDebugStringA(buf);
					}
					if (s1_hi || s2_hi) {
						sprintf(buf, "[MetroDev]     +0x%03X(hi): \"%s\" vs \"%s\"", off + 4, s1_hi ? s1_hi : "-", s2_hi ? s2_hi : "-");
						OutputDebugStringA(buf);
					}
				} else {
					sprintf(buf, "[MetroDev]   +0x%03X: %016llX vs %016llX", off, (unsigned long long)v1, (unsigned long long)v2);
					OutputDebugStringA(buf);
				}
				diff_count++;
			} __except(EXCEPTION_EXECUTE_HANDLER) {}
		}
		sprintf(buf, "[MetroDev] compare: %d differences in 0x800 bytes\n", diff_count);
		OutputDebugStringA(buf);
		return;
	}

	// fields サブコマンド: エンティティメモリ内の有効な文字列ハンドルを全探索
	if (strncmp(args, "fields", 6) == 0) {
		const char* target = args + 6;
		while (*target == ' ') target++;
		if (*target == '\0') {
			OutputDebugStringA("[MetroDev] fields: usage: wpn_give fields <name>\n");
			return;
		}

		void* found_ent = nullptr;
		int found_idx = -1;
		const char* found_name = nullptr;
		for (int i = 0; i < 0xFFFF && found_ent == nullptr; i++) {
			void* ent = entity_array[i];
			if (ent == nullptr) continue;
			__try {
				DWORD handle = *(DWORD*)((BYTE*)ent + 0x238);
				if (handle == 0) continue;
				const char* name = resolve_str_handle(handle);
				if (name != nullptr && strstr(name, target) != nullptr) {
					found_ent = ent;
					found_idx = i;
					found_name = name;
				}
			} __except(EXCEPTION_EXECUTE_HANDLER) {}
		}
		if (found_ent == nullptr) {
			sprintf(buf, "[MetroDev] fields: '%s' not found\n", target);
			OutputDebugStringA(buf);
			return;
		}

		sprintf(buf, "[MetroDev] fields: scanning [%d] '%s' (%p) for string handles...\n", found_idx, found_name, found_ent);
		OutputDebugStringA(buf);

		BYTE* base = (BYTE*)found_ent;
		int str_count = 0;
		for (DWORD off = 0; off < 0x800; off += 4) {
			__try {
				DWORD val = *(DWORD*)(base + off);
				if (val == 0) continue;
				const char* s = resolve_str_handle(val);
				if (s != nullptr && s[0] != '\0') {
					sprintf(buf, "[MetroDev]   +0x%03X: 0x%08X = \"%s\"\n", off, val, s);
					OutputDebugStringA(buf);
					str_count++;
				}
			} __except(EXCEPTION_EXECUTE_HANDLER) {}
		}
		sprintf(buf, "[MetroDev] fields: %d string handles found\n", str_count);
		OutputDebugStringA(buf);
		return;
	}

	// inspect サブコマンド: vtable RVA とエンティティ名を表示
	if (strncmp(args, "inspect", 7) == 0) {
		const char* target = args + 7;
		while (*target == ' ') target++;
		if (*target == '\0') {
			OutputDebugStringA("[MetroDev] inspect: usage: wpn_give inspect <name|prefix>\n");
			return;
		}

		uintptr_t mod_base = (uintptr_t)GetModuleHandle(NULL);
		int found_count = 0;
		for (int i = 0; i < 0xFFFF && found_count < 30; i++) {
			void* ent = entity_array[i];
			if (ent == nullptr) continue;
			__try {
				DWORD handle = *(DWORD*)((BYTE*)ent + 0x238);
				if (handle == 0) continue;
				const char* name = resolve_str_handle(handle);
				if (name == nullptr || strstr(name, target) == nullptr) continue;

				void** vtable = *(void***)ent;
				DWORD vt_rva = (DWORD)((uintptr_t)vtable - mod_base);

				sprintf(buf, "[MetroDev] inspect [%d] '%s' vt_rva=0x%08X\n", i, name, vt_rva);
				OutputDebugStringA(buf);

				// vtableエントリのRVAを表示（基底クラス共通メソッドの特定用）
				__try {
					char line[512];
					int pos = sprintf(line, "[MetroDev]   vt:");
					for (int v = 0; v < 16; v++) {
						DWORD fn_rva = (DWORD)((uintptr_t)vtable[v] - mod_base);
						pos += sprintf(line + pos, " [%d]=%08X", v, fn_rva);
						if (v == 7) {
							strcat(line, "\n");
							OutputDebugStringA(line);
							pos = sprintf(line, "[MetroDev]   vt:");
						}
					}
					strcat(line, "\n");
					OutputDebugStringA(line);
				} __except(EXCEPTION_EXECUTE_HANDLER) {}
				found_count++;
			} __except(EXCEPTION_EXECUTE_HANDLER) {}
		}
		sprintf(buf, "[MetroDev] inspect: %d entities\n", found_count);
		OutputDebugStringA(buf);
		return;
	}

	// classes サブコマンド: 全エンティティをvtable RVAでグループ化し、各クラスのサンプル名を表示
	if (strcmp(args, "classes") == 0) {
		uintptr_t mod_base = (uintptr_t)GetModuleHandle(NULL);

		struct vt_group {
			DWORD rva;
			int count;
			int first_idx;
			char sample_names[256];
			int sample_len;
		};
		const int MAX_GROUPS = 128;
		vt_group* groups = (vt_group*)malloc(sizeof(vt_group) * MAX_GROUPS);
		if (groups == nullptr) { OutputDebugStringA("[MetroDev] classes: malloc failed\n"); return; }
		int group_count = 0;

		for (int i = 0; i < 0xFFFF; i++) {
			void* ent = entity_array[i];
			if (ent == nullptr) continue;
			__try {
				DWORD handle = *(DWORD*)((BYTE*)ent + 0x238);
				if (handle == 0) continue;
				const char* name = resolve_str_handle(handle);
				if (name == nullptr) continue;

				void* vtable = *(void**)ent;
				DWORD vt_rva = (DWORD)((uintptr_t)vtable - mod_base);

				// 既存グループを検索
				int gi = -1;
				for (int g = 0; g < group_count; g++) {
					if (groups[g].rva == vt_rva) { gi = g; break; }
				}
				if (gi < 0 && group_count < MAX_GROUPS) {
					gi = group_count++;
					groups[gi].rva = vt_rva;
					groups[gi].count = 0;
					groups[gi].first_idx = i;
					groups[gi].sample_names[0] = '\0';
					groups[gi].sample_len = 0;
				}
				if (gi >= 0) {
					groups[gi].count++;
					// 最初の3エンティティ名をサンプルとして記録
					if (groups[gi].count <= 3 && groups[gi].sample_len < 200) {
						if (groups[gi].sample_len > 0) {
							groups[gi].sample_names[groups[gi].sample_len++] = ',';
							groups[gi].sample_names[groups[gi].sample_len++] = ' ';
						}
						int nlen = (int)strlen(name);
						if (nlen > 40) nlen = 40;
						memcpy(groups[gi].sample_names + groups[gi].sample_len, name, nlen);
						groups[gi].sample_len += nlen;
						groups[gi].sample_names[groups[gi].sample_len] = '\0';
					}
				}
			} __except(EXCEPTION_EXECUTE_HANDLER) {}
		}

		// カウント降順でソート
		for (int a = 0; a < group_count - 1; a++) {
			for (int b = a + 1; b < group_count; b++) {
				if (groups[b].count > groups[a].count) {
					vt_group tmp = groups[a];
					groups[a] = groups[b];
					groups[b] = tmp;
				}
			}
		}

		sprintf(buf, "[MetroDev] classes: %d vtable groups found\n", group_count);
		OutputDebugStringA(buf);
		for (int g = 0; g < group_count; g++) {
			sprintf(buf, "[MetroDev]   vt=0x%08X count=%d  e.g. %s\n",
				groups[g].rva, groups[g].count, groups[g].sample_names);
			OutputDebugStringA(buf);
		}
		free(groups);
		return;
	}

	// list サブコマンド: vtable[4]ベースの武器判定 + entity[+0x240]->[+0x008]の武器種別で分類表示
	if (strncmp(args, "list", 4) == 0) {
		const char* filter = nullptr;
		const char* sp = args + 4;
		while (*sp == ' ') sp++;
		if (*sp != '\0') filter = sp;

		// Step1: 既知の武器名からvt[4]の武器判定値を動的取得
		void* weapon_vt4 = nullptr;
		const char* ref_probes[] = { "revolver_", "ak_74_", "ashot_", "shotgun_", nullptr };
		for (int p = 0; ref_probes[p] != nullptr && weapon_vt4 == nullptr; p++) {
			for (int i = 0; i < 0xFFFF && weapon_vt4 == nullptr; i++) {
				void* ent = entity_array[i];
				if (ent == nullptr) continue;
				__try {
					DWORD handle = *(DWORD*)((BYTE*)ent + 0x238);
					if (handle == 0) continue;
					const char* name = resolve_str_handle(handle);
					if (name != nullptr && strncmp(name, ref_probes[p], strlen(ref_probes[p])) == 0) {
						void** vt = *(void***)ent;
						weapon_vt4 = vt[4];
					}
				} __except(EXCEPTION_EXECUTE_HANDLER) {}
			}
		}

		if (weapon_vt4 == nullptr) {
			OutputDebugStringA("[MetroDev] wpn_give list: weapon reference not found (vt[4] unknown)\n");
			return;
		}

		// Step2: 全武器を収集し、entity[+0x240]->[+0x008] から武器種別を取得
		struct wpn_item {
			int index;
			char name[64];
			char weapon_type[64];
		};
		const int MAX_WEAPONS = 512;
		wpn_item* weapons = (wpn_item*)malloc(sizeof(wpn_item) * MAX_WEAPONS);
		if (weapons == nullptr) { OutputDebugStringA("[MetroDev] wpn_give list: malloc failed\n"); return; }
		int weapon_count = 0;

		for (int i = 0; i < 0xFFFF && weapon_count < MAX_WEAPONS; i++) {
			void* ent = entity_array[i];
			if (ent == nullptr) continue;
			__try {
				void** vt = *(void***)ent;
				if (vt[4] != weapon_vt4) continue;

				DWORD handle = *(DWORD*)((BYTE*)ent + 0x238);
				if (handle == 0) continue;
				const char* name = resolve_str_handle(handle);
				if (name == nullptr) continue;
				if (filter != nullptr && strstr(name, filter) == nullptr) continue;

				// 武器種別を取得: entity[+0x240] -> pointer -> [+0x008] -> string handle
				const char* wtype = nullptr;
				__try {
					void* def_ptr = *(void**)((BYTE*)ent + 0x240);
					if (def_ptr != nullptr) {
						DWORD type_handle = *(DWORD*)((BYTE*)def_ptr + 0x008);
						if (type_handle != 0) wtype = resolve_str_handle(type_handle);
					}
				} __except(EXCEPTION_EXECUTE_HANDLER) {}

				weapons[weapon_count].index = i;
				strncpy(weapons[weapon_count].name, name, 63);
				weapons[weapon_count].name[63] = '\0';
				if (wtype != nullptr) {
					strncpy(weapons[weapon_count].weapon_type, wtype, 63);
				} else {
					strncpy(weapons[weapon_count].weapon_type, "(unknown)", 63);
				}
				weapons[weapon_count].weapon_type[63] = '\0';
				weapon_count++;
			} __except(EXCEPTION_EXECUTE_HANDLER) {}
		}

		// Step3: weapon_type → name 順でソート
		for (int a = 0; a < weapon_count - 1; a++) {
			for (int b = a + 1; b < weapon_count; b++) {
				int cmp = strcmp(weapons[a].weapon_type, weapons[b].weapon_type);
				if (cmp == 0) cmp = strcmp(weapons[a].name, weapons[b].name);
				if (cmp > 0) {
					wpn_item tmp = weapons[a];
					weapons[a] = weapons[b];
					weapons[b] = tmp;
				}
			}
		}

		// Step4: グループ化してファイルに出力
		FILE* fList = fopen("wpn_give_list.log", "w");
		if (fList == nullptr) {
			OutputDebugStringA("[MetroDev] wpn_give list: failed to open wpn_give_list.log\n");
			free(weapons);
			return;
		}

		int total = 0;
		int type_count = 0;
		char current_type[64] = "";
		for (int w = 0; w < weapon_count; w++) {
			if (strcmp(weapons[w].weapon_type, current_type) != 0) {
				strncpy(current_type, weapons[w].weapon_type, 63);
				current_type[63] = '\0';
				type_count++;
				int group_size = 0;
				for (int c = w; c < weapon_count && strcmp(weapons[c].weapon_type, current_type) == 0; c++) group_size++;
				fprintf(fList, "=== %s (%d) ===\n", current_type, group_size);
			}
			fprintf(fList, "  [%d] %s\n", weapons[w].index, weapons[w].name);
			total++;
		}
		fprintf(fList, "\n%d weapons in %d types\n", total, type_count);
		fclose(fList);

		free(weapons);
		return;
	}

	// presets サブコマンド
	if (strncmp(args, "presets", 7) == 0 && (args[7] == '\0' || args[7] == ' ')) {
		const char* sub = args + 7;
		while (*sub == ' ') sub++;

		if (strncmp(sub, "dezp_status", 11) == 0) {
			sprintf(buf, "[MetroDev] DEZP: hook=%s block=%s(%llu bytes) patched=%s\n",
				dezp_handler_orig ? "ON" : "OFF",
				full_preset_block_data ? "loaded" : "none",
				(unsigned long long)full_preset_block_size,
				dezp_preset_patched ? "YES" : "NO");
			OutputDebugStringA(buf);
			return;
		}
		if (strncmp(sub, "dezp_reset", 10) == 0) {
			dezp_preset_patched = false;
			OutputDebugStringA("[MetroDev] DEZP: patch flag reset, will patch on next save load\n");
			return;
		}

		if (strncmp(sub, "give_all", 8) == 0) {
			if (g_equip_actor_ptr == nullptr) {
				OutputDebugStringA("[MetroDev] presets give_all: g_equip_actor_ptr not initialized\n");
				return;
			}
			void* actor = *g_equip_actor_ptr;
			if (actor == nullptr) {
				OutputDebugStringA("[MetroDev] presets give_all: equip actor is null\n");
				return;
			}
			if (queue_add_weapon_fn == nullptr) {
				OutputDebugStringA("[MetroDev] presets give_all: queue_add not found\n");
				return;
			}

			int batch_size = 0;
			int batch_offset = 0;
			const char* barg = sub + 8;
			while (*barg == ' ') barg++;
			if (*barg != '\0') {
				sscanf(barg, "%d %d", &batch_size, &batch_offset);
			}

			FILE* fGive = fopen("wpn_give_give_all.log", "w");
			if (fGive == nullptr) OutputDebugStringA("[MetroDev] presets give_all: failed to open wpn_give_give_all.log\n");

			int total_match = 0;
			int given = 0;
			int n_empty = 0, n_broken = 0, n_garbage = 0, n_valid = 0;
			int failed = 0;
			bool limit_reached = false;
			int preset_idx = 0;
			for (int i = 0; i < 0xFFFF; i++) {
				void* ent = entity_array[i];
				if (ent == nullptr) continue;
				__try {
					DWORD handle = *(DWORD*)((BYTE*)ent + 0x238);
					if (handle == 0) continue;
					const char* name = resolve_str_handle(handle);
					if (name == nullptr) continue;
					if (strncmp(name, "$weapon_item_preset_", 20) != 0) continue;

					total_match++;

					// カテゴリ分類 (フィルタはせず、ログのみ)
					const char* category = "valid";
					DWORD uid = 0, v230 = 0;
					void* def_slot = nullptr;
					const char* s230 = nullptr;

					__try { uid = *(DWORD*)((BYTE*)ent + 0x28C); } __except(EXCEPTION_EXECUTE_HANDLER) {}
					__try { def_slot = *(void**)((BYTE*)ent + 0x180); } __except(EXCEPTION_EXECUTE_HANDLER) {}
					__try { v230 = *(DWORD*)((BYTE*)ent + 0x230); } __except(EXCEPTION_EXECUTE_HANDLER) {}
					if (v230 != 0 && v230 < 0x100000) {
						__try {
							const char* s = resolve_str_handle(v230);
							if (s != nullptr && s[0] != '\0' && strlen(s) >= 3 && strlen(s) < 80) s230 = s;
						} __except(EXCEPTION_EXECUTE_HANDLER) {}
					}

					if (uid == 0xFFFFFFFF)        { category = "empty";   n_empty++; }
					else if (def_slot == nullptr) { category = "broken";  n_broken++; }
					else if (s230 != nullptr)     { category = "garbage"; n_garbage++; }
					else                          {                       n_valid++; }

					// action 判定 (フィルタなし: queue or skip(offset/limit))
					const char* action = "queued";
					if (preset_idx < batch_offset) {
						action = "skipped(offset)";
					} else if (batch_size > 0 && given >= batch_size) {
						action = "skipped(limit)";
						limit_reached = true;
					}

					DWORD exc_code = 0;
					int result = 0;
					if (strcmp(action, "queued") == 0) {
						__try {
							result = queue_add_weapon_fn(actor, (unsigned short)i, nullptr, 0, 0);
							given++;
						} __except(exc_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
							failed++;
						}
					}

					if (fGive != nullptr) {
						fprintf(fGive, "[%d] action=%-15s cat=%-7s name=%s uid=%X +180=%p +230=%X%s%s%s",
							i, action, category, name, uid, def_slot, v230,
							s230 ? "(" : "", s230 ? s230 : "", s230 ? ")" : "");
						if (strcmp(action, "queued") == 0) {
							fprintf(fGive, " exc=%X result=%d", exc_code, result);
						}
						fprintf(fGive, "\n");
						fflush(fGive);
					}
					preset_idx++;
				} __except(EXCEPTION_EXECUTE_HANDLER) {}
			}

			if (fGive != nullptr) {
				fprintf(fGive, "\n=== summary ===\ntotal_match=%d queued=%d failed=%d (cats: empty=%d broken=%d garbage=%d valid=%d) batch=%d offset=%d limit_reached=%d\n",
					total_match, given, failed, n_empty, n_broken, n_garbage, n_valid, batch_size, batch_offset, limit_reached ? 1 : 0);
				fclose(fGive);
			}

			sprintf(buf, "[MetroDev] presets give_all: total=%d queued=%d (empty=%d broken=%d garbage=%d valid=%d) failed=%d offset=%d -> wpn_give_give_all.log\n",
				total_match, given, n_empty, n_broken, n_garbage, n_valid, failed, batch_offset);
			OutputDebugStringA(buf);
			return;
		}

		// presets (引数なし/フィルタ): 一覧をファイルに出力
		const char* filter = (*sub != '\0') ? sub : nullptr;

		struct preset_item {
			int index;
			char name[64];
			char preset_type[64];
		};
		const int MAX_PRESETS = 1024;
		preset_item* presets = (preset_item*)malloc(sizeof(preset_item) * MAX_PRESETS);
		if (presets == nullptr) { OutputDebugStringA("[MetroDev] wpn_give presets: malloc failed\n"); return; }
		int preset_count = 0;

		for (int i = 0; i < 0xFFFF && preset_count < MAX_PRESETS; i++) {
			void* ent = entity_array[i];
			if (ent == nullptr) continue;
			__try {
				DWORD handle = *(DWORD*)((BYTE*)ent + 0x238);
				if (handle == 0) continue;
				const char* name = resolve_str_handle(handle);
				if (name == nullptr) continue;
				if (strncmp(name, "$weapon_item_preset_", 20) != 0 &&
					strncmp(name, "$weapon_item_magazine_", 22) != 0) continue;
				if (filter != nullptr && strstr(name, filter) == nullptr) continue;

				const char* ptype = nullptr;
				__try {
					void* def_ptr = *(void**)((BYTE*)ent + 0x240);
					if (def_ptr != nullptr) {
						DWORD type_handle = *(DWORD*)((BYTE*)def_ptr + 0x008);
						if (type_handle != 0) ptype = resolve_str_handle(type_handle);
					}
				} __except(EXCEPTION_EXECUTE_HANDLER) {}

				presets[preset_count].index = i;
				strncpy(presets[preset_count].name, name, 63);
				presets[preset_count].name[63] = '\0';
				if (ptype != nullptr) {
					strncpy(presets[preset_count].preset_type, ptype, 63);
				} else {
					strncpy(presets[preset_count].preset_type, "(unknown)", 63);
				}
				presets[preset_count].preset_type[63] = '\0';
				preset_count++;
			} __except(EXCEPTION_EXECUTE_HANDLER) {}
		}

		for (int a = 0; a < preset_count - 1; a++) {
			for (int b = a + 1; b < preset_count; b++) {
				int cmp = strcmp(presets[a].preset_type, presets[b].preset_type);
				if (cmp == 0) cmp = strcmp(presets[a].name, presets[b].name);
				if (cmp > 0) {
					preset_item tmp = presets[a];
					presets[a] = presets[b];
					presets[b] = tmp;
				}
			}
		}

		FILE* fList = fopen("wpn_give_presets.log", "w");
		if (fList == nullptr) {
			OutputDebugStringA("[MetroDev] wpn_give presets: failed to open wpn_give_presets.log\n");
			free(presets);
			return;
		}

		int total = 0;
		int type_count = 0;
		char current_type[64] = "";
		for (int p = 0; p < preset_count; p++) {
			if (strcmp(presets[p].preset_type, current_type) != 0) {
				strncpy(current_type, presets[p].preset_type, 63);
				current_type[63] = '\0';
				type_count++;
				int group_size = 0;
				for (int c = p; c < preset_count && strcmp(presets[c].preset_type, current_type) == 0; c++) group_size++;
				fprintf(fList, "=== %s (%d) ===\n", current_type, group_size);
			}
			fprintf(fList, "  [%d] %s\n", presets[p].index, presets[p].name);
			total++;
		}
		fprintf(fList, "\n%d presets in %d types\n", total, type_count);
		fclose(fList);

		sprintf(buf, "[MetroDev] wpn_give presets: %d presets in %d types -> wpn_give_presets.log\n", total, type_count);
		OutputDebugStringA(buf);
		free(presets);
		return;
	}

	// defscan サブコマンド: preset entity の +0x180 が指す preset slot 構造体を hex dump し、
	// DWORD毎に string handle 解決を試し、entity 間の差分オフセットを抽出。
	// 目的: preset 設定名 (master block の 1077 件のいずれか) が slot 内のどのオフセット
	// に格納されているかを特定する。
	// (補足: +0x240 は共有クラス記述子で entity 間で完全一致のため、+0x180 を採用)
	// defscan        : 既存give_allフィルタを通った最初の8件をスキャン
	// defscan <N>    : 最大N件
	if (strncmp(args, "defscan", 7) == 0) {
		const char* sp = args + 7;
		while (*sp == ' ') sp++;
		int max_scan = 8;
		if (*sp != '\0') max_scan = atoi(sp);
		if (max_scan <= 0) max_scan = 8;
		if (max_scan > 32) max_scan = 32;

		const DWORD SLOT_DUMP = 0x200;

		FILE* fLog = fopen("wpn_give_defscan.log", "w");
		if (fLog == nullptr) { OutputDebugStringA("[MetroDev] defscan: failed to open log\n"); return; }

		struct slot_ref { int idx; void* ent; void* slot; };
		slot_ref refs[32];
		int ref_count = 0;

		// give_all と同じフィルタで preset entity を収集
		for (int i = 0; i < 0xFFFF && ref_count < max_scan; i++) {
			void* ent = entity_array[i];
			if (ent == nullptr) continue;
			__try {
				DWORD handle = *(DWORD*)((BYTE*)ent + 0x238);
				if (handle == 0) continue;
				const char* name = resolve_str_handle(handle);
				if (name == nullptr) continue;
				if (strncmp(name, "$weapon_item_preset_", 20) != 0) continue;
				DWORD uid = *(DWORD*)((BYTE*)ent + 0x28C);
				if (uid == 0xFFFFFFFF) continue;
				void* slot = *(void**)((BYTE*)ent + 0x180);
				if (slot == nullptr) continue;
				DWORD v230 = *(DWORD*)((BYTE*)ent + 0x230);
				if (v230 != 0 && v230 < 0x100000) {
					const char* s230 = resolve_str_handle(v230);
					if (s230 != nullptr && s230[0] != '\0' && strlen(s230) >= 3 && strlen(s230) < 80) continue;
				}
				refs[ref_count].idx = i;
				refs[ref_count].ent = ent;
				refs[ref_count].slot = slot;
				ref_count++;
			} __except(EXCEPTION_EXECUTE_HANDLER) {}
		}

		fprintf(fLog, "Collected %d preset entity slots (+0x180 deref, filter passed)\n\n", ref_count);

		if (ref_count == 0) { fclose(fLog); return; }

		// Step1: 各 entity の概要 + slot の最初の 0x80 を hex dump
		for (int r = 0; r < ref_count; r++) {
			BYTE* base_e = (BYTE*)refs[r].ent;
			BYTE* base_s = (BYTE*)refs[r].slot;
			DWORD v230 = 0, v28C = 0;
			__try { v230 = *(DWORD*)(base_e + 0x230); v28C = *(DWORD*)(base_e + 0x28C); } __except(EXCEPTION_EXECUTE_HANDLER) {}
			fprintf(fLog, "=== [%d] ent=%p slot=%p e+230=%X e+28C=%X ===\n", refs[r].idx, refs[r].ent, refs[r].slot, v230, v28C);

			for (DWORD off = 0; off < 0x80; off += 16) {
				char line[256];
				int pos = sprintf(line, "  +0x%03X: ", off);
				for (int b = 0; b < 16; b++) {
					__try { pos += sprintf(line + pos, "%02X ", base_s[off + b]); }
					__except(EXCEPTION_EXECUTE_HANDLER) { pos += sprintf(line + pos, "?? "); }
				}
				pos += sprintf(line + pos, " |");
				for (int b = 0; b < 16; b++) {
					__try { BYTE ch = base_s[off + b]; line[pos++] = (ch >= 0x20 && ch < 0x7F) ? ch : '.'; }
					__except(EXCEPTION_EXECUTE_HANDLER) { line[pos++] = '?'; }
				}
				line[pos++] = '|'; line[pos++] = '\n'; line[pos] = '\0';
				fprintf(fLog, "%s", line);
			}
			fprintf(fLog, "\n");
		}

		// Step2: 全 entity 横並び DWORD 解析 (0..0x200) + string 解決
		fprintf(fLog, "=== DWORD analysis: each row = 1 offset, columns = entities ===\n");
		fprintf(fLog, "fmt: +OFF: [idx=v(str)] [idx=v(str)] ...   '*' marks offsets that DIFFER between entities\n\n");
		for (DWORD off = 0; off < SLOT_DUMP; off += 4) {
			// 全 entity で同値かチェック (差分のあるオフセットを優先表示するため)
			DWORD v0 = 0;
			bool all_same = true;
			__try { v0 = *(DWORD*)((BYTE*)refs[0].slot + off); } __except(EXCEPTION_EXECUTE_HANDLER) {}
			for (int r = 1; r < ref_count; r++) {
				DWORD vr = 0;
				__try { vr = *(DWORD*)((BYTE*)refs[r].slot + off); } __except(EXCEPTION_EXECUTE_HANDLER) {}
				if (vr != v0) { all_same = false; break; }
			}

			fprintf(fLog, "+%03X%s:", off, all_same ? "  " : "* ");
			for (int r = 0; r < ref_count; r++) {
				DWORD v = 0;
				__try { v = *(DWORD*)((BYTE*)refs[r].slot + off); } __except(EXCEPTION_EXECUTE_HANDLER) {}
				const char* s = nullptr;
				__try { if (v != 0 && v < 0x200000) s = resolve_str_handle(v); } __except(EXCEPTION_EXECUTE_HANDLER) {}
				bool sv = (s != nullptr && s[0] != '\0' && strlen(s) >= 3 && strlen(s) < 80);
				fprintf(fLog, " [%d=%X", refs[r].idx, v);
				if (sv) fprintf(fLog, "(%s)", s);
				fprintf(fLog, "]");
			}
			fprintf(fLog, "\n");
		}

		fclose(fLog);
		sprintf(buf, "[MetroDev] defscan: %d slots (+0x180) -> wpn_give_defscan.log\n", ref_count);
		OutputDebugStringA(buf);
		return;
	}

	// presetscan サブコマンド: 複数のpreset entityのメモリをhexダンプし差分を表示
	// presetscan        : 既存挙動（10件、ヘックスダンプ＋差分検出）
	// presetscan <N>    : 最大N件で同上
	// presetscan all    : 全件をコンパクト1行/件で出力（FULL/PARTIAL diff用）
	if (strncmp(args, "presetscan", 10) == 0) {
		const char* sp = args + 10;
		while (*sp == ' ') sp++;

		// "all" モード: 全プリセットをコンパクト1行/件で出力
		bool full_mode = (strncmp(sp, "all", 3) == 0 && (sp[3] == '\0' || sp[3] == ' '));

		int max_scan = 10;
		if (!full_mode && *sp != '\0') max_scan = atoi(sp);
		if (max_scan <= 0) max_scan = 10;

		const DWORD DUMP_SIZE = 0x800;

		const char* logname = full_mode ? "wpn_give_presetscan_all.log" : "wpn_give_presetscan.log";
		FILE* fLog = fopen(logname, "w");
		if (fLog == nullptr) { OutputDebugStringA("[MetroDev] presetscan: failed to open log\n"); return; }

		// Step1: プリセットエンティティを収集
		struct preset_ref { int idx; void* ent; };
		const int MAX_REFS = full_mode ? 2048 : 512;
		preset_ref* refs = (preset_ref*)malloc(sizeof(preset_ref) * MAX_REFS);
		if (refs == nullptr) { fclose(fLog); return; }
		int ref_count = 0;

		int collect_limit = full_mode ? MAX_REFS : max_scan;
		for (int i = 0; i < 0xFFFF && ref_count < MAX_REFS && ref_count < collect_limit; i++) {
			void* ent = entity_array[i];
			if (ent == nullptr) continue;
			__try {
				DWORD handle = *(DWORD*)((BYTE*)ent + 0x238);
				if (handle == 0) continue;
				const char* name = resolve_str_handle(handle);
				if (name == nullptr) continue;
				if (strncmp(name, "$weapon_item_preset_", 20) != 0) continue;
				refs[ref_count].idx = i;
				refs[ref_count].ent = ent;
				ref_count++;
			} __except(EXCEPTION_EXECUTE_HANDLER) {}
		}

		fprintf(fLog, "Collected %d preset entities\n\n", ref_count);

		// full_mode: コンパクト1行/件のみ出力（FULL/PARTIAL diff用）
		if (full_mode) {
			fprintf(fLog, "=== Compact dump (1 line per preset) ===\n");
			fprintf(fLog, "fmt: [idx] +230=v(str) +234=v +288=v +28C=v +648=v +64C=v +244=v +180=ptr type=...\n\n");
			DWORD koff[] = { 0x230, 0x234, 0x288, 0x28C, 0x648, 0x64C, 0x244 };
			for (int r = 0; r < ref_count; r++) {
				BYTE* base_r = (BYTE*)refs[r].ent;
				fprintf(fLog, "[%d]", refs[r].idx);
				for (int k = 0; k < sizeof(koff) / sizeof(koff[0]); k++) {
					__try {
						DWORD v = *(DWORD*)(base_r + koff[k]);
						const char* s = nullptr;
						__try { if (v != 0 && v < 0x100000) s = resolve_str_handle(v); } __except(EXCEPTION_EXECUTE_HANDLER) {}
						bool sv = (s != nullptr && s[0] != '\0' && strlen(s) >= 3 && strlen(s) < 80);
						fprintf(fLog, " +%03X=%X", koff[k], v);
						if (sv) fprintf(fLog, "(%s)", s);
					} __except(EXCEPTION_EXECUTE_HANDLER) { fprintf(fLog, " +%03X=??", koff[k]); }
				}
				// +0x180 ptr (parent slot)
				__try {
					void* p180 = *(void**)(base_r + 0x180);
					fprintf(fLog, " +180=%p", p180);
				} __except(EXCEPTION_EXECUTE_HANDLER) { fprintf(fLog, " +180=??"); }
				// type derived from +0x240->+0x008
				const char* ptype = nullptr;
				__try {
					void* def = *(void**)(base_r + 0x240);
					if (def != nullptr) {
						DWORD th = *(DWORD*)((BYTE*)def + 0x008);
						if (th != 0) ptype = resolve_str_handle(th);
					}
				} __except(EXCEPTION_EXECUTE_HANDLER) {}
				fprintf(fLog, " type=%s", ptype ? ptype : "?");
				fprintf(fLog, "\n");
			}
			free(refs);
			fclose(fLog);
			sprintf(buf, "[MetroDev] presetscan all: %d presets -> %s\n", ref_count, logname);
			OutputDebugStringA(buf);
			return;
		}

		// Step2: 最初のエンティティのフルhexダンプ
		if (ref_count > 0) {
			fprintf(fLog, "=== Full dump of [%d] ent=%p ===\n", refs[0].idx, refs[0].ent);
			BYTE* base = (BYTE*)refs[0].ent;
			for (DWORD off = 0; off < DUMP_SIZE; off += 16) {
				char line[256];
				int pos = sprintf(line, "  +0x%03X: ", off);
				for (int b = 0; b < 16; b++) {
					__try {
						pos += sprintf(line + pos, "%02X ", base[off + b]);
					} __except(EXCEPTION_EXECUTE_HANDLER) {
						pos += sprintf(line + pos, "?? ");
					}
				}
				pos += sprintf(line + pos, " |");
				for (int b = 0; b < 16; b++) {
					__try {
						BYTE ch = base[off + b];
						line[pos++] = (ch >= 0x20 && ch < 0x7F) ? ch : '.';
					} __except(EXCEPTION_EXECUTE_HANDLER) {
						line[pos++] = '?';
					}
				}
				line[pos++] = '|';
				line[pos++] = '\n';
				line[pos] = '\0';
				fprintf(fLog, "%s", line);
			}
			fprintf(fLog, "\n");
		}

		// Step3: 差分のあるオフセットのDWORD値を文字列ハンドルとして解決
		if (ref_count >= 2) {
			fprintf(fLog, "=== Resolving differing DWORD values as string handles ===\n");
			BYTE* base0 = (BYTE*)refs[0].ent;
			BYTE* base1 = (BYTE*)refs[1].ent;

			// まず差分のあるオフセットを収集
			for (DWORD off = 0; off < DUMP_SIZE; off += 4) {
				__try {
					DWORD v0 = *(DWORD*)(base0 + off);
					DWORD v1 = *(DWORD*)(base1 + off);
					if (v0 == v1) continue;

					const char* s0 = nullptr;
					const char* s1 = nullptr;
					__try { if (v0 != 0) s0 = resolve_str_handle(v0); } __except(EXCEPTION_EXECUTE_HANDLER) {}
					__try { if (v1 != 0) s1 = resolve_str_handle(v1); } __except(EXCEPTION_EXECUTE_HANDLER) {}

					bool s0_valid = (s0 != nullptr && s0[0] != '\0' && strlen(s0) >= 3);
					bool s1_valid = (s1 != nullptr && s1[0] != '\0' && strlen(s1) >= 3);

					if (s0_valid || s1_valid) {
						fprintf(fLog, "  +0x%03X: 0x%08X='%s' vs 0x%08X='%s'\n",
							off, v0, s0_valid ? s0 : "?", v1, s1_valid ? s1 : "?");
					}
				} __except(EXCEPTION_EXECUTE_HANDLER) {}
			}

			// 全プリセットの候補オフセットの値を表示
			fprintf(fLog, "\n=== All presets: key offsets ===\n");
			DWORD check_offsets[] = { 0x230, 0x234, 0x288, 0x28C, 0x648, 0x64C, 0x380, 0x384 };
			for (int r = 0; r < ref_count; r++) {
				BYTE* base_r = (BYTE*)refs[r].ent;
				fprintf(fLog, "[%d] ent=%p:", refs[r].idx, refs[r].ent);
				for (int k = 0; k < sizeof(check_offsets) / sizeof(check_offsets[0]); k++) {
					__try {
						DWORD v = *(DWORD*)(base_r + check_offsets[k]);
						const char* s = nullptr;
						__try { if (v != 0) s = resolve_str_handle(v); } __except(EXCEPTION_EXECUTE_HANDLER) {}
						bool sv = (s != nullptr && s[0] != '\0' && strlen(s) >= 3);
						fprintf(fLog, " +0x%03X=0x%X", check_offsets[k], v);
						if (sv) fprintf(fLog, "('%s')", s);
					} __except(EXCEPTION_EXECUTE_HANDLER) {}
				}
				fprintf(fLog, "\n");
			}
		}

		free(refs);
		fclose(fLog);
		sprintf(buf, "[MetroDev] presetscan: %d presets -> wpn_give_presetscan.log\n", ref_count);
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

#ifdef _WIN64
// バックグラウンドスキャン用パラメータ (1ジョブのみ同時実行)
struct find_str_job {
	char pattern[256];
	int pat_len;
	int max_hits;
	int ctx_size;
	bool rw_only;
	int max_region_mb;  // 1領域の上限 MB (0=無制限)
	bool aborted;
};
static find_str_job g_find_job;
static volatile LONG g_find_running = 0;
static HANDLE g_find_thread = nullptr;

static DWORD WINAPI find_str_thread_proc(LPVOID lp)
{
	FILE* fLog = fopen("find_str.log", "w");
	if (fLog == nullptr) {
		OutputDebugStringA("[MetroDev] find_str: failed to open log\n");
		InterlockedExchange(&g_find_running, 0);
		return 1;
	}

	fprintf(fLog, "find_str: pattern=%s len=%d max=%d ctx=%d rw_only=%d max_region_mb=%d\n",
		g_find_job.pattern, g_find_job.pat_len, g_find_job.max_hits,
		g_find_job.ctx_size, g_find_job.rw_only ? 1 : 0, g_find_job.max_region_mb);
	fflush(fLog);

	SYSTEM_INFO si; GetSystemInfo(&si);
	BYTE* addr = (BYTE*)si.lpMinimumApplicationAddress;
	BYTE* max_addr = (BYTE*)si.lpMaximumApplicationAddress;
	int region_count = 0;
	int region_skipped_size = 0;
	int hit_count = 0;
	SIZE_T total_scanned = 0;
	DWORD t_start = GetTickCount();
	BYTE first = (BYTE)g_find_job.pattern[0];
	const SIZE_T size_limit = g_find_job.max_region_mb > 0 ?
		(SIZE_T)g_find_job.max_region_mb * 1024 * 1024 : 0;

	while (addr < max_addr && hit_count < g_find_job.max_hits && !g_find_job.aborted) {
		MEMORY_BASIC_INFORMATION mbi;
		if (VirtualQuery(addr, &mbi, sizeof(mbi)) == 0) break;
		BYTE* next = (BYTE*)mbi.BaseAddress + mbi.RegionSize;
		if (next <= addr) {
			// 無限ループ防止: 進まなければ 1ページ進める
			fprintf(fLog, "  [warn] VirtualQuery non-progress at 0x%p, advancing 1 page\n", addr);
			fflush(fLog);
			next = addr + 0x1000;
		}

		bool readable = (mbi.State == MEM_COMMIT) &&
			(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY)) != 0 &&
			(mbi.Protect & PAGE_GUARD) == 0 &&
			(mbi.Protect & PAGE_NOACCESS) == 0;
		bool is_image = (mbi.Type == MEM_IMAGE);
		bool is_writable = (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY)) != 0;

		if (!readable) { addr = next; continue; }
		if (g_find_job.rw_only && (is_image || !is_writable)) { addr = next; continue; }
		if (size_limit > 0 && mbi.RegionSize > size_limit) {
			region_skipped_size++;
			addr = next; continue;
		}

		region_count++;
		BYTE* base = (BYTE*)mbi.BaseAddress;
		SIZE_T size = mbi.RegionSize;
		total_scanned += size;

		// 進捗ログ (32領域ごと)
		if ((region_count & 31) == 0) {
			fprintf(fLog, "  [progress] region=%d hits=%d scanned=%.1f MB elapsed=%.1f s\n",
				region_count, hit_count, total_scanned / 1024.0 / 1024.0,
				(GetTickCount() - t_start) / 1000.0);
			fflush(fLog);
		}

		__try {
			SIZE_T limit = size - g_find_job.pat_len;
			for (SIZE_T i = 0; i <= limit; i++) {
				if (base[i] != first) continue;
				if (memcmp(base + i, g_find_job.pattern, g_find_job.pat_len) != 0) continue;

				BYTE* hit = base + i;
				BYTE* ctx_start = hit > base + g_find_job.ctx_size ? hit - g_find_job.ctx_size : base;
				int pre_len = (int)(hit - ctx_start);
				int post_len = g_find_job.ctx_size;
				if ((SIZE_T)(i + g_find_job.pat_len + post_len) > size)
					post_len = (int)(size - i - g_find_job.pat_len);

				char ascii[200] = { 0 };
				char hexbuf[400] = { 0 };
				int total = pre_len + g_find_job.pat_len + post_len;
				if (total > 80) total = 80;
				int hp = 0, ap = 0;
				for (int k = 0; k < total; k++) {
					BYTE b = ctx_start[k];
					hp += sprintf(hexbuf + hp, "%02X ", b);
					ascii[ap++] = (b >= 32 && b < 127) ? (char)b : '.';
				}
				ascii[ap] = '\0';

				fprintf(fLog, "@0x%p [%s%c] pre=%d post=%d  %s| %s\n",
					hit,
					is_image ? "img" : (is_writable ? "rw" : "r-"),
					(mbi.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) ? 'x' : ' ',
					pre_len, post_len, hexbuf, ascii);
				fflush(fLog);

				hit_count++;
				if (hit_count >= g_find_job.max_hits) break;
				i += g_find_job.pat_len - 1;
			}
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			fprintf(fLog, "[exception in region 0x%p, size=0x%zX]\n", base, size);
		}

		addr = next;
	}

	fprintf(fLog, "\nfind_str: DONE %d hits / %d regions / %d size-skipped (total %.1f MB / %.1f s)%s\n",
		hit_count, region_count, region_skipped_size,
		total_scanned / 1024.0 / 1024.0,
		(GetTickCount() - t_start) / 1000.0,
		g_find_job.aborted ? "  [ABORTED]" : "");
	fclose(fLog);

	char msg[256];
	sprintf(msg, "[MetroDev] find_str '%s' done: %d hits / %d regions in %.1f s\n",
		g_find_job.pattern, hit_count, region_count, (GetTickCount() - t_start) / 1000.0);
	OutputDebugStringA(msg);

	InterlockedExchange(&g_find_running, 0);
	return 0;
}

// runtime メモリスキャン: バックグラウンド thread で実行 (即座に return)
// 使い方:
//   find_str silencer_044
//   find_str silencer_044 max=100      （最大ヒット数, デフォルト 200）
//   find_str silencer_044 ctx=24       （周辺表示 byte 数, デフォルト 16, 上限 64）
//   find_str silencer_044 rw           （RW image 以外のみ。書込可能データ領域に絞る）
//   find_str silencer_044 rmb=64       （1領域の上限 MB, デフォルト 64。0=無制限）
//   find_str status                    （実行中ジョブの状態確認）
//   find_str abort                     （実行中ジョブの中断要求）
void __fastcall RestoreCommands::find_str_execute(void* _this, const char* args)
{
	if (args == nullptr || *args == '\0') {
		OutputDebugStringA("[MetroDev] find_str: usage: find_str <pattern> [max=N] [ctx=N] [rw] [rmb=N] | status | abort\n");
		return;
	}

	const char* p = args;
	while (*p == ' ') p++;

	if (strncmp(p, "status", 6) == 0) {
		char msg[128];
		sprintf(msg, "[MetroDev] find_str: running=%ld\n", g_find_running);
		OutputDebugStringA(msg);
		return;
	}
	if (strncmp(p, "abort", 5) == 0) {
		g_find_job.aborted = true;
		OutputDebugStringA("[MetroDev] find_str: abort requested\n");
		return;
	}

	if (InterlockedCompareExchange(&g_find_running, 1, 0) != 0) {
		OutputDebugStringA("[MetroDev] find_str: already running, use 'find_str abort' to cancel\n");
		return;
	}

	const char* pat_begin = p;
	while (*p != '\0' && *p != ' ') p++;
	int pat_len = (int)(p - pat_begin);
	if (pat_len <= 0 || pat_len > 255) {
		OutputDebugStringA("[MetroDev] find_str: invalid pattern length\n");
		InterlockedExchange(&g_find_running, 0);
		return;
	}

	memcpy(g_find_job.pattern, pat_begin, pat_len);
	g_find_job.pattern[pat_len] = '\0';
	g_find_job.pat_len = pat_len;
	g_find_job.max_hits = 200;
	g_find_job.ctx_size = 16;
	g_find_job.rw_only = false;
	g_find_job.max_region_mb = 512;  // 1領域の上限。0=無制限
	g_find_job.aborted = false;

	while (*p != '\0') {
		while (*p == ' ') p++;
		if (*p == '\0') break;
		if (strncmp(p, "max=", 4) == 0) { g_find_job.max_hits = atoi(p + 4); if (g_find_job.max_hits <= 0) g_find_job.max_hits = 200; }
		else if (strncmp(p, "ctx=", 4) == 0) { g_find_job.ctx_size = atoi(p + 4); if (g_find_job.ctx_size < 0) g_find_job.ctx_size = 0; if (g_find_job.ctx_size > 64) g_find_job.ctx_size = 64; }
		else if (strncmp(p, "rmb=", 4) == 0) { g_find_job.max_region_mb = atoi(p + 4); if (g_find_job.max_region_mb < 0) g_find_job.max_region_mb = 0; }
		else if (strncmp(p, "rw", 2) == 0) { g_find_job.rw_only = true; }
		while (*p != '\0' && *p != ' ') p++;
	}

	if (g_find_thread != nullptr) { CloseHandle(g_find_thread); g_find_thread = nullptr; }
	g_find_thread = CreateThread(nullptr, 0, &find_str_thread_proc, nullptr, 0, nullptr);
	if (g_find_thread == nullptr) {
		OutputDebugStringA("[MetroDev] find_str: CreateThread failed\n");
		InterlockedExchange(&g_find_running, 0);
		return;
	}

	char msg[256];
	sprintf(msg, "[MetroDev] find_str '%s' started in background -> find_str.log (max=%d, rw=%d, rmb=%d MB)\n",
		g_find_job.pattern, g_find_job.max_hits, g_find_job.rw_only ? 1 : 0, g_find_job.max_region_mb);
	OutputDebugStringA(msg);
}

// 指定アドレスから N byte をダンプ (hex+ASCII)
// 使い方:
//   dump_mem 0x123456789ABC
//   dump_mem 0x123456789ABC 512
void __fastcall RestoreCommands::dump_mem_execute(void* _this, const char* args)
{
	if (args == nullptr || *args == '\0') {
		OutputDebugStringA("[MetroDev] dump_mem: usage: dump_mem <hex_addr> [size]\n");
		return;
	}
	const char* p = args;
	while (*p == ' ') p++;
	if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;

	uintptr_t addr = 0;
	while (*p != '\0' && *p != ' ') {
		char c = *p++;
		uintptr_t d;
		if (c >= '0' && c <= '9') d = c - '0';
		else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
		else { OutputDebugStringA("[MetroDev] dump_mem: invalid hex addr\n"); return; }
		addr = (addr << 4) | d;
	}
	while (*p == ' ') p++;
	int size = 256;
	if (*p != '\0') { size = atoi(p); if (size <= 0 || size > 0x10000) size = 256; }

	BYTE* base = (BYTE*)addr;
	MEMORY_BASIC_INFORMATION mbi;
	if (VirtualQuery(base, &mbi, sizeof(mbi)) == 0 || mbi.State != MEM_COMMIT ||
		(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY)) == 0 ||
		(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
		char msg[128];
		sprintf(msg, "[MetroDev] dump_mem: 0x%p not readable\n", base);
		OutputDebugStringA(msg);
		return;
	}

	// 領域内に収まるよう調整
	BYTE* region_end = (BYTE*)mbi.BaseAddress + mbi.RegionSize;
	if (base + size > region_end) size = (int)(region_end - base);
	if (size <= 0) return;

	FILE* fLog = fopen("dump_mem.log", "w");
	if (fLog == nullptr) { OutputDebugStringA("[MetroDev] dump_mem: failed to open log\n"); return; }

	fprintf(fLog, "dump_mem: addr=0x%p size=%d region_base=0x%p region_size=0x%zX prot=0x%X\n",
		base, size, mbi.BaseAddress, mbi.RegionSize, mbi.Protect);

	__try {
		for (int row = 0; row < size; row += 16) {
			char hexbuf[64] = { 0 }, ascii[20] = { 0 };
			int hp = 0, ap = 0;
			for (int c = 0; c < 16 && row + c < size; c++) {
				BYTE b = base[row + c];
				hp += sprintf(hexbuf + hp, "%02X ", b);
				ascii[ap++] = (b >= 32 && b < 127) ? (char)b : '.';
			}
			ascii[ap] = '\0';
			fprintf(fLog, "  +0x%04X  %-48s |%s|\n", row, hexbuf, ascii);
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		fprintf(fLog, "[exception during dump]\n");
	}

	fclose(fLog);
	char msg[128];
	sprintf(msg, "[MetroDev] dump_mem 0x%p (%d bytes) -> dump_mem.log\n", base, size);
	OutputDebugStringA(msg);
}
#endif

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

		// runtime メモリ解析用 (アタッチメント保持構造特定のため)
		if (Utils::isExodus()) {
			find_str_new.construct(&find_str_vftable_exodus, pCmd1->__vftable, "find_str", &find_str_execute);
			cu.command_add(&find_str_new);
			dump_mem_new.construct(&dump_mem_vftable_exodus, pCmd1->__vftable, "dump_mem", &dump_mem_execute);
			cu.command_add(&dump_mem_new);
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
