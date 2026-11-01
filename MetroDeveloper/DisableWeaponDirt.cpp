#include "DisableWeaponDirt.h"
#include "Utils.h"

DisableWeaponDirt::DisableWeaponDirt()
{
#ifdef _WIN64
	if (!Utils::isExodus() || !Utils::isExodusEE) {
		return;
	}

	if (!Utils::GetBool("other", "disable_weapon_dirt", false)) {
		return;
	}

	// movzx ecx, word ptr [rcx+000000B0]
	const BYTE pat[] = {
		0x0F, 0xB7, 0x89, 0xB0, 0x00, 0x00, 0x00,
		0x48, 0x89
	};
	const char msk[] = "xxxxxxxxx";
	DWORD64 site = FindPatternInEXE((char*)pat, (char*)msk);
	if (site == 0) return;

	// xor ecx, ecx (2 bytes) + nop x5 = 7 bytes
	BYTE patch[7] = { 0x31, 0xC9, 0x90, 0x90, 0x90, 0x90, 0x90 };
	ASMWrite((LPVOID)site, patch, sizeof(patch));
#endif
}