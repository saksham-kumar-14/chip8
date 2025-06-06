# CHIP8 EMULATION
- Read more about on [Wiki](https://en.wikipedia.org/wiki/CHIP-8)
- [CHIP8 technical references](http://devernay.free.fr/hacks/chip8/C8TECH10.HTM)
### Hardware
- Memory of 0xFFF bytes
- 16 8-Bit data registers, called V0 to VF. VF doubles as the carry flag
- 16 Bit address register I which is used to access memory
- 16 Bit program counter and a stack  

most programs written for the original system begin at memory location 512 (0x200) and do not access any of the memory below the location 512 (0x200). The uppermost 256 bytes (0xF00-0xFFF) are reserved for display refresh, and the 96 bytes below that (0xEA0-0xEFF) were reserved for the call stack, internal use, and other variables.

In modern CHIP-8 implementations, where the interpreter is running natively outside the 4K memory space, there is no need to avoid the lower 512 bytes of memory (0x000-0x1FF), and it is common to store font data there.
