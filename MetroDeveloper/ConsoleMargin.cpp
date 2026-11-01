#include "ConsoleMargin.h"
#include "Utils.h"

ConsoleMargin::ConsoleMargin()
{
#ifdef _WIN64
	if (!Utils::isRedux()) {
		return;
	}

	float margin = Utils::GetFloat("other", "console_bottom_margin", 0.0f);
	if (margin <= 0.0f) {
		return;
	}

	// uconsole::render: y = 画面の高さ - フォントの高さ (xmm6) の直後に余白を引く
	// subss xmm6, xmm0; xorps xmm1, xmm1; mov rcx, rdi; movaps xmm2, xmm6 - Redux NextGen
	const BYTE pat_nextgen[] = {
		0xF3, 0x0F, 0x5C, 0xF0, 0x0F, 0x57, 0xC9, 0x48, 0x8B, 0xCF, 0x0F, 0x28, 0xD6
	};
	const char msk_nextgen[] = "xxxxxxxxxxxxx";
	// subss xmm6, xmm0; movaps xmm2, xmm6; call [rdi+A8]; mov rcx, [rsi+20B8]; mov edx, FF8080FF - Redux STEAM
	const BYTE pat_steam[] = {
		0xF3, 0x0F, 0x5C, 0xF0, 0x0F, 0x28, 0xD6, 0xFF, 0x97, 0xA8, 0x00, 0x00, 0x00,
		0x48, 0x8B, 0x8E, 0xB8, 0x20, 0x00, 0x00, 0xBA, 0xFF, 0x80, 0x80, 0xFF
	};
	const char msk_steam[] = "xxxxxxxxxxxxxxxxxxxxxxxxx";

	DWORD64 site = FindPatternInEXE((char*)pat_nextgen, (char*)msk_nextgen);
	if (site == 0) {
		site = FindPatternInEXE((char*)pat_steam, (char*)msk_steam);
	}
	if (site == 0) return;

	// 上書きする命令は rip 相対ではないので、そのままスタブへ移せる
	const int stolen = 7;
	const int margin_offset = 0x20;

	// rel32 が届く範囲にスタブを置く
	BYTE* stub = nullptr;
	for (DWORD64 addr = (site & ~0xFFFFull) - 0x10000; addr > site - 0x7FF00000; addr -= 0x10000) {
		stub = (BYTE*)VirtualAlloc((LPVOID)addr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (stub != nullptr) break;
	}
	if (stub == nullptr) return;

	// subss xmm6, [margin]
	const BYTE subss[] = { 0xF3, 0x0F, 0x5C, 0x35 };
	memcpy(stub, subss, sizeof(subss));
	*(int*)(stub + 4) = margin_offset - 8;
	memcpy(stub + 8, (void*)site, stolen);
	// jmp site + stolen
	stub[8 + stolen] = 0xE9;
	*(int*)(stub + 8 + stolen + 1) = (int)((site + stolen) - (DWORD64)(stub + 8 + stolen + 5));
	*(float*)(stub + margin_offset) = margin;

	DWORD old_protect;
	VirtualProtect(stub, 0x1000, PAGE_EXECUTE_READ, &old_protect);

	// jmp stub; nop; nop
	BYTE patch[7] = { 0xE9, 0, 0, 0, 0, 0x90, 0x90 };
	*(int*)(patch + 1) = (int)((DWORD64)stub - (site + 5));
	ASMWrite((LPVOID)site, patch, sizeof(patch));
#endif
}
