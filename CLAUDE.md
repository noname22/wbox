# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

WBox is a hybrid Windows PC emulator that uses an x86 CPU emulator for Windows user space applications while implementing kernel space as native host code. It targets Windows XP-era userlands (ReactOS, Wine, NT4, 2000, XP).

## Build Commands

```bash
# Configure and build (from project root)
mkdir -p build && cd build
cmake ..
make -j$(nproc)

# Run the emulator (requires drive mapping for DLL loading)
./wbox --gui -C: /path/to/reactos/root /path/to/app.exe

# Run tests
ctest

# Run a single CPU test
./cpu_tests -f /path/to/test.MOO.gz
```

**Dependencies:** SDL3, CMake 3.16+, zlib (optional, for test compression)

## Architecture

### Execution Model
- **User space**: Emulated via 86Box-derived P6 CPU core with dynamic recompilation (x86-64 and ARM64 backends)
- **Kernel space**: Native host code intercepts syscalls via SYSENTER and implements NT/Win32k APIs

### Key Subsystems

**CPU (`src/cpu/`)**: x86 emulation from 86Box with dynarec codegen. The `codegen/` subdirectory contains the JIT compiler with architecture-specific backends.

**VM (`src/vm/`)**: Virtual machine manager handling protected mode setup, paging, GDT/IDT configuration, and PE loading. Memory layout:
- 0x00400000: Executable image
- 0x01000000: Desktop heap (user-accessible WND/CLS structures)
- 0x04000000-0x08000000: User stack
- 0x10000000: Process heap
- 0x7C000000+: DLLs (kernel32, ntdll, user32, etc.)
- 0x7FFE0000: KUSER_SHARED_DATA

**NT Subsystem (`src/nt/`)**: NT syscall implementations. `syscalls.h` defines syscall numbers 0-295 for NT APIs. Win32k syscalls start at 0x1000.

**USER Subsystem (`src/user/`)**: Win32k USER implementation including:
- Window management (`user_window.c`)
- Window class registration (`user_class.c`)
- Desktop heap for guest-accessible structures (`desktop_heap.c`, `guest_wnd.c`, `guest_cls.c`)
- WndProc callback mechanism (`user_callback.c`)
- USER handle table (`user_handle_table.c`)

**GDI Subsystem (`src/gdi/`)**: Graphics device interface with DC management, drawing primitives, and SDL3-based display.

**Loader (`src/loader/`)**: PE loader with DLL import resolution, export parsing, and stub generation for unimplemented functions.

### Guest Memory Structures

The emulator maintains dual host/guest representations for USER objects:
- `WBOX_WND` (host) ↔ WND (guest at `guest_wnd_va`)
- `WBOX_CLS` (host) ↔ CLS (guest at `guest_cls_va`)

Guest WND structures must match ReactOS layout exactly for `ValidateHwnd()` to work. The desktop heap at 0x01000000 is mapped user-readable so user32.dll can access window data directly.

### Syscall Flow

1. Guest code executes SYSENTER
2. `nt_syscall_handler()` intercepts based on EAX (syscall number)
3. NT syscalls (0-295) handled in `src/nt/ntdll.c`
4. Win32k syscalls (0x1000+) dispatched via `src/nt/win32k_dispatcher.c` to `src/user/user_syscalls.c`

## Development Notes

- No backwards compatibility concerns - this is a new project, replace old behavior completely when changing things
- CPU test data files (`.MOO.gz`) are in `ext/80386/v1_ex_real_mode/`
- The dynarec is enabled by default (`USE_DYNAREC`, `USE_NEW_DYNAREC`)

## Reference code
If reference code is available it's under ref. This includes:
86Box  dosbox-x reactos  wine

## Userland
Reactos and winxp installations are available for testing under the userland directory.

Example: run reactos calculator under with reactos userland:
```
build/wbox --gui -C: userland/reactos/ userland/reactos/reactos/system32/calc.exe
```
