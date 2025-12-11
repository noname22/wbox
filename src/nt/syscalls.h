/*
 * WBOX NT Syscall Numbers
 * Generated from doc/nt.md
 */
#ifndef WBOX_SYSCALLS_H
#define WBOX_SYSCALLS_H

#include <stdint.h>

/* NT syscall numbers (0-296) */
#define NtAcceptConnectPort                                0
#define NtAccessCheck                                      1
#define NtAccessCheckAndAuditAlarm                         2
#define NtAccessCheckByType                                3
#define NtAccessCheckByTypeAndAuditAlarm                   4
#define NtAccessCheckByTypeResultList                      5
#define NtAccessCheckByTypeResultListAndAuditAlarm         6
#define NtAccessCheckByTypeResultListAndAuditAlarmByHandle 7
#define NtAddAtom                                          8
#define NtAddBootEntry                                     9
#define NtAddDriverEntry                                   10
#define NtAdjustGroupsToken                                11
#define NtAdjustPrivilegesToken                            12
#define NtAlertResumeThread                                13
#define NtAlertThread                                      14
#define NtAllocateLocallyUniqueId                          15
#define NtAllocateUserPhysicalPages                        16
#define NtAllocateUuids                                    17
#define NtAllocateVirtualMemory                            18
#define NtApphelpCacheControl                              19
#define NtAreMappedFilesTheSame                            20
#define NtAssignProcessToJobObject                         21
#define NtCallbackReturn                                   22
#define NtCancelDeviceWakeupRequest                        23
#define NtCancelIoFile                                     24
#define NtCancelTimer                                      25
#define NtClearEvent                                       26
#define NtClose                                            27
#define NtCloseObjectAuditAlarm                            28
#define NtCompactKeys                                      29
#define NtCompareTokens                                    30
#define NtCompleteConnectPort                              31
#define NtCompressKey                                      32
#define NtConnectPort                                      33
#define NtContinue                                         34
#define NtCreateDebugObject                                35
#define NtCreateDirectoryObject                            36
#define NtCreateEvent                                      37
#define NtCreateEventPair                                  38
#define NtCreateFile                                       39
#define NtCreateIoCompletion                               40
#define NtCreateJobObject                                  41
#define NtCreateJobSet                                     42
#define NtCreateKey                                        43
#define NtCreateMailslotFile                               44
#define NtCreateMutant                                     45
#define NtCreateNamedPipeFile                              46
#define NtCreatePagingFile                                 47
#define NtCreatePort                                       48
#define NtCreateProcess                                    49
#define NtCreateProcessEx                                  50
#define NtCreateProfile                                    51
#define NtCreateSection                                    52
#define NtCreateSemaphore                                  53
#define NtCreateSymbolicLinkObject                         54
#define NtCreateThread                                     55
#define NtCreateTimer                                      56
#define NtCreateToken                                      57
#define NtCreateWaitablePort                               58
#define NtDebugActiveProcess                               59
#define NtDebugContinue                                    60
#define NtDelayExecution                                   61
#define NtDeleteAtom                                       62
#define NtDeleteBootEntry                                  63
#define NtDeleteDriverEntry                                64
#define NtDeleteFile                                       65
#define NtDeleteKey                                        66
#define NtDeleteObjectAuditAlarm                           67
#define NtDeleteValueKey                                   68
#define NtDeviceIoControlFile                              69
#define NtDisplayString                                    70
#define NtDuplicateObject                                  71
#define NtDuplicateToken                                   72
#define NtEnumerateBootEntries                             73
#define NtEnumerateDriverEntries                           74
#define NtEnumerateKey                                     75
#define NtEnumerateSystemEnvironmentValuesEx               76
#define NtEnumerateValueKey                                77
#define NtExtendSection                                    78
#define NtFilterToken                                      79
#define NtFindAtom                                         80
#define NtFlushBuffersFile                                 81
#define NtFlushInstructionCache                            82
#define NtFlushKey                                         83
#define NtFlushVirtualMemory                               84
#define NtFlushWriteBuffer                                 85
#define NtFreeUserPhysicalPages                            86
#define NtFreeVirtualMemory                                87
#define NtFsControlFile                                    88
#define NtGetContextThread                                 89
#define NtGetDevicePowerState                              90
#define NtGetPlugPlayEvent                                 91
#define NtGetWriteWatch                                    92
#define NtImpersonateAnonymousToken                        93
#define NtImpersonateClientOfPort                          94
#define NtImpersonateThread                                95
#define NtInitializeRegistry                               96
#define NtInitiatePowerAction                              97
#define NtIsProcessInJob                                   98
#define NtIsSystemResumeAutomatic                          99
#define NtListenPort                                       100
#define NtLoadDriver                                       101
#define NtLoadKey                                          102
#define NtLoadKey2                                         103
#define NtLoadKeyEx                                        104
#define NtLockFile                                         105
#define NtLockProductActivationKeys                        106
#define NtLockRegistryKey                                  107
#define NtLockVirtualMemory                                108
#define NtMakePermanentObject                              109
#define NtMakeTemporaryObject                              110
#define NtMapUserPhysicalPages                             111
#define NtMapUserPhysicalPagesScatter                      112
#define NtMapViewOfSection                                 113
#define NtModifyBootEntry                                  114
#define NtModifyDriverEntry                                115
#define NtNotifyChangeDirectoryFile                        116
#define NtNotifyChangeKey                                  117
#define NtNotifyChangeMultipleKeys                         118
#define NtOpenDirectoryObject                              119
#define NtOpenEvent                                        120
#define NtOpenEventPair                                    121
#define NtOpenFile                                         122
#define NtOpenIoCompletion                                 123
#define NtOpenJobObject                                    124
#define NtOpenKey                                          125
#define NtOpenMutant                                       126
#define NtOpenObjectAuditAlarm                             127
#define NtOpenProcess                                      128
#define NtOpenProcessToken                                 129
#define NtOpenProcessTokenEx                               130
#define NtOpenSection                                      131
#define NtOpenSemaphore                                    132
#define NtOpenSymbolicLinkObject                           133
#define NtOpenThread                                       134
#define NtOpenThreadToken                                  135
#define NtOpenThreadTokenEx                                136
#define NtOpenTimer                                        137
#define NtPlugPlayControl                                  138
#define NtPowerInformation                                 139
#define NtPrivilegeCheck                                   140
#define NtPrivilegeObjectAuditAlarm                        141
#define NtPrivilegedServiceAuditAlarm                      142
#define NtProtectVirtualMemory                             143
#define NtPulseEvent                                       144
#define NtQueryAttributesFile                              145
#define NtQueryBootEntryOrder                              146
#define NtQueryBootOptions                                 147
#define NtQueryDebugFilterState                            148
#define NtQueryDefaultLocale                               149
#define NtQueryDefaultUILanguage                           150
#define NtQueryDirectoryFile                               151
#define NtQueryDirectoryObject                             152
#define NtQueryDriverEntryOrder                            153
#define NtQueryEaFile                                      154
#define NtQueryEvent                                       155
#define NtQueryFullAttributesFile                          156
#define NtQueryInformationAtom                             157
#define NtQueryInformationFile                             158
#define NtQueryInformationJobObject                        159
#define NtQueryInformationPort                             160
#define NtQueryInformationProcess                          161
#define NtQueryInformationThread                           162
#define NtQueryInformationToken                            163
#define NtQueryInstallUILanguage                           164
#define NtQueryIntervalProfile                             165
#define NtQueryIoCompletion                                166
#define NtQueryKey                                         167
#define NtQueryMultipleValueKey                            168
#define NtQueryMutant                                      169
#define NtQueryObject                                      170
#define NtQueryOpenSubKeys                                 171
#define NtQueryOpenSubKeysEx                               172
#define NtQueryPerformanceCounter                          173
#define NtQueryQuotaInformationFile                        174
#define NtQuerySection                                     175
#define NtQuerySecurityObject                              176
#define NtQuerySemaphore                                   177
#define NtQuerySymbolicLinkObject                          178
#define NtQuerySystemEnvironmentValue                      179
#define NtQuerySystemEnvironmentValueEx                    180
#define NtQuerySystemInformation                           181
#define NtQuerySystemTime                                  182
#define NtQueryTimer                                       183
#define NtQueryTimerResolution                             184
#define NtQueryValueKey                                    185
#define NtQueryVirtualMemory                               186
#define NtQueryVolumeInformationFile                       187
#define NtQueueApcThread                                   188
#define NtRaiseException                                   189
#define NtRaiseHardError                                   190
#define NtReadFile                                         191
#define NtReadFileScatter                                  192
#define NtReadRequestData                                  193
#define NtReadVirtualMemory                                194
#define NtRegisterThreadTerminatePort                      195
#define NtReleaseMutant                                    196
#define NtReleaseSemaphore                                 197
#define NtRemoveIoCompletion                               198
#define NtRemoveProcessDebug                               199
#define NtRenameKey                                        200
#define NtReplaceKey                                       201
#define NtReplyPort                                        202
#define NtReplyWaitReceivePort                             203
#define NtReplyWaitReceivePortEx                           204
#define NtReplyWaitReplyPort                               205
#define NtRequestDeviceWakeup                              206
#define NtRequestPort                                      207
#define NtRequestWaitReplyPort                             208
#define NtRequestWakeupLatency                             209
#define NtResetEvent                                       210
#define NtResetWriteWatch                                  211
#define NtRestoreKey                                       212
#define NtResumeProcess                                    213
#define NtResumeThread                                     214
#define NtSaveKey                                          215
#define NtSaveKeyEx                                        216
#define NtSaveMergedKeys                                   217
#define NtSecureConnectPort                                218
#define NtSetBootEntryOrder                                219
#define NtSetBootOptions                                   220
#define NtSetContextThread                                 221
#define NtSetDebugFilterState                              222
#define NtSetDefaultHardErrorPort                          223
#define NtSetDefaultLocale                                 224
#define NtSetDefaultUILanguage                             225
#define NtSetDriverEntryOrder                              226
#define NtSetEaFile                                        227
#define NtSetEvent                                         228
#define NtSetEventBoostPriority                            229
#define NtSetHighEventPair                                 230
#define NtSetHighWaitLowEventPair                          231
#define NtSetInformationDebugObject                        232
#define NtSetInformationFile                               233
#define NtSetInformationJobObject                          234
#define NtSetInformationKey                                235
#define NtSetInformationObject                             236
#define NtSetInformationProcess                            237
#define NtSetInformationThread                             238
#define NtSetInformationToken                              239
#define NtSetIntervalProfile                               240
#define NtSetIoCompletion                                  241
#define NtSetLdtEntries                                    242
#define NtSetLowEventPair                                  243
#define NtSetLowWaitHighEventPair                          244
#define NtSetQuotaInformationFile                          245
#define NtSetSecurityObject                                246
#define NtSetSystemEnvironmentValue                        247
#define NtSetSystemEnvironmentValueEx                      248
#define NtSetSystemInformation                             249
#define NtSetSystemPowerState                              250
#define NtSetSystemTime                                    251
#define NtSetThreadExecutionState                          252
#define NtSetTimer                                         253
#define NtSetTimerResolution                               254
#define NtSetUuidSeed                                      255
#define NtSetValueKey                                      256
#define NtSetVolumeInformationFile                         257
#define NtShutdownSystem                                   258
#define NtSignalAndWaitForSingleObject                     259
#define NtStartProfile                                     260
#define NtStopProfile                                      261
#define NtSuspendProcess                                   262
#define NtSuspendThread                                    263
#define NtSystemDebugControl                               264
#define NtTerminateJobObject                               265
#define NtTerminateProcess                                 266
#define NtTerminateThread                                  267
#define NtTestAlert                                        268
#define NtTraceEvent                                       269
#define NtTranslateFilePath                                270
#define NtUnloadDriver                                     271
#define NtUnloadKey                                        272
#define NtUnloadKey2                                       273
#define NtUnloadKeyEx                                      274
#define NtUnlockFile                                       275
#define NtUnlockVirtualMemory                              276
#define NtUnmapViewOfSection                               277
#define NtVdmControl                                       278
#define NtWaitForDebugEvent                                279
#define NtWaitForMultipleObjects                           280
#define NtWaitForSingleObject                              281
#define NtWaitHighEventPair                                282
#define NtWaitLowEventPair                                 283
#define NtWriteFile                                        284
#define NtWriteFileGather                                  285
#define NtWriteRequestData                                 286
#define NtWriteVirtualMemory                               287
#define NtYieldExecution                                   288
#define NtCreateKeyedEvent                                 289
#define NtOpenKeyedEvent                                   290
#define NtReleaseKeyedEvent                                291
#define NtWaitForKeyedEvent                                292
#define NtQueryPortInformationProcess                      293
#define NtGetCurrentProcessorNumber                        294
#define NtWaitForMultipleObjects32                         295

#define NT_SYSCALL_COUNT  296

/* Win32k syscalls start at 0x1000 */
#define WIN32K_SYSCALL_BASE  0x1000

/* Special internal syscalls for VM control */
#define WBOX_SYSCALL_DLL_INIT_DONE  0xFFFE  /* DLL entry point completed */

/* Pseudo syscalls for heap function interception */
#define WBOX_SYSCALL_HEAP_ALLOC    0xFFF0  /* RtlAllocateHeap */
#define WBOX_SYSCALL_HEAP_FREE     0xFFF1  /* RtlFreeHeap */
#define WBOX_SYSCALL_HEAP_REALLOC  0xFFF2  /* RtlReAllocateHeap */
#define WBOX_SYSCALL_HEAP_SIZE     0xFFF3  /* RtlSizeHeap */

/* Pseudo syscalls for string conversion function interception */
#define WBOX_SYSCALL_MBSTR_TO_UNICODE  0xFFE0  /* RtlMultiByteToUnicodeN */
#define WBOX_SYSCALL_UNICODE_TO_MBSTR  0xFFE1  /* RtlUnicodeToMultiByteN */
#define WBOX_SYSCALL_MBSTR_SIZE        0xFFE2  /* RtlMultiByteToUnicodeSize */
#define WBOX_SYSCALL_UNICODE_SIZE      0xFFE3  /* RtlUnicodeToMultiByteSize */

/* NTSTATUS values */
#define STATUS_SUCCESS              0x00000000
#define STATUS_END_OF_FILE          0xC0000011
#define STATUS_NOT_IMPLEMENTED      0xC0000002
#define STATUS_INVALID_HANDLE       0xC0000008
#define STATUS_INVALID_PARAMETER    0xC000000D
#define STATUS_NO_MEMORY            0xC0000017
#define STATUS_ACCESS_DENIED        0xC0000022
#define STATUS_BUFFER_TOO_SMALL     0xC0000023
#define STATUS_OBJECT_TYPE_MISMATCH 0xC0000024
#define STATUS_OBJECT_NAME_INVALID   0xC0000033
#define STATUS_OBJECT_NAME_NOT_FOUND 0xC0000034
#define STATUS_OBJECT_NAME_COLLISION 0xC0000035
#define STATUS_OBJECT_PATH_INVALID   0xC0000039
#define STATUS_OBJECT_PATH_NOT_FOUND 0xC000003A
#define STATUS_IO_DEVICE_ERROR      0xC0000185

/* File operation info values (IO_STATUS_BLOCK.Information) */
#define FILE_SUPERSEDED    0
#define FILE_OPENED        1
#define FILE_CREATED       2
#define FILE_OVERWRITTEN   3
#define FILE_EXISTS        4
#define FILE_DOES_NOT_EXIST 5

/* CreateDisposition values for NtCreateFile */
#define FILE_SUPERSEDE     0
#define FILE_OPEN          1
#define FILE_CREATE        2
#define FILE_OPEN_IF       3
#define FILE_OVERWRITE     4
#define FILE_OVERWRITE_IF  5

/* CreateOptions flags for NtCreateFile */
#define FILE_DIRECTORY_FILE            0x00000001
#define FILE_WRITE_THROUGH             0x00000002
#define FILE_SEQUENTIAL_ONLY           0x00000004
#define FILE_NO_INTERMEDIATE_BUFFERING 0x00000008
#define FILE_SYNCHRONOUS_IO_ALERT      0x00000010
#define FILE_SYNCHRONOUS_IO_NONALERT   0x00000020
#define FILE_NON_DIRECTORY_FILE        0x00000040
#define FILE_DELETE_ON_CLOSE           0x00001000

/* Access mask flags */
#define FILE_READ_DATA     0x0001
#define FILE_WRITE_DATA    0x0002
#define FILE_APPEND_DATA   0x0004
#define FILE_READ_EA       0x0008
#define FILE_WRITE_EA      0x0010
#define FILE_EXECUTE       0x0020
#define FILE_READ_ATTRIBUTES  0x0080
#define FILE_WRITE_ATTRIBUTES 0x0100
#define GENERIC_READ       0x80000000
#define GENERIC_WRITE      0x40000000
#define GENERIC_EXECUTE    0x20000000
#define GENERIC_ALL        0x10000000

/* Syscall handler return type */
typedef uint32_t ntstatus_t;

/*
 * Get syscall name by number
 */
const char *syscall_get_name(uint32_t num);

/*
 * NT syscall handler - called when SYSENTER is executed
 * Returns 1 to skip normal SYSENTER processing
 */
int nt_syscall_handler(void);

/*
 * Install/remove the syscall handler
 */
void nt_install_syscall_handler(void);
void nt_remove_syscall_handler(void);

/*
 * Individual syscall implementations
 */
ntstatus_t sys_NtClose(void);
ntstatus_t sys_NtCreateFile(void);
ntstatus_t sys_NtOpenFile(void);
ntstatus_t sys_NtReadFile(void);
ntstatus_t sys_NtWriteFile(void);
ntstatus_t sys_NtTerminateProcess(void);
ntstatus_t sys_NtQueryPerformanceCounter(void);

#endif /* WBOX_SYSCALLS_H */
