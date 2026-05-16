#include "DisableWeaponJam.h"
#include "Utils.h"

DisableWeaponJam::DisableWeaponJam()
{
#ifdef _WIN64
	if (!Utils::isExodus() || !Utils::isExodusEE) {
		return;
	}

	if (!Utils::GetBool("other", "disable_weapon_jam", false)) {
		return;
	}

	const BYTE pat[] = {
		0xF3, 0x0F, 0x10, 0x31,
		0x48, 0x8B, 0x90, 0x00, 0x00, 0x00, 0x00,
		0x48, 0x8D, 0x05
	};
	const char msk[] = "xxxxxxx??xxxxx";
	DWORD64 site = FindPatternInEXE((char*)pat, (char*)msk);
	if (site == 0) return;

	BYTE xorps_xmm6[4] = { 0x0F, 0x57, 0xF6, 0x90 };
	ASMWrite((LPVOID)site, xorps_xmm6, sizeof(xorps_xmm6));
#endif
}