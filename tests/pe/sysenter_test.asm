; Minimal 32-bit PE file that executes SYSENTER
; Assemble with: nasm -f bin -o sysenter_test.exe sysenter_test.asm

BITS 32

; Constants (must be defined before use)
IMAGE_BASE      equ 0x00400000
SECTION_ALIGN   equ 0x1000
FILE_ALIGN      equ 0x200

; DOS Header
dos_header:
    dw 0x5A4D               ; e_magic: MZ
    dw 0                    ; e_cblp
    dw 0                    ; e_cp
    dw 0                    ; e_crlc
    dw 0                    ; e_cparhdr
    dw 0                    ; e_minalloc
    dw 0                    ; e_maxalloc
    dw 0                    ; e_ss
    dw 0                    ; e_sp
    dw 0                    ; e_csum
    dw 0                    ; e_ip
    dw 0                    ; e_cs
    dw 0                    ; e_lfarlc
    dw 0                    ; e_ovno
    times 4 dw 0            ; e_res[4]
    dw 0                    ; e_oemid
    dw 0                    ; e_oeminfo
    times 10 dw 0           ; e_res2[10]
    dd pe_header            ; e_lfanew: offset to PE header

; PE Header
pe_header:
    dd 0x00004550           ; Signature: "PE\0\0"

; COFF Header
coff_header:
    dw 0x014c               ; Machine: i386
    dw 1                    ; NumberOfSections
    dd 0                    ; TimeDateStamp
    dd 0                    ; PointerToSymbolTable
    dd 0                    ; NumberOfSymbols
    dw optional_header_end - optional_header  ; SizeOfOptionalHeader
    dw 0x0103               ; Characteristics: no relocs, executable, 32-bit

; Optional Header (PE32)
optional_header:
    dw 0x010b               ; Magic: PE32
    db 0                    ; MajorLinkerVersion
    db 0                    ; MinorLinkerVersion
    dd code_end - code_start ; SizeOfCode
    dd 0                    ; SizeOfInitializedData
    dd 0                    ; SizeOfUninitializedData
    dd code_start - dos_header ; AddressOfEntryPoint (RVA)
    dd code_start - dos_header ; BaseOfCode (RVA)
    dd code_start - dos_header ; BaseOfData (RVA)

    ; Windows-specific fields
    dd IMAGE_BASE           ; ImageBase
    dd SECTION_ALIGN        ; SectionAlignment
    dd FILE_ALIGN           ; FileAlignment
    dw 5                    ; MajorOperatingSystemVersion
    dw 1                    ; MinorOperatingSystemVersion
    dw 0                    ; MajorImageVersion
    dw 0                    ; MinorImageVersion
    dw 5                    ; MajorSubsystemVersion
    dw 1                    ; MinorSubsystemVersion
    dd 0                    ; Win32VersionValue
    dd SECTION_ALIGN * 2    ; SizeOfImage (aligned to section alignment)
    dd code_start - dos_header ; SizeOfHeaders
    dd 0                    ; CheckSum
    dw 3                    ; Subsystem: CONSOLE
    dw 0                    ; DllCharacteristics
    dd 0x100000             ; SizeOfStackReserve
    dd 0x1000               ; SizeOfStackCommit
    dd 0x100000             ; SizeOfHeapReserve
    dd 0x1000               ; SizeOfHeapCommit
    dd 0                    ; LoaderFlags
    dd 16                   ; NumberOfRvaAndSizes

    ; Data directories (all zero for static exe)
    times 16 dq 0
optional_header_end:

; Section Headers
section_header:
    db ".text", 0, 0, 0     ; Name (8 bytes)
    dd code_end - code_start ; VirtualSize
    dd code_start - dos_header ; VirtualAddress (RVA)
    dd code_end - code_start ; SizeOfRawData
    dd code_start - dos_header ; PointerToRawData
    dd 0                    ; PointerToRelocations
    dd 0                    ; PointerToLinenumbers
    dw 0                    ; NumberOfRelocations
    dw 0                    ; NumberOfLinenumbers
    dd 0x60000020           ; Characteristics: code, execute, read

; Align to file alignment
align FILE_ALIGN, db 0

; Code section
code_start:
    ; NtAcceptConnectPort syscall (number 0)
    xor eax, eax            ; EAX = 0
    xor ecx, ecx            ; ECX = 0 (user stack ptr for SYSENTER)
    xor edx, edx            ; EDX = 0
    sysenter                ; Execute syscall

    ; Should not reach here - trap with int3
    int3
code_end:

; Pad to file alignment
times (FILE_ALIGN - ((code_end - dos_header) % FILE_ALIGN)) % FILE_ALIGN db 0
file_end:
