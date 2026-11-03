#pragma once

// Prevent the Redux Next-Gen keyboard helper from repeatedly switching layouts.
#ifdef _WIN64
#include <intrin.h>
#include <cstring>
#include "Utils.h"
#include "MinHook/MinHook.h"

namespace ReduxImeFix {
typedef HKL (WINAPI* ActivateFn)(HKL, UINT);
static ActivateFn original = nullptr;
static uintptr_t base = 0;
static HKL WINAPI Hook(HKL requested, UINT flags)
{
    const uintptr_t caller = (uintptr_t)_ReturnAddress() - base;
    if (caller != 0x948A1C && caller != 0x948A75)
        return original(requested, flags);

    // Report the current layout as the previous one without activating another.
    const DWORD lastError = GetLastError();
    const HKL current = GetKeyboardLayout(0);
    SetLastError(lastError);
    return current;
}

static void Install()
{
    if (!Utils::isRedux() || !Utils::isReduxNextGen) return;

    base = (uintptr_t)GetModuleHandleA(nullptr);
    const auto dos = (const IMAGE_DOS_HEADER*)base;
    const auto nt = (const IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    // Both call sites belong to the same keyboard-property helper. Fail closed
    // on another build rather than changing unrelated calls to this API.
    const BYTE first[] = {0x48,0x8B,0x8D,0x40,0x06,0,0,0x33,0xD2,0xFF,0x15};
    const BYTE second[] = {0x33,0xD2,0x48,0x8B,0xCF,0xFF,0x15};
    const auto api = GetProcAddress(GetModuleHandleA("user32.dll"), "ActivateKeyboardLayout");
    bool valid = nt->OptionalHeader.SizeOfImage >= 0x948A8E && api != nullptr;
    if (valid) {
        valid = memcmp((void*)(base + 0x948A0D), first, sizeof(first)) == 0 &&
            memcmp((void*)(base + 0x948A6A), second, sizeof(second)) == 0;
        const uintptr_t sites[] = {0x948A16, 0x948A6F};
        for (const uintptr_t site : sites) {
            const auto slot = base + site + 6 + *(const INT32*)(base + site + 2);
            valid = valid && slot >= base && slot <= base + nt->OptionalHeader.SizeOfImage - sizeof(void*);
            if (valid) valid = *(const FARPROC*)slot == api;
        }
    }
    if (!valid) return;

    if (MH_CreateHook((void*)api, (void*)&Hook, (void**)&original) == MH_OK &&
        MH_EnableHook((void*)api) != MH_OK) {
        MH_RemoveHook((void*)api);
    }
}
}
#else
namespace ReduxImeFix { static void Install() {} }
#endif
