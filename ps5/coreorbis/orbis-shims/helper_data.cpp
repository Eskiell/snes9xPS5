// Snes9x PS5: Snes9xPS5-helper.elf built into the app (ProsperoJailbreak.h). Only the native eboot links
// this file; the Makefile passes HELPER_ELF, the helper payload's path.
//
// SPDX-License-Identifier: MIT

#include "ProsperoJailbreak.h"

__asm__(".section .rodata\n"
		".balign 16\n"
		".global s9x_helper_elf_begin\n"
		"s9x_helper_elf_begin:\n"
		".incbin \"" HELPER_ELF "\"\n"
		".global s9x_helper_elf_end\n"
		"s9x_helper_elf_end:\n"
		".previous\n");
extern "C" const unsigned char s9x_helper_elf_begin[];
extern "C" const unsigned char s9x_helper_elf_end[];

namespace jailbreak
{
Blob EmbeddedHelper()
{
	return {s9x_helper_elf_begin, size_t(s9x_helper_elf_end - s9x_helper_elf_begin)};
}
} // namespace jailbreak
