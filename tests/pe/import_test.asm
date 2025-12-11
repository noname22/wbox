; import_test.asm - Test PE that imports NtWriteFile from ntdll.dll
; Tests the DLL loader and stub generation system
;
; Assemble with: nasm -f bin -o import_test.exe import_test.asm

BITS 32

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
    dw 2                        ; NumberOfSections
    dd 0                        ; TimeDateStamp
    dd 0                        ; PointerToSymbolTable
    dd 0                        ; NumberOfSymbols
    dw optional_header_end - optional_header  ; SizeOfOptionalHeader
    dw 0x0103                   ; Characteristics (EXECUTABLE_IMAGE | NO_RELOC | 32BIT_MACHINE)

; Optional Header (PE32)
optional_header:
    dw 0x010B                   ; Magic (PE32)
    db 0, 0                     ; Linker version
    dd 0x200                    ; SizeOfCode
    dd 0x200                    ; SizeOfInitializedData
    dd 0                        ; SizeOfUninitializedData
    dd 0x1000                   ; AddressOfEntryPoint (RVA)
    dd 0x1000                   ; BaseOfCode (RVA)
    dd 0x2000                   ; BaseOfData (RVA)

    ; Windows-specific fields
    dd IMAGE_BASE               ; ImageBase
    dd 0x1000                   ; SectionAlignment
    dd 0x200                    ; FileAlignment
    dw 5, 1                     ; OS Version (5.1 = WinXP)
    dw 0, 0                     ; Image Version
    dw 5, 1                     ; Subsystem Version
    dd 0                        ; Win32VersionValue
    dd 0x4000                   ; SizeOfImage (3 pages: headers + .text + .rdata + extra)
    dd 0x200                    ; SizeOfHeaders
    dd 0                        ; CheckSum
    dw 3                        ; Subsystem (CONSOLE)
    dw 0                        ; DllCharacteristics
    dd 0x100000                 ; SizeOfStackReserve
    dd 0x1000                   ; SizeOfStackCommit
    dd 0x100000                 ; SizeOfHeapReserve
    dd 0x1000                   ; SizeOfHeapCommit
    dd 0                        ; LoaderFlags
    dd 16                       ; NumberOfRvaAndSizes

; Data Directories (16 entries)
    dd 0, 0                     ; 0: Export
    dd 0x2000, import_dir_end - import_dir  ; 1: Import (RVA, size)
    dd 0, 0                     ; 2: Resource
    dd 0, 0                     ; 3: Exception
    dd 0, 0                     ; 4: Security
    dd 0, 0                     ; 5: BaseReloc
    dd 0, 0                     ; 6: Debug
    dd 0, 0                     ; 7: Architecture
    dd 0, 0                     ; 8: GlobalPtr
    dd 0, 0                     ; 9: TLS
    dd 0, 0                     ; 10: LoadConfig
    dd 0, 0                     ; 11: BoundImport
    dd 0x2080, iat_end - iat    ; 12: IAT (RVA, size)
    dd 0, 0                     ; 13: DelayImport
    dd 0, 0                     ; 14: CLR
    dd 0, 0                     ; 15: Reserved
optional_header_end:

; Section Table
section_table:
    ; .text section
    db '.text', 0, 0, 0         ; Name
    dd 0x100                    ; VirtualSize
    dd 0x1000                   ; VirtualAddress (RVA)
    dd 0x200                    ; SizeOfRawData
    dd 0x200                    ; PointerToRawData
    dd 0                        ; PointerToRelocations
    dd 0                        ; PointerToLinenumbers
    dw 0                        ; NumberOfRelocations
    dw 0                        ; NumberOfLinenumbers
    dd 0xE0000020               ; Characteristics (CODE | EXECUTE | READ | WRITE)

    ; .rdata section (imports)
    db '.rdata', 0, 0           ; Name
    dd 0x200                    ; VirtualSize
    dd 0x2000                   ; VirtualAddress (RVA)
    dd 0x200                    ; SizeOfRawData
    dd 0x400                    ; PointerToRawData
    dd 0                        ; PointerToRelocations
    dd 0                        ; PointerToLinenumbers
    dw 0                        ; NumberOfRelocations
    dw 0                        ; NumberOfLinenumbers
    dd 0x40000040               ; Characteristics (INITIALIZED_DATA | READ)

; Pad to 0x200 (file alignment)
    times (0x200 - ($-$$)) db 0

; ============================================================================
; Code Section at file offset 0x200 (RVA = 0x1000)
; ============================================================================
code_section:

entry_point:
    ; Test NtWriteFile via import
    ; Set up IO_STATUS_BLOCK on stack
    sub esp, 8                      ; NTSTATUS Status; ULONG_PTR Information
    mov ebx, esp                    ; Save IoStatusBlock address

    ; Push arguments right to left (stdcall)
    push 0                          ; Key (NULL)
    push 0                          ; ByteOffset (NULL)
    push msg_len                    ; Length
    push dword (IMAGE_BASE + 0x1000 + (msg - code_section))  ; Buffer VA
    push ebx                        ; IoStatusBlock
    push 0                          ; ApcContext
    push 0                          ; ApcRoutine
    push 0                          ; Event (NULL)
    push -11                        ; Handle (stdout = -11)

    ; Call NtWriteFile via IAT
    call [IMAGE_BASE + 0x2080]      ; Call via IAT slot for NtWriteFile

    ; Result in EAX (NTSTATUS)
    mov ebx, eax                    ; Save result

    ; Clean up stack (IoStatusBlock)
    add esp, 8

    ; Exit via NtTerminateProcess
    push ebx                        ; ExitStatus (use NtWriteFile result)
    push -1                         ; Handle (current process = -1)
    call [IMAGE_BASE + 0x2084]      ; Call via IAT slot for NtTerminateProcess

    ; Should not reach here
    int3

msg:
    db "Hello from ntdll import!", 13, 10
msg_len equ $ - msg

; Pad to 0x200 bytes
    times (0x200 - ($ - code_section)) db 0

; ============================================================================
; .rdata Section at file offset 0x400 (RVA = 0x2000)
; ============================================================================
rdata_section:

; Import Directory Table at RVA 0x2000
import_dir:
    ; ntdll.dll entry
    dd 0x2040                       ; OriginalFirstThunk (INT RVA)
    dd 0                            ; TimeDateStamp
    dd 0                            ; ForwarderChain
    dd 0x2090                       ; Name RVA
    dd 0x2080                       ; FirstThunk (IAT RVA)
    ; Null terminator
    dd 0, 0, 0, 0, 0
import_dir_end:

; Pad to offset 0x40 (relative to section)
    times (0x40 - ($ - rdata_section)) db 0

; Import Name Table (INT) at RVA 0x2040
int_table:
    dd 0x2050                       ; RVA to hint_NtWriteFile
    dd 0x2060                       ; RVA to hint_NtTerminateProcess
    dd 0                            ; Null terminator

; Pad to offset 0x50 (relative to section)
    times (0x50 - ($ - rdata_section)) db 0

; Hint/Name entries at RVA 0x2050
hint_NtWriteFile:
    dw 0                            ; Hint (ordinal)
    db "NtWriteFile", 0
    db 0                            ; Align to even (total 15 bytes, ends at 0x5F)

; Pad to offset 0x60 (relative to section)
    times (0x60 - ($ - rdata_section)) db 0

; hint_NtTerminateProcess at RVA 0x2060
hint_NtTerminateProcess:
    dw 0                            ; Hint (ordinal)
    db "NtTerminateProcess", 0

; Pad to offset 0x80 (relative to section)
    times (0x80 - ($ - rdata_section)) db 0

; Import Address Table (IAT) at RVA 0x2080
iat:
iat_NtWriteFile:
    dd 0x2050                       ; Will be overwritten by loader
iat_NtTerminateProcess:
    dd 0x2060                       ; Will be overwritten by loader
    dd 0                            ; Null terminator
iat_end:

; Pad to offset 0x90 (relative to section)
    times (0x90 - ($ - rdata_section)) db 0

; DLL name at RVA 0x2090
ntdll_name:
    db "ntdll.dll", 0

; Pad to 0x200 bytes
    times (0x200 - ($ - rdata_section)) db 0
