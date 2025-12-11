; WBOX Hello World Test
; Prints "Hello, World!" using NtWriteFile syscall and exits cleanly
;
; Assemble with: nasm -f bin -o hello.exe hello.asm

BITS 32

; Syscall numbers (Windows XP SP3)
NtWriteFile         equ 284
NtTerminateProcess  equ 266

; Standard handle pseudo-values
STD_OUTPUT_HANDLE   equ 0xFFFFFFF5  ; -11

; PE Constants
IMAGE_BASE          equ 0x00400000

; DOS Header at file offset 0
dos_header:
    db 'MZ'                     ; e_magic
    times 58 db 0               ; padding
    dd 0x80                     ; e_lfanew (offset to PE header at 0x80)
    times (0x80 - 64) db 0      ; pad to PE header

; PE Header at file offset 0x80
pe_header:
    db 'PE', 0, 0               ; Signature

; COFF File Header
    dw 0x014C                   ; Machine (i386)
    dw 1                        ; NumberOfSections
    dd 0                        ; TimeDateStamp
    dd 0                        ; PointerToSymbolTable
    dd 0                        ; NumberOfSymbols
    dw optional_header_end - optional_header  ; SizeOfOptionalHeader
    dw 0x0103                   ; Characteristics (EXECUTABLE_IMAGE | NO_RELOC | 32BIT_MACHINE)

; Optional Header (PE32)
optional_header:
    dw 0x010B                   ; Magic (PE32)
    db 0, 0                     ; Linker version
    dd 0x200                    ; SizeOfCode (512 bytes)
    dd 0                        ; SizeOfInitializedData
    dd 0                        ; SizeOfUninitializedData
    dd 0x1000                   ; AddressOfEntryPoint (RVA)
    dd 0x1000                   ; BaseOfCode (RVA)
    dd 0x1000                   ; BaseOfData (RVA)

    ; Windows-specific fields
    dd IMAGE_BASE               ; ImageBase
    dd 0x1000                   ; SectionAlignment
    dd 0x200                    ; FileAlignment
    dw 5, 0                     ; OS Version (5.0 = Win2000)
    dw 0, 0                     ; Image Version
    dw 5, 0                     ; Subsystem Version
    dd 0                        ; Win32VersionValue
    dd 0x3000                   ; SizeOfImage (headers + 2 sections worth)
    dd 0x200                    ; SizeOfHeaders
    dd 0                        ; CheckSum
    dw 3                        ; Subsystem (CONSOLE)
    dw 0                        ; DllCharacteristics
    dd 0x100000                 ; SizeOfStackReserve
    dd 0x1000                   ; SizeOfStackCommit
    dd 0x100000                 ; SizeOfHeapReserve
    dd 0x1000                   ; SizeOfHeapCommit
    dd 0                        ; LoaderFlags
    dd 0                        ; NumberOfRvaAndSizes (no data directories)
optional_header_end:

; Section Table
section_table:
    ; .text section
    db '.text', 0, 0, 0         ; Name
    dd 0x100                    ; VirtualSize (256 bytes)
    dd 0x1000                   ; VirtualAddress (RVA)
    dd 0x200                    ; SizeOfRawData
    dd 0x200                    ; PointerToRawData
    dd 0                        ; PointerToRelocations
    dd 0                        ; PointerToLinenumbers
    dw 0                        ; NumberOfRelocations
    dw 0                        ; NumberOfLinenumbers
    dd 0xE0000020               ; Characteristics (CODE | EXECUTE | READ | WRITE)

; Pad to 0x200 (file alignment)
    times (0x200 - ($-$$)) db 0

; Code Section at file offset 0x200 (VA = 0x00401000)
code_start:

entry_point:
    ; NtWriteFile(stdout, NULL, NULL, NULL, &io_status, msg, msg_len, NULL, NULL)
    ; Arguments pushed right-to-left for __stdcall
    push dword 0                ; Key (NULL)
    push dword 0                ; ByteOffset (NULL)
    push dword msg_len          ; Length
    push dword (IMAGE_BASE + 0x1000 + (msg - code_start))  ; Buffer (absolute VA)
    push dword (IMAGE_BASE + 0x1000 + (io_status - code_start))  ; IoStatusBlock (absolute VA)
    push dword 0                ; ApcContext (NULL)
    push dword 0                ; ApcRoutine (NULL)
    push dword 0                ; Event (NULL)
    push dword STD_OUTPUT_HANDLE ; FileHandle (-11 = stdout)
    push dword (IMAGE_BASE + 0x1000 + (.write_return - code_start))  ; Return address

    mov eax, NtWriteFile        ; Syscall number in EAX
    mov edx, esp                ; Stack pointer in EDX
    sysenter
.write_return:
    add esp, 40                 ; Clean up return addr + 9 args * 4 bytes

    ; NtTerminateProcess(NULL, 0)
    push dword 0                ; ExitStatus
    push dword 0                ; ProcessHandle (NULL = current process)
    push dword (IMAGE_BASE + 0x1000 + (.exit_return - code_start))  ; Return address

    mov eax, NtTerminateProcess ; Syscall number in EAX
    mov edx, esp                ; Stack pointer in EDX
    sysenter
.exit_return:
    ; Should never reach here
    jmp .exit_return

; Data area (still within the code section)
msg:
    db 'Hello, World!', 13, 10
msg_len equ $ - msg

io_status:
    times 8 db 0                ; IO_STATUS_BLOCK (8 bytes)

code_end:

; Pad to 0x200 bytes (file alignment for section)
    times (0x200 - (code_end - code_start)) db 0
