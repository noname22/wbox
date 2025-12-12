# WBox
WBox is a hybrid Windows PC emulator that uses a 86x CPU emulator for windows user space applications. Kernel space, on the other hand, is implemented as native host code.

## Userland Version Compatibility
The kernel implementation aims to support Windows userland up to Windows XP, with the actual userland configurable between

 * ReactOS
 * Wine
 * Windows NT4
 * Windows 2000
 * Windows XP

## Portability
WBox is written in portable C and should be possible to build on most targets, however the dynamically recompling CPU implementation core currently only supports x86-64 and ARM64. In practice, this means that other platforms, such as RISC-V, will use the interpreter and suffer quite a performance penalty.

## Acknowledgements

This project borrows heavily from projects such as reactos and wine. The CPU implementation is based on 86Box's P6 core.
