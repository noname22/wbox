cmake_minimum_required(VERSION 3.16)
project(wbox LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SDL3 is required for GUI display
find_package(SDL3 REQUIRED)

# Detect host architecture for dynarec
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    set(DYNAREC_ARCH "x86_64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64")
    set(DYNAREC_ARCH "arm64")
else()
    message(WARNING "Unknown architecture ${CMAKE_SYSTEM_PROCESSOR}, dynarec disabled")
    set(DYNAREC_ARCH "")
endif()

# CPU emulation sources
set(CPU_SOURCES
    src/cpu/cpu.c
    src/cpu/cpu_table.c
    src/cpu/386.c
    src/cpu/386_common.c
    src/cpu/386_dynarec.c
    src/cpu/386_dynarec_ops.c
    src/cpu/x86.c
    src/cpu/x86seg.c
    src/cpu/x86seg_common.c
    src/cpu/x86seg_2386.c
    src/cpu/x86_ops_mmx.c
    src/cpu/fpu.c
    src/cpu/x87.c
    src/cpu/x87_timings.c
    src/cpu/codegen_timing_486.c
    src/cpu/codegen_timing_686.c
    src/cpu/codegen_timing_common.c
    src/cpu/codegen_timing_k5.c
    src/cpu/codegen_timing_k6.c
    src/cpu/codegen_timing_p6.c
    src/cpu/codegen_timing_pentium.c
    src/cpu/codegen_timing_winchip.c
    src/cpu/codegen_timing_winchip2.c
    src/cpu/mem.c
    src/cpu/stubs.c
)

# Codegen (dynarec) sources
set(CODEGEN_SOURCES
    src/cpu/codegen/codegen.c
    src/cpu/codegen/codegen_block.c
    src/cpu/codegen/codegen_ir.c
    src/cpu/codegen/codegen_allocator.c
    src/cpu/codegen/codegen_reg.c
    src/cpu/codegen/codegen_accumulate.c
    src/cpu/codegen/codegen_ops.c
    src/cpu/codegen/codegen_ops_helpers.c
    src/cpu/codegen/codegen_ops_arith.c
    src/cpu/codegen/codegen_ops_branch.c
    src/cpu/codegen/codegen_ops_jump.c
    src/cpu/codegen/codegen_ops_logic.c
    src/cpu/codegen/codegen_ops_misc.c
    src/cpu/codegen/codegen_ops_mov.c
    src/cpu/codegen/codegen_ops_shift.c
    src/cpu/codegen/codegen_ops_stack.c
    src/cpu/codegen/codegen_ops_fpu_arith.c
    src/cpu/codegen/codegen_ops_fpu_constant.c
    src/cpu/codegen/codegen_ops_fpu_loadstore.c
    src/cpu/codegen/codegen_ops_fpu_misc.c
    src/cpu/codegen/codegen_ops_mmx_arith.c
    src/cpu/codegen/codegen_ops_mmx_cmp.c
    src/cpu/codegen/codegen_ops_mmx_loadstore.c
    src/cpu/codegen/codegen_ops_mmx_logic.c
    src/cpu/codegen/codegen_ops_mmx_pack.c
    src/cpu/codegen/codegen_ops_mmx_shift.c
    src/cpu/codegen/codegen_ops_3dnow.c
)

# Architecture-specific codegen backend
if(DYNAREC_ARCH STREQUAL "x86_64")
    list(APPEND CODEGEN_SOURCES
        src/cpu/codegen/codegen_backend_x86-64.c
        src/cpu/codegen/codegen_backend_x86-64_ops.c
        src/cpu/codegen/codegen_backend_x86-64_ops_sse.c
        src/cpu/codegen/codegen_backend_x86-64_uops.c
    )
elseif(DYNAREC_ARCH STREQUAL "arm64")
    list(APPEND CODEGEN_SOURCES
        src/cpu/codegen/codegen_backend_arm64.c
        src/cpu/codegen/codegen_backend_arm64_ops.c
        src/cpu/codegen/codegen_backend_arm64_imm.c
        src/cpu/codegen/codegen_backend_arm64_uops.c
    )
endif()

# Softfloat library (C++ files)
file(GLOB SOFTFLOAT_SOURCES src/cpu/softfloat3e/*.c src/cpu/softfloat3e/*.cc)

# CPU emulation library (shared between main executable and tests)
add_library(wbox_cpu STATIC
    ${CPU_SOURCES}
    ${CODEGEN_SOURCES}
    ${SOFTFLOAT_SOURCES}
)

target_include_directories(wbox_cpu PUBLIC
    src/cpu
    src/cpu/codegen
    src/cpu/softfloat3e
)

target_compile_definitions(wbox_cpu PUBLIC
    USE_DYNAREC
    USE_NEW_DYNAREC
)

# VM and NT subsystem sources
set(VM_SOURCES
    src/pe/pe_loader.c
    src/vm/vm.c
    src/vm/paging.c
    src/nt/ntdll.c
    src/nt/syscall_table.c
    src/nt/handles.c
    src/nt/heap.c
    src/nt/vfs_jail.c
    src/nt/sys_file.c
    src/nt/sys_process.c
    src/nt/win32k_dispatcher.c
    src/process/process.c
    src/loader/module.c
    src/loader/exports.c
    src/loader/imports.c
    src/loader/stubs.c
    src/loader/loader.c
    src/gdi/display.c
    src/gdi/gdi_handle_table.c
    src/gdi/gdi_dc.c
    src/gdi/gdi_drawing.c
    src/gdi/gdi_text.c
    src/user/user_shared.c
    src/user/user_handle_table.c
    src/user/user_class.c
    src/user/user_window.c
    src/user/user_syscalls.c
    src/user/user_message.c
    src/user/user_callback.c
    src/user/desktop_heap.c
    src/user/guest_wnd.c
    src/user/guest_cls.c
)

# WBOX VM library
add_library(wbox_vm STATIC ${VM_SOURCES})
target_include_directories(wbox_vm PUBLIC src)
target_link_libraries(wbox_vm PUBLIC wbox_cpu SDL3::SDL3)

add_executable(wbox
    src/main.c
)

target_include_directories(wbox PRIVATE src)
target_link_libraries(wbox PRIVATE wbox_vm)

# Warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(wbox PRIVATE -Wall -Wno-unused-variable -Wno-unused-function)
endif()

# Tests
option(BUILD_TESTING "Build the testing tree" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
