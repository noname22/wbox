; WBOX File Read Test
; Opens a file, reads content, writes to stdout, closes handle, exits
;
; Assemble with: nasm -f bin -o file_test.exe file_test.asm

BITS 32

; Syscall numbers (Windows XP SP3)
NtClose             equ 27
NtCreateFile        equ 39
NtReadFile          equ 191
NtWriteFile         equ 284
NtTerminateProcess  equ 266

; Standard handle pseudo-values
STD_OUTPUT_HANDLE   equ 0xFFFFFFF5  ; -11

; CreateDisposition values
FILE_OPEN           equ 1           ; Open existing file only

; Access mask
FILE_READ_DATA      equ 0x0001
GENERIC_READ        equ 0x80000000
SYNCHRONIZE         equ 0x00100000

; ShareAccess
FILE_SHARE_READ     equ 0x00000001

; CreateOptions
FILE_SYNCHRONOUS_IO_NONALERT equ 0x00000020

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
    dd 0x400                    ; SizeOfCode (1024 bytes)
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
    dd 0x3000                   ; SizeOfImage
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
    dd 0x400                    ; VirtualSize (1024 bytes)
    dd 0x1000                   ; VirtualAddress (RVA)
    dd 0x400                    ; SizeOfRawData
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
    ; =================================================================
    ; NtCreateFile to open \??\C:\test.txt
    ; =================================================================
    ; NtCreateFile(
    ;   OUT PHANDLE FileHandle,              ; [EDX+4]
    ;   IN ACCESS_MASK DesiredAccess,        ; [EDX+8]
    ;   IN POBJECT_ATTRIBUTES ObjAttr,       ; [EDX+12]
    ;   OUT PIO_STATUS_BLOCK IoStatusBlock,  ; [EDX+16]
    ;   IN PLARGE_INTEGER AllocSize,         ; [EDX+20] NULL
    ;   IN ULONG FileAttributes,             ; [EDX+24] 0
    ;   IN ULONG ShareAccess,                ; [EDX+28] FILE_SHARE_READ
    ;   IN ULONG CreateDisposition,          ; [EDX+32] FILE_OPEN
    ;   IN ULONG CreateOptions,              ; [EDX+36] FILE_SYNCHRONOUS_IO_NONALERT
    ;   IN PVOID EaBuffer,                   ; [EDX+40] NULL
    ;   IN ULONG EaLength                    ; [EDX+44] 0
    ; )

    push dword 0                ; EaLength
    push dword 0                ; EaBuffer
    push dword FILE_SYNCHRONOUS_IO_NONALERT ; CreateOptions
    push dword FILE_OPEN        ; CreateDisposition (open existing)
    push dword FILE_SHARE_READ  ; ShareAccess
    push dword 0                ; FileAttributes (normal)
    push dword 0                ; AllocationSize (NULL)
    push dword (IMAGE_BASE + 0x1000 + (io_status - code_start))  ; IoStatusBlock
    push dword (IMAGE_BASE + 0x1000 + (obj_attr - code_start))   ; ObjectAttributes
    push dword (GENERIC_READ | SYNCHRONIZE)  ; DesiredAccess
    push dword (IMAGE_BASE + 0x1000 + (file_handle - code_start))  ; FileHandle output
    push dword (IMAGE_BASE + 0x1000 + (.create_return - code_start))  ; Return address

    mov eax, NtCreateFile
    mov edx, esp
    sysenter
.create_return:
    add esp, 48                 ; Clean up return addr + 11 args

    ; Check if open succeeded (EAX == 0 means STATUS_SUCCESS)
    test eax, eax
    jnz .open_failed

    ; =================================================================
    ; NtReadFile to read content
    ; =================================================================
    ; NtReadFile(
    ;   IN HANDLE FileHandle,                ; [EDX+4]
    ;   IN HANDLE Event,                     ; [EDX+8] NULL
    ;   IN PIO_APC_ROUTINE ApcRoutine,       ; [EDX+12] NULL
    ;   IN PVOID ApcContext,                 ; [EDX+16] NULL
    ;   OUT PIO_STATUS_BLOCK IoStatusBlock,  ; [EDX+20]
    ;   OUT PVOID Buffer,                    ; [EDX+24]
    ;   IN ULONG Length,                     ; [EDX+28]
    ;   IN PLARGE_INTEGER ByteOffset,        ; [EDX+32] NULL
    ;   IN PULONG Key                        ; [EDX+36] NULL
    ; )

    ; Get the file handle value
    mov ecx, [IMAGE_BASE + 0x1000 + (file_handle - code_start)]

    push dword 0                ; Key (NULL)
    push dword 0                ; ByteOffset (NULL)
    push dword 256              ; Length (read up to 256 bytes)
    push dword (IMAGE_BASE + 0x1000 + (read_buffer - code_start))  ; Buffer
    push dword (IMAGE_BASE + 0x1000 + (io_status - code_start))    ; IoStatusBlock
    push dword 0                ; ApcContext (NULL)
    push dword 0                ; ApcRoutine (NULL)
    push dword 0                ; Event (NULL)
    push ecx                    ; FileHandle
    push dword (IMAGE_BASE + 0x1000 + (.read_return - code_start))  ; Return address

    mov eax, NtReadFile
    mov edx, esp
    sysenter
.read_return:
    add esp, 40                 ; Clean up return addr + 9 args

    ; Check if read succeeded
    test eax, eax
    jnz .read_failed

    ; =================================================================
    ; NtWriteFile to write content to stdout
    ; =================================================================
    ; Get bytes read from IO_STATUS_BLOCK.Information (offset +4)
    mov ecx, [IMAGE_BASE + 0x1000 + (io_status - code_start) + 4]

    push dword 0                ; Key (NULL)
    push dword 0                ; ByteOffset (NULL)
    push ecx                    ; Length (bytes actually read)
    push dword (IMAGE_BASE + 0x1000 + (read_buffer - code_start))  ; Buffer
    push dword (IMAGE_BASE + 0x1000 + (io_status - code_start))    ; IoStatusBlock
    push dword 0                ; ApcContext (NULL)
    push dword 0                ; ApcRoutine (NULL)
    push dword 0                ; Event (NULL)
    push dword STD_OUTPUT_HANDLE ; FileHandle (-11 = stdout)
    push dword (IMAGE_BASE + 0x1000 + (.write_return - code_start))  ; Return address

    mov eax, NtWriteFile
    mov edx, esp
    sysenter
.write_return:
    add esp, 40

    ; =================================================================
    ; NtClose to close the file handle
    ; =================================================================
    mov ecx, [IMAGE_BASE + 0x1000 + (file_handle - code_start)]

    push ecx                    ; Handle
    push dword (IMAGE_BASE + 0x1000 + (.close_return - code_start))  ; Return address

    mov eax, NtClose
    mov edx, esp
    sysenter
.close_return:
    add esp, 8

    ; Exit successfully
    jmp .exit_success

.open_failed:
    ; Write "OPEN FAILED" message
    push dword 0
    push dword 0
    push dword open_fail_len
    push dword (IMAGE_BASE + 0x1000 + (open_fail_msg - code_start))
    push dword (IMAGE_BASE + 0x1000 + (io_status - code_start))
    push dword 0
    push dword 0
    push dword 0
    push dword STD_OUTPUT_HANDLE
    push dword (IMAGE_BASE + 0x1000 + (.fail_write_return - code_start))
    mov eax, NtWriteFile
    mov edx, esp
    sysenter
.fail_write_return:
    add esp, 40
    jmp .exit_fail

.read_failed:
    ; Write "READ FAILED" message
    push dword 0
    push dword 0
    push dword read_fail_len
    push dword (IMAGE_BASE + 0x1000 + (read_fail_msg - code_start))
    push dword (IMAGE_BASE + 0x1000 + (io_status - code_start))
    push dword 0
    push dword 0
    push dword 0
    push dword STD_OUTPUT_HANDLE
    push dword (IMAGE_BASE + 0x1000 + (.fail_write_return2 - code_start))
    mov eax, NtWriteFile
    mov edx, esp
    sysenter
.fail_write_return2:
    add esp, 40
    jmp .exit_fail

.exit_success:
    push dword 0                ; ExitStatus = 0
    push dword 0                ; ProcessHandle (NULL = current)
    push dword (IMAGE_BASE + 0x1000 + (.exit_return - code_start))
    mov eax, NtTerminateProcess
    mov edx, esp
    sysenter
.exit_return:
    jmp .exit_return

.exit_fail:
    push dword 1                ; ExitStatus = 1
    push dword 0
    push dword (IMAGE_BASE + 0x1000 + (.exit_return2 - code_start))
    mov eax, NtTerminateProcess
    mov edx, esp
    sysenter
.exit_return2:
    jmp .exit_return2

; =================================================================
; Data Section
; =================================================================

; UNICODE_STRING structure for the file path
; Path: \??\C:\test.txt
unicode_string:
    dw file_path_len * 2        ; Length (bytes, not including null)
    dw (file_path_len + 1) * 2  ; MaximumLength
    dd (IMAGE_BASE + 0x1000 + (file_path - code_start))  ; Buffer pointer

; Wide char file path (each char is 2 bytes)
file_path:
    dw '\', '?', '?', '\', 'C', ':', '\', 't', 'e', 's', 't', '.', 't', 'x', 't', 0
file_path_len equ 15            ; Number of characters (not including null)

; OBJECT_ATTRIBUTES structure (24 bytes)
obj_attr:
    dd 24                       ; Length
    dd 0                        ; RootDirectory (NULL)
    dd (IMAGE_BASE + 0x1000 + (unicode_string - code_start))  ; ObjectName
    dd 0x40                     ; Attributes (OBJ_CASE_INSENSITIVE)
    dd 0                        ; SecurityDescriptor (NULL)
    dd 0                        ; SecurityQualityOfService (NULL)

; Output file handle
file_handle:
    dd 0

; IO_STATUS_BLOCK (8 bytes)
io_status:
    dd 0                        ; Status
    dd 0                        ; Information (bytes transferred)

; Read buffer
read_buffer:
    times 256 db 0

; Error messages
open_fail_msg:
    db 'OPEN FAILED', 13, 10
open_fail_len equ $ - open_fail_msg

read_fail_msg:
    db 'READ FAILED', 13, 10
read_fail_len equ $ - read_fail_msg

code_end:

; Pad to 0x400 bytes (section size)
    times (0x400 - (code_end - code_start)) db 0
