# NT Syscall Documentation

This document lists all NT syscalls implemented in ReactOS ntoskrnl.

Total syscalls: 297 (0-296)

## Syscall Table

| # | Name | Arguments | File:Line |
|---|------|-----------|-----------|
| 0 | NtAcceptConnectPort | `OUT PHANDLE PortHandle, IN PVOID PortContext OPTIONAL, IN PPORT_MESSAGE ReplyMessage, IN BOOLEAN AcceptConnection, IN OUT PPORT_VIEW ServerView OPTIONAL, OUT PREMOTE_PORT_VIEW ClientView OPTIONAL` | ntoskrnl/lpc/complete.c:40 |
| 1 | NtAccessCheck | `IN PSECURITY_DESCRIPTOR SecurityDescriptor, IN HANDLE ClientToken, IN ACCESS_MASK DesiredAccess, IN PGENERIC_MAPPING GenericMapping, OUT PPRIVILEGE_SET PrivilegeSet, IN OUT PULONG PrivilegeSetLength, OUT PACCESS_MASK GrantedAccess, OUT PNTSTATUS AccessStatus` | ntoskrnl/se/accesschk.c:2214 |
| 2 | NtAccessCheckAndAuditAlarm | `IN PUNICODE_STRING SubsystemName, IN PVOID HandleId OPTIONAL, IN PUNICODE_STRING ObjectTypeName, IN PUNICODE_STRING ObjectName, IN PSECURITY_DESCRIPTOR SecurityDescriptor, IN ACCESS_MASK DesiredAccess, IN PGENERIC_MAPPING GenericMapping, IN BOOLEAN ObjectCreation, OUT PACCESS_MASK GrantedAccess, OUT PNTSTATUS AccessStatus, OUT PBOOLEAN GenerateOnClose` | ntoskrnl/se/audit.c:2125 |
| 3 | NtAccessCheckByType | `IN PSECURITY_DESCRIPTOR SecurityDescriptor, IN PSID PrincipalSelfSid OPTIONAL, IN HANDLE ClientToken, IN ACCESS_MASK DesiredAccess, IN POBJECT_TYPE_LIST ObjectTypeList OPTIONAL, IN ULONG ObjectTypeListLength, IN PGENERIC_MAPPING GenericMapping, OUT PPRIVILEGE_SET PrivilegeSet, IN OUT PULONG PrivilegeSetLength, OUT PACCESS_MASK GrantedAccess, OUT PNTSTATUS AccessStatus` | ntoskrnl/se/accesschk.c:2254 |
| 4 | NtAccessCheckByTypeAndAuditAlarm | `IN PUNICODE_STRING SubsystemName, IN PVOID HandleId OPTIONAL, IN PUNICODE_STRING ObjectTypeName, IN PUNICODE_STRING ObjectName, IN PSECURITY_DESCRIPTOR SecurityDescriptor, IN PSID PrincipalSelfSid OPTIONAL, IN ACCESS_MASK DesiredAccess, IN AUDIT_EVENT_TYPE AuditType, IN ULONG Flags, IN POBJECT_TYPE_LIST ObjectTypeList OPTIONAL, IN ULONG ObjectTypeLength, IN PGENERIC_MAPPING GenericMapping, IN BOOLEAN ObjectCreation, OUT PACCESS_MASK GrantedAccessList, OUT PNTSTATUS AccessStatusList, OUT PBOOLEAN AccessCheckResult` | ntoskrnl/se/audit.c:2222 |
| 5 | NtAccessCheckByTypeResultList | `IN PSECURITY_DESCRIPTOR SecurityDescriptor, IN PSID PrincipalSelfSid OPTIONAL, IN HANDLE ClientToken, IN ACCESS_MASK DesiredAccess, IN POBJECT_TYPE_LIST ObjectTypeList, IN ULONG ObjectTypeListLength, IN PGENERIC_MAPPING GenericMapping, OUT PPRIVILEGE_SET PrivilegeSet, IN OUT PULONG PrivilegeSetLength, OUT PACCESS_MASK GrantedAccess, OUT PNTSTATUS AccessStatus` | ntoskrnl/se/accesschk.c:2297 |
| 6 | NtAccessCheckByTypeResultListAndAuditAlarm | `IN PUNICODE_STRING SubsystemName, IN PVOID HandleId OPTIONAL, IN PUNICODE_STRING ObjectTypeName, IN PUNICODE_STRING ObjectName, IN PSECURITY_DESCRIPTOR SecurityDescriptor, IN PSID PrincipalSelfSid OPTIONAL, IN ACCESS_MASK DesiredAccess, IN AUDIT_EVENT_TYPE AuditType, IN ULONG Flags, IN POBJECT_TYPE_LIST ObjectTypeList OPTIONAL, IN ULONG ObjectTypeListLength, IN PGENERIC_MAPPING GenericMapping, IN BOOLEAN ObjectCreation, OUT PACCESS_MASK GrantedAccessList, OUT PNTSTATUS AccessStatusList, OUT PBOOLEAN AccessCheckResult` | ntoskrnl/se/audit.c:2324 |
| 7 | NtAccessCheckByTypeResultListAndAuditAlarmByHandle | `IN PUNICODE_STRING SubsystemName, IN PVOID HandleId OPTIONAL, IN HANDLE ClientToken, IN PUNICODE_STRING ObjectTypeName, IN PUNICODE_STRING ObjectName, IN PSECURITY_DESCRIPTOR SecurityDescriptor, IN PSID PrincipalSelfSid OPTIONAL, IN ACCESS_MASK DesiredAccess, IN AUDIT_EVENT_TYPE AuditType, IN ULONG Flags, IN POBJECT_TYPE_LIST ObjectTypeList OPTIONAL, IN ULONG ObjectTypeListLength, IN PGENERIC_MAPPING GenericMapping, IN BOOLEAN ObjectCreation, OUT PACCESS_MASK GrantedAccessList, OUT PNTSTATUS AccessStatusList, OUT PBOOLEAN AccessCheckResult` | ntoskrnl/se/audit.c:2430 |
| 8 | NtAddAtom | `IN PWSTR AtomName, IN ULONG AtomNameLength, OUT PRTL_ATOM Atom` | ntoskrnl/ex/atom.c:86 |
| 9 | NtAddBootEntry | `IN PBOOT_ENTRY Entry, IN ULONG Id` | ntoskrnl/ex/efi.c:19 |
| 10 | NtAddDriverEntry | `IN PEFI_DRIVER_ENTRY Entry, IN ULONG Id` | ntoskrnl/ex/efi.c:28 |
| 11 | NtAdjustGroupsToken | `IN HANDLE TokenHandle, IN BOOLEAN ResetToDefault, IN PTOKEN_GROUPS NewState, IN ULONG BufferLength, OUT PTOKEN_GROUPS PreviousState OPTIONAL, OUT PULONG ReturnLength` | ntoskrnl/se/tokenadj.c:695 |
| 12 | NtAdjustPrivilegesToken | `IN HANDLE TokenHandle, IN BOOLEAN DisableAllPrivileges, IN PTOKEN_PRIVILEGES NewState OPTIONAL, IN ULONG BufferLength, OUT PTOKEN_PRIVILEGES PreviousState OPTIONAL, OUT PULONG ReturnLength` | ntoskrnl/se/tokenadj.c:451 |
| 13 | NtAlertResumeThread | `IN HANDLE ThreadHandle, OUT PULONG SuspendCount` | ntoskrnl/ps/state.c:226 |
| 14 | NtAlertThread | `IN HANDLE ThreadHandle` | ntoskrnl/ps/state.c:193 |
| 15 | NtAllocateLocallyUniqueId | `OUT LUID *LocallyUniqueId` | ntoskrnl/ex/uuid.c:348 |
| 16 | NtAllocateUserPhysicalPages | `IN HANDLE ProcessHandle, IN OUT PULONG_PTR NumberOfPages, IN OUT PULONG_PTR UserPfnArray` | ntoskrnl/mm/ARM3/procsup.c:1436 |
| 17 | NtAllocateUuids | `OUT PULARGE_INTEGER Time, OUT PULONG Range, OUT PULONG Sequence, OUT PUCHAR Seed` | ntoskrnl/ex/uuid.c:460 |
| 18 | NtAllocateVirtualMemory | `IN HANDLE ProcessHandle, IN OUT PVOID* UBaseAddress, IN ULONG_PTR ZeroBits, IN OUT PSIZE_T URegionSize, IN ULONG AllocationType, IN ULONG Protect` | ntoskrnl/mm/ARM3/virtual.c:4457 |
| 19 | NtApphelpCacheControl | `IN APPHELPCACHESERVICECLASS Service, IN PAPPHELP_CACHE_SERVICE_LOOKUP ServiceData OPTIONAL` | ntoskrnl/ps/apphelp.c:728 |
| 20 | NtAreMappedFilesTheSame | `IN PVOID File1MappedAsAnImage, IN PVOID File2MappedAsFile` | ntoskrnl/mm/ARM3/section.c:2997 |
| 21 | NtAssignProcessToJobObject | `HANDLE JobHandle, HANDLE ProcessHandle` | ntoskrnl/ps/job.c:157 |
| 22 | NtCallbackReturn | `IN PVOID Result, IN ULONG ResultLength, IN NTSTATUS CallbackStatus` | ntoskrnl/ke/i386/usercall.c:386 |
| 23 | NtCancelDeviceWakeupRequest | `IN HANDLE DeviceHandle` | ntoskrnl/io/iomgr/iofunc.c:4631 |
| 24 | NtCancelIoFile | `IN HANDLE FileHandle, OUT PIO_STATUS_BLOCK IoStatusBlock` | ntoskrnl/io/iomgr/file.c:4020 |
| 25 | NtCancelTimer | `IN HANDLE TimerHandle, OUT PBOOLEAN CurrentState OPTIONAL` | ntoskrnl/ex/timer.c:252 |
| 26 | NtClearEvent | `IN HANDLE EventHandle` | ntoskrnl/ex/event.c:65 |
| 27 | NtClose | `IN HANDLE Handle` | ntoskrnl/ob/obhandle.c:3402 |
| 28 | NtCloseObjectAuditAlarm | `IN PUNICODE_STRING SubsystemName, IN PVOID HandleId, IN BOOLEAN GenerateOnClose` | ntoskrnl/se/audit.c:1358 |
| 29 | NtCompactKeys | `IN ULONG Count, IN PHANDLE KeyArray` | ntoskrnl/config/ntapi.c:1384 |
| 30 | NtCompareTokens | `IN HANDLE FirstTokenHandle, IN HANDLE SecondTokenHandle, OUT PBOOLEAN Equal` | ntoskrnl/se/token.c:2504 |
| 31 | NtCompleteConnectPort | `IN HANDLE PortHandle` | ntoskrnl/lpc/complete.c:423 |
| 32 | NtCompressKey | `IN HANDLE Key` | ntoskrnl/config/ntapi.c:1393 |
| 33 | NtConnectPort | `OUT PHANDLE PortHandle, IN PUNICODE_STRING PortName, IN PSECURITY_QUALITY_OF_SERVICE SecurityQos, IN OUT PPORT_VIEW ClientView OPTIONAL, IN OUT PREMOTE_PORT_VIEW ServerView OPTIONAL, OUT PULONG MaxMessageLength OPTIONAL, IN OUT PVOID ConnectionInformation OPTIONAL, IN OUT PULONG ConnectionInformationLength OPTIONAL` | ntoskrnl/lpc/connect.c:777 |
| 34 | NtContinue | `IN PCONTEXT Context, IN BOOLEAN TestAlert` | ntoskrnl/ke/except.c:216 |
| 35 | NtCreateDebugObject | `OUT PHANDLE DebugHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes, IN ULONG Flags` | ntoskrnl/dbgk/dbgkobj.c:1571 |
| 36 | NtCreateDirectoryObject | `OUT PHANDLE DirectoryHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes` | ntoskrnl/ob/obdir.c:765 |
| 37 | NtCreateEvent | `OUT PHANDLE EventHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL, IN EVENT_TYPE EventType, IN BOOLEAN InitialState` | ntoskrnl/ex/event.c:96 |
| 38 | NtCreateEventPair | `OUT PHANDLE EventPairHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes` | ntoskrnl/ex/evtpair.c:57 |
| 39 | NtCreateFile | `PHANDLE FileHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock, PLARGE_INTEGER AllocateSize, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PVOID EaBuffer, ULONG EaLength` | ntoskrnl/io/iomgr/file.c:3759 |
| 40 | NtCreateIoCompletion | `OUT PHANDLE IoCompletionHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes, IN ULONG NumberOfConcurrentThreads` | ntoskrnl/io/iomgr/iocomp.c:253 |
| 41 | NtCreateJobObject | `PHANDLE JobHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes` | ntoskrnl/ps/job.c:256 |
| 42 | NtCreateJobSet | `IN ULONG NumJob, IN PJOB_SET_ARRAY UserJobSet, IN ULONG Flags` | ntoskrnl/ps/job.c:243 |
| 43 | NtCreateKey | `OUT PHANDLE KeyHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes, IN ULONG TitleIndex, IN PUNICODE_STRING Class OPTIONAL, IN ULONG CreateOptions, OUT PULONG Disposition OPTIONAL` | ntoskrnl/config/ntapi.c:240 |
| 44 | NtCreateMailslotFile | `OUT PHANDLE FileHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes, OUT PIO_STATUS_BLOCK IoStatusBlock, IN ULONG CreateOptions, IN ULONG MailslotQuota, IN ULONG MaxMessageSize, IN PLARGE_INTEGER TimeOut` | ntoskrnl/io/iomgr/file.c:3790 |
| 45 | NtCreateMutant | `OUT PHANDLE MutantHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL, IN BOOLEAN InitialOwner` | ntoskrnl/ex/mutant.c:79 |
| 46 | NtCreateNamedPipeFile | `OUT PHANDLE FileHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes, OUT PIO_STATUS_BLOCK IoStatusBlock, IN ULONG ShareAccess, IN ULONG CreateDisposition, IN ULONG CreateOptions, IN ULONG NamedPipeType, IN ULONG ReadMode, IN ULONG CompletionMode, IN ULONG MaximumInstances, IN ULONG InboundQuota, IN ULONG OutboundQuota, IN PLARGE_INTEGER DefaultTimeout` | ntoskrnl/io/iomgr/file.c:3859 |
| 47 | NtCreatePagingFile | `IN PUNICODE_STRING FileName, IN PLARGE_INTEGER MinimumSize, IN PLARGE_INTEGER MaximumSize, IN ULONG Reserved` | ntoskrnl/mm/pagefile.c:366 |
| 48 | NtCreatePort | `OUT PHANDLE PortHandle, IN POBJECT_ATTRIBUTES ObjectAttributes, IN ULONG MaxConnectInfoLength, IN ULONG MaxDataLength, IN ULONG MaxPoolUsage` | ntoskrnl/lpc/create.c:222 |
| 49 | NtCreateProcess | `OUT PHANDLE ProcessHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL, IN HANDLE ParentProcess, IN BOOLEAN InheritObjectTable, IN HANDLE SectionHandle OPTIONAL, IN HANDLE DebugPort OPTIONAL, IN HANDLE ExceptionPort OPTIONAL` | ntoskrnl/ps/process.c:1405 |
| 50 | NtCreateProcessEx | `OUT PHANDLE ProcessHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL, IN HANDLE ParentProcess, IN ULONG Flags, IN HANDLE SectionHandle OPTIONAL, IN HANDLE DebugPort OPTIONAL, IN HANDLE ExceptionPort OPTIONAL, IN BOOLEAN InJob` | ntoskrnl/ps/process.c:1344 |
| 51 | NtCreateProfile | `OUT PHANDLE ProfileHandle, IN HANDLE Process OPTIONAL, IN PVOID RangeBase, IN SIZE_T RangeSize, IN ULONG BucketSize, IN PVOID Buffer, IN ULONG BufferSize, IN KPROFILE_SOURCE ProfileSource, IN KAFFINITY Affinity` | ntoskrnl/ex/profile.c:89 |
| 52 | NtCreateSection | `OUT PHANDLE SectionHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL, IN PLARGE_INTEGER MaximumSize OPTIONAL, IN ULONG SectionPageProtection OPTIONAL, IN ULONG AllocationAttributes, IN HANDLE FileHandle OPTIONAL` | ntoskrnl/mm/ARM3/section.c:3076 |
| 53 | NtCreateSemaphore | `OUT PHANDLE SemaphoreHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL, IN LONG InitialCount, IN LONG MaximumCount` | ntoskrnl/ex/sem.c:69 |
| 54 | NtCreateSymbolicLinkObject | `OUT PHANDLE LinkHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes, IN PUNICODE_STRING LinkTarget` | ntoskrnl/ob/oblink.c:676 |
| 55 | NtCreateThread | `OUT PHANDLE ThreadHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL, IN HANDLE ProcessHandle, OUT PCLIENT_ID ClientId, IN PCONTEXT ThreadContext, IN PINITIAL_TEB InitialTeb, IN BOOLEAN CreateSuspended` | ntoskrnl/ps/thread.c:941 |
| 56 | NtCreateTimer | `OUT PHANDLE TimerHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL, IN TIMER_TYPE TimerType` | ntoskrnl/ex/timer.c:372 |
| 57 | NtCreateToken | `OUT PHANDLE TokenHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL, IN TOKEN_TYPE TokenType, IN PLUID AuthenticationId, IN PLARGE_INTEGER ExpirationTime, IN PTOKEN_USER TokenUser, IN PTOKEN_GROUPS TokenGroups, IN PTOKEN_PRIVILEGES TokenPrivileges, IN PTOKEN_OWNER TokenOwner OPTIONAL, IN PTOKEN_PRIMARY_GROUP TokenPrimaryGroup, IN PTOKEN_DEFAULT_DACL TokenDefaultDacl OPTIONAL, IN PTOKEN_SOURCE TokenSource` | ntoskrnl/se/tokenlif.c:1560 |
| 58 | NtCreateWaitablePort | `OUT PHANDLE PortHandle, IN POBJECT_ATTRIBUTES ObjectAttributes, IN ULONG MaxConnectInfoLength, IN ULONG MaxDataLength, IN ULONG MaxPoolUsage` | ntoskrnl/lpc/create.c:244 |
| 59 | NtDebugActiveProcess | `IN HANDLE ProcessHandle, IN HANDLE DebugHandle` | ntoskrnl/dbgk/dbgkobj.c:1797 |
| 60 | NtDebugContinue | `IN HANDLE DebugHandle, IN PCLIENT_ID AppClientId, IN NTSTATUS ContinueStatus` | ntoskrnl/dbgk/dbgkobj.c:1665 |
| 61 | NtDelayExecution | `IN BOOLEAN Alertable, IN PLARGE_INTEGER DelayInterval` | ntoskrnl/ke/wait.c:876 |
| 62 | NtDeleteAtom | `IN RTL_ATOM Atom` | ntoskrnl/ex/atom.c:206 |
| 63 | NtDeleteBootEntry | `IN ULONG Id` | ntoskrnl/ex/efi.c:37 |
| 64 | NtDeleteDriverEntry | `IN ULONG Id` | ntoskrnl/ex/efi.c:45 |
| 65 | NtDeleteFile | `IN POBJECT_ATTRIBUTES ObjectAttributes` | ntoskrnl/io/iomgr/file.c:4152 |
| 66 | NtDeleteKey | `IN HANDLE KeyHandle` | ntoskrnl/config/ntapi.c:408 |
| 67 | NtDeleteObjectAuditAlarm | `IN PUNICODE_STRING SubsystemName, IN PVOID HandleId, IN BOOLEAN GenerateOnClose` | ntoskrnl/se/audit.c:1475 |
| 68 | NtDeleteValueKey | `IN HANDLE KeyHandle, IN PUNICODE_STRING ValueName` | ntoskrnl/config/ntapi.c:1014 |
| 69 | NtDeviceIoControlFile | `IN HANDLE DeviceHandle, IN HANDLE Event OPTIONAL, IN PIO_APC_ROUTINE UserApcRoutine OPTIONAL, IN PVOID UserApcContext OPTIONAL, OUT PIO_STATUS_BLOCK IoStatusBlock, IN ULONG IoControlCode, IN PVOID InputBuffer, IN ULONG InputBufferLength OPTIONAL, OUT PVOID OutputBuffer, IN ULONG OutputBufferLength OPTIONAL` | ntoskrnl/io/iomgr/iofunc.c:1430 |
| 70 | NtDisplayString | `IN PUNICODE_STRING DisplayString` | ntoskrnl/inbv/inbv.c:713 |
| 71 | NtDuplicateObject | `IN HANDLE SourceProcessHandle, IN HANDLE SourceHandle, IN HANDLE TargetProcessHandle OPTIONAL, OUT PHANDLE TargetHandle OPTIONAL, IN ACCESS_MASK DesiredAccess, IN ULONG HandleAttributes, IN ULONG Options` | ntoskrnl/ob/obhandle.c:3410 |
| 72 | NtDuplicateToken | `IN HANDLE ExistingTokenHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL, IN BOOLEAN EffectiveOnly, IN TOKEN_TYPE TokenType, OUT PHANDLE NewTokenHandle` | ntoskrnl/se/tokenlif.c:1871 |
| 73 | NtEnumerateBootEntries | `IN PVOID Buffer, IN PULONG BufferLength` | ntoskrnl/ex/efi.c:53 |
| 74 | NtEnumerateDriverEntries | `IN PVOID Buffer, IN PULONG BufferLength` | ntoskrnl/ex/efi.c:62 |
| 75 | NtEnumerateKey | `IN HANDLE KeyHandle, IN ULONG Index, IN KEY_INFORMATION_CLASS KeyInformationClass, OUT PVOID KeyInformation, IN ULONG Length, OUT PULONG ResultLength` | ntoskrnl/config/ntapi.c:457 |
| 76 | NtEnumerateSystemEnvironmentValuesEx | `IN ULONG InformationClass, IN PVOID Buffer, IN ULONG BufferLength` | ntoskrnl/ex/sysinfo.c:557 |
| 77 | NtEnumerateValueKey | `IN HANDLE KeyHandle, IN ULONG Index, IN KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass, OUT PVOID KeyValueInformation, IN ULONG Length, OUT PULONG ResultLength` | ntoskrnl/config/ntapi.c:542 |
| 78 | NtExtendSection | `IN HANDLE SectionHandle, IN OUT PLARGE_INTEGER NewMaximumSize` | ntoskrnl/mm/ARM3/section.c:3516 |
| 79 | NtFilterToken | `IN HANDLE ExistingTokenHandle, IN ULONG Flags, IN PTOKEN_GROUPS SidsToDisable OPTIONAL, IN PTOKEN_PRIVILEGES PrivilegesToDelete OPTIONAL, IN PTOKEN_GROUPS RestrictedSids OPTIONAL, OUT PHANDLE NewTokenHandle` | ntoskrnl/se/tokenlif.c:2077 |
| 80 | NtFindAtom | `IN PWSTR AtomName, IN ULONG AtomNameLength, OUT PRTL_ATOM Atom` | ntoskrnl/ex/atom.c:243 |
| 81 | NtFlushBuffersFile | `IN HANDLE FileHandle, OUT PIO_STATUS_BLOCK IoStatusBlock` | ntoskrnl/io/iomgr/iofunc.c:1487 |
| 82 | NtFlushInstructionCache | `IN HANDLE ProcessHandle, IN PVOID BaseAddress OPTIONAL, IN SIZE_T FlushSize` | ntoskrnl/mm/ARM3/virtual.c:3009 |
| 83 | NtFlushKey | `IN HANDLE KeyHandle` | ntoskrnl/config/ntapi.c:1085 |
| 84 | NtFlushVirtualMemory | `IN HANDLE ProcessHandle, IN OUT PVOID *BaseAddress, IN OUT PSIZE_T NumberOfBytesToFlush, OUT PIO_STATUS_BLOCK IoStatusBlock` | ntoskrnl/mm/ARM3/virtual.c:4000 |
| 85 | NtFlushWriteBuffer | `VOID` | ntoskrnl/io/iomgr/file.c:3939 |
| 86 | NtFreeUserPhysicalPages | `IN HANDLE ProcessHandle, IN OUT PULONG_PTR NumberOfPages, IN OUT PULONG_PTR UserPfnArray` | ntoskrnl/mm/ARM3/procsup.c:1466 |
| 87 | NtFreeVirtualMemory | `IN HANDLE ProcessHandle, IN PVOID* UBaseAddress, IN PSIZE_T URegionSize, IN ULONG FreeType` | ntoskrnl/mm/ARM3/virtual.c:5192 |
| 88 | NtFsControlFile | `IN HANDLE DeviceHandle, IN HANDLE Event OPTIONAL, IN PIO_APC_ROUTINE UserApcRoutine OPTIONAL, IN PVOID UserApcContext OPTIONAL, OUT PIO_STATUS_BLOCK IoStatusBlock, IN ULONG IoControlCode, IN PVOID InputBuffer, IN ULONG InputBufferLength OPTIONAL, OUT PVOID OutputBuffer, IN ULONG OutputBufferLength OPTIONAL` | ntoskrnl/io/iomgr/iofunc.c:1460 |
| 89 | NtGetContextThread | `IN HANDLE ThreadHandle, IN OUT PCONTEXT ThreadContext` | ntoskrnl/ps/debug.c:350 |
| 90 | NtGetDevicePowerState | `IN HANDLE Device, IN PDEVICE_POWER_STATE PowerState` | ntoskrnl/po/power.c:1045 |
| 91 | NtGetPlugPlayEvent | `IN ULONG Reserved1, IN ULONG Reserved2, OUT PPLUGPLAY_EVENT_BLOCK Buffer, IN ULONG BufferSize` | ntoskrnl/io/pnpmgr/plugplay.c:1443 |
| 92 | NtGetWriteWatch | `IN HANDLE ProcessHandle, IN ULONG Flags, IN PVOID BaseAddress, IN SIZE_T RegionSize, IN PVOID *UserAddressArray, OUT PULONG_PTR EntriesInUserAddressArray, OUT PULONG Granularity` | ntoskrnl/mm/ARM3/virtual.c:4122 |
| 93 | NtImpersonateAnonymousToken | `IN HANDLE ThreadHandle` | ntoskrnl/se/token.c:2613 |
| 94 | NtImpersonateClientOfPort | `IN HANDLE PortHandle, IN PPORT_MESSAGE ClientMessage` | ntoskrnl/lpc/port.c:126 |
| 95 | NtImpersonateThread | `IN HANDLE ThreadHandle, IN HANDLE ThreadToImpersonateHandle, IN PSECURITY_QUALITY_OF_SERVICE SecurityQualityOfService` | ntoskrnl/ps/security.c:1036 |
| 96 | NtInitializeRegistry | `IN USHORT Flag` | ntoskrnl/config/ntapi.c:1318 |
| 97 | NtInitiatePowerAction | `IN POWER_ACTION SystemAction, IN SYSTEM_POWER_STATE MinSystemState, IN ULONG Flags, IN BOOLEAN Asynchronous` | ntoskrnl/po/power.c:778 |
| 98 | NtIsProcessInJob | `IN HANDLE ProcessHandle, IN HANDLE JobHandle OPTIONAL` | ntoskrnl/ps/job.c:361 |
| 99 | NtIsSystemResumeAutomatic | `VOID` | ntoskrnl/po/power.c:1054 |
| 100 | NtListenPort | `IN HANDLE PortHandle, OUT PPORT_MESSAGE ConnectMessage` | ntoskrnl/lpc/listen.c:22 |
| 101 | NtLoadDriver | `IN PUNICODE_STRING DriverServiceName` | ntoskrnl/io/iomgr/driver.c:2165 |
| 102 | NtLoadKey | `IN POBJECT_ATTRIBUTES KeyObjectAttributes, IN POBJECT_ATTRIBUTES FileObjectAttributes` | ntoskrnl/config/ntapi.c:1129 |
| 103 | NtLoadKey2 | `IN POBJECT_ATTRIBUTES KeyObjectAttributes, IN POBJECT_ATTRIBUTES FileObjectAttributes, IN ULONG Flags` | ntoskrnl/config/ntapi.c:1138 |
| 104 | NtLoadKeyEx | `IN POBJECT_ATTRIBUTES TargetKey, IN POBJECT_ATTRIBUTES SourceFile, IN ULONG Flags, IN HANDLE TrustClassKey` | ntoskrnl/config/ntapi.c:1148 |
| 105 | NtLockFile | `IN HANDLE FileHandle, IN HANDLE EventHandle OPTIONAL, IN PIO_APC_ROUTINE ApcRoutine OPTIONAL, IN PVOID ApcContext OPTIONAL, OUT PIO_STATUS_BLOCK IoStatusBlock, IN PLARGE_INTEGER ByteOffset, IN PLARGE_INTEGER Length, IN ULONG Key, IN BOOLEAN FailImmediately, IN BOOLEAN ExclusiveLock` | ntoskrnl/io/iomgr/iofunc.c:1764 |
| 106 | NtLockProductActivationKeys | `IN PULONG pPrivateVer, IN PULONG pSafeMode` | ntoskrnl/config/ntapi.c:1404 |
| 107 | NtLockRegistryKey | `IN HANDLE KeyHandle` | ntoskrnl/config/ntapi.c:1449 |
| 108 | NtLockVirtualMemory | `IN HANDLE ProcessHandle, IN OUT PVOID *BaseAddress, IN OUT PSIZE_T NumberOfBytesToLock, IN ULONG MapType` | ntoskrnl/mm/ARM3/virtual.c:3494 |
| 109 | NtMakePermanentObject | `IN HANDLE ObjectHandle` | ntoskrnl/ob/oblife.c:1510 |
| 110 | NtMakeTemporaryObject | `IN HANDLE ObjectHandle` | ntoskrnl/ob/oblife.c:1473 |
| 111 | NtMapUserPhysicalPages | `IN PVOID VirtualAddresses, IN ULONG_PTR NumberOfPages, IN OUT PULONG_PTR UserPfnArray` | ntoskrnl/mm/ARM3/procsup.c:1446 |
| 112 | NtMapUserPhysicalPagesScatter | `IN PVOID *VirtualAddresses, IN ULONG_PTR NumberOfPages, IN OUT PULONG_PTR UserPfnArray` | ntoskrnl/mm/ARM3/procsup.c:1456 |
| 113 | NtMapViewOfSection | `IN HANDLE SectionHandle, IN HANDLE ProcessHandle, IN OUT PVOID* BaseAddress, IN ULONG_PTR ZeroBits, IN SIZE_T CommitSize, IN OUT PLARGE_INTEGER SectionOffset OPTIONAL, IN OUT PSIZE_T ViewSize, IN SECTION_INHERIT InheritDisposition, IN ULONG AllocationType, IN ULONG Protect` | ntoskrnl/mm/ARM3/section.c:3257 |
| 114 | NtModifyBootEntry | `IN PBOOT_ENTRY BootEntry` | ntoskrnl/ex/efi.c:71 |
| 115 | NtModifyDriverEntry | `IN PEFI_DRIVER_ENTRY DriverEntry` | ntoskrnl/ex/efi.c:79 |
| 116 | NtNotifyChangeDirectoryFile | `IN HANDLE FileHandle, IN HANDLE EventHandle OPTIONAL, IN PIO_APC_ROUTINE ApcRoutine OPTIONAL, IN PVOID ApcContext OPTIONAL, OUT PIO_STATUS_BLOCK IoStatusBlock, OUT PVOID Buffer, IN ULONG BufferSize, IN ULONG CompletionFilter, IN BOOLEAN WatchTree` | ntoskrnl/io/iomgr/iofunc.c:1622 |
| 117 | NtNotifyChangeKey | `IN HANDLE KeyHandle, IN HANDLE Event, IN PIO_APC_ROUTINE ApcRoutine OPTIONAL, IN PVOID ApcContext OPTIONAL, OUT PIO_STATUS_BLOCK IoStatusBlock, IN ULONG CompletionFilter, IN BOOLEAN WatchTree, OUT PVOID Buffer, IN ULONG Length, IN BOOLEAN Asynchronous` | ntoskrnl/config/ntapi.c:1290 |
| 118 | NtNotifyChangeMultipleKeys | `IN HANDLE MasterKeyHandle, IN ULONG Count, IN POBJECT_ATTRIBUTES SlaveObjects, IN HANDLE Event, IN PIO_APC_ROUTINE ApcRoutine OPTIONAL, IN PVOID ApcContext OPTIONAL, OUT PIO_STATUS_BLOCK IoStatusBlock, IN ULONG CompletionFilter, IN BOOLEAN WatchTree, IN OUT PVOID Buffer, IN ULONG Length, IN BOOLEAN Asynchronous` | ntoskrnl/config/ntapi.c:1457 |
| 119 | NtOpenDirectoryObject | `OUT PHANDLE DirectoryHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes` | ntoskrnl/ob/obdir.c:393 |
| 120 | NtOpenEvent | `OUT PHANDLE EventHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes` | ntoskrnl/ex/event.c:189 |
| 121 | NtOpenEventPair | `OUT PHANDLE EventPairHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes` | ntoskrnl/ex/evtpair.c:136 |
| 122 | NtOpenFile | `OUT PHANDLE FileHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes, OUT PIO_STATUS_BLOCK IoStatusBlock, IN ULONG ShareAccess, IN ULONG OpenOptions` | ntoskrnl/io/iomgr/file.c:3953 |
| 123 | NtOpenIoCompletion | `OUT PHANDLE IoCompletionHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes` | ntoskrnl/io/iomgr/iocomp.c:326 |
| 124 | NtOpenJobObject | `PHANDLE JobHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes` | ntoskrnl/ps/job.c:429 |
| 125 | NtOpenKey | `OUT PHANDLE KeyHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes` | ntoskrnl/config/ntapi.c:336 |
| 126 | NtOpenMutant | `OUT PHANDLE MutantHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes` | ntoskrnl/ex/mutant.c:162 |
| 127 | NtOpenObjectAuditAlarm | `IN PUNICODE_STRING SubsystemName, IN PVOID HandleId OPTIONAL, IN PUNICODE_STRING ObjectTypeName, IN PUNICODE_STRING ObjectName, IN PSECURITY_DESCRIPTOR SecurityDescriptor OPTIONAL, IN HANDLE ClientTokenHandle, IN ACCESS_MASK DesiredAccess, IN ACCESS_MASK GrantedAccess, IN PPRIVILEGE_SET PrivilegeSet OPTIONAL, IN BOOLEAN ObjectCreation, IN BOOLEAN AccessGranted, OUT PBOOLEAN GenerateOnClose` | ntoskrnl/se/audit.c:1622 |
| 128 | NtOpenProcess | `OUT PHANDLE ProcessHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes, IN PCLIENT_ID ClientId` | ntoskrnl/ps/process.c:1440 |
| 129 | NtOpenProcessToken | `IN HANDLE ProcessHandle, IN ACCESS_MASK DesiredAccess, OUT PHANDLE TokenHandle` | ntoskrnl/ps/security.c:350 |
| 130 | NtOpenProcessTokenEx | `IN HANDLE ProcessHandle, IN ACCESS_MASK DesiredAccess, IN ULONG HandleAttributes, OUT PHANDLE TokenHandle` | ntoskrnl/ps/security.c:366 |
| 131 | NtOpenSection | `OUT PHANDLE SectionHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes` | ntoskrnl/mm/ARM3/section.c:3204 |
| 132 | NtOpenSemaphore | `OUT PHANDLE SemaphoreHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes` | ntoskrnl/ex/sem.c:161 |
| 133 | NtOpenSymbolicLinkObject | `OUT PHANDLE LinkHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes` | ntoskrnl/ob/oblink.c:831 |
| 134 | NtOpenThread | `OUT PHANDLE ThreadHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes, IN PCLIENT_ID ClientId OPTIONAL` | ntoskrnl/ps/thread.c:1013 |
| 135 | NtOpenThreadToken | `IN HANDLE ThreadHandle, IN ACCESS_MASK DesiredAccess, IN BOOLEAN OpenAsSelf, OUT PHANDLE TokenHandle` | ntoskrnl/se/token.c:2475 |
| 136 | NtOpenThreadTokenEx | `IN HANDLE ThreadHandle, IN ACCESS_MASK DesiredAccess, IN BOOLEAN OpenAsSelf, IN ULONG HandleAttributes, OUT PHANDLE TokenHandle` | ntoskrnl/se/token.c:2332 |
| 137 | NtOpenTimer | `OUT PHANDLE TimerHandle, IN ACCESS_MASK DesiredAccess, IN POBJECT_ATTRIBUTES ObjectAttributes` | ntoskrnl/ex/timer.c:463 |
| 138 | NtPlugPlayControl | `IN PLUGPLAY_CONTROL_CLASS PlugPlayControlClass, IN OUT PVOID Buffer, IN ULONG BufferLength` | ntoskrnl/io/pnpmgr/plugplay.c:1573 |
| 139 | NtPowerInformation | `IN POWER_INFORMATION_LEVEL PowerInformationLevel, IN PVOID InputBuffer OPTIONAL, IN ULONG InputBufferLength, OUT PVOID OutputBuffer OPTIONAL, IN ULONG OutputBufferLength` | ntoskrnl/po/power.c:901 |
| 140 | NtPrivilegeCheck | `IN HANDLE ClientToken, IN PPRIVILEGE_SET RequiredPrivileges, OUT PBOOLEAN Result` | ntoskrnl/se/priv.c:868 |
| 141 | NtPrivilegeObjectAuditAlarm | `IN PUNICODE_STRING SubsystemName, IN PVOID HandleId, IN HANDLE ClientToken, IN ULONG DesiredAccess, IN PPRIVILEGE_SET Privileges, IN BOOLEAN AccessGranted` | ntoskrnl/se/audit.c:2066 |
| 142 | NtPrivilegedServiceAuditAlarm | `IN PUNICODE_STRING SubsystemName OPTIONAL, IN PUNICODE_STRING ServiceName OPTIONAL, IN HANDLE ClientTokenHandle, IN PPRIVILEGE_SET Privileges, IN BOOLEAN AccessGranted` | ntoskrnl/se/audit.c:1883 |
| 143 | NtProtectVirtualMemory | `IN HANDLE ProcessHandle, IN OUT PVOID *UnsafeBaseAddress, IN OUT SIZE_T *UnsafeNumberOfBytesToProtect, IN ULONG NewAccessProtection, OUT PULONG UnsafeOldAccessProtection` | ntoskrnl/mm/ARM3/virtual.c:3076 |
| 144 | NtPulseEvent | `IN HANDLE EventHandle, OUT PLONG PreviousState OPTIONAL` | ntoskrnl/ex/event.c:252 |
| 145 | NtQueryAttributesFile | `IN POBJECT_ATTRIBUTES ObjectAttributes, OUT PFILE_BASIC_INFORMATION FileInformation` | ntoskrnl/io/iomgr/file.c:3979 |
| 146 | NtQueryBootEntryOrder | `IN PULONG Ids, IN PULONG Count` | ntoskrnl/ex/efi.c:87 |
| 147 | NtQueryBootOptions | `IN PBOOT_OPTIONS BootOptions, IN PULONG BootOptionsLength` | ntoskrnl/ex/efi.c:105 |
| 148 | NtQueryDebugFilterState | `IN ULONG ComponentId, IN ULONG Level` | ntoskrnl/kd64/kdapi.c:2712 |
| 149 | NtQueryDefaultLocale | `IN BOOLEAN UserProfile, OUT PLCID DefaultLocaleId` | ntoskrnl/ex/locale.c:396 |
| 150 | NtQueryDefaultUILanguage | `OUT LANGID* LanguageId` | ntoskrnl/ex/locale.c:645 |
| 151 | NtQueryDirectoryFile | `IN HANDLE FileHandle, IN HANDLE EventHandle OPTIONAL, IN PIO_APC_ROUTINE ApcRoutine OPTIONAL, IN PVOID ApcContext OPTIONAL, OUT PIO_STATUS_BLOCK IoStatusBlock, OUT PVOID FileInformation, IN ULONG Length, IN FILE_INFORMATION_CLASS FileInformationClass, IN BOOLEAN ReturnSingleEntry, IN PUNICODE_STRING FileName OPTIONAL, IN BOOLEAN RestartScan` | ntoskrnl/io/iomgr/iofunc.c:1994 |
| 152 | NtQueryDirectoryObject | `IN HANDLE DirectoryHandle, OUT PVOID Buffer, IN ULONG BufferLength, IN BOOLEAN ReturnSingleEntry, IN BOOLEAN RestartScan, IN OUT PULONG Context, OUT PULONG ReturnLength OPTIONAL` | ntoskrnl/ob/obdir.c:490 |
| 153 | NtQueryDriverEntryOrder | `IN PULONG Ids, IN PULONG Count` | ntoskrnl/ex/efi.c:96 |
| 154 | NtQueryEaFile | `IN HANDLE FileHandle, OUT PIO_STATUS_BLOCK IoStatusBlock, OUT PVOID Buffer, IN ULONG Length, IN BOOLEAN ReturnSingleEntry, IN PVOID EaList OPTIONAL, IN ULONG EaListLength, IN PULONG EaIndex OPTIONAL, IN BOOLEAN RestartScan` | ntoskrnl/io/iomgr/iofunc.c:2260 |
| 155 | NtQueryEvent | `IN HANDLE EventHandle, IN EVENT_INFORMATION_CLASS EventInformationClass, OUT PVOID EventInformation, IN ULONG EventInformationLength, OUT PULONG ReturnLength OPTIONAL` | ntoskrnl/ex/event.c:321 |
| 156 | NtQueryFullAttributesFile | `IN POBJECT_ATTRIBUTES ObjectAttributes, OUT PFILE_NETWORK_OPEN_INFORMATION FileInformation` | ntoskrnl/io/iomgr/file.c:3991 |
| 157 | NtQueryInformationAtom | `RTL_ATOM Atom, ATOM_INFORMATION_CLASS AtomInformationClass, PVOID AtomInformation, ULONG AtomInformationLength, PULONG ReturnLength` | ntoskrnl/ex/atom.c:365 |
| 158 | NtQueryInformationFile | `IN HANDLE FileHandle, OUT PIO_STATUS_BLOCK IoStatusBlock, IN PVOID FileInformation, IN ULONG Length, IN FILE_INFORMATION_CLASS FileInformationClass` | ntoskrnl/io/iomgr/iofunc.c:2279 |
| 159 | NtQueryInformationJobObject | `HANDLE JobHandle, JOBOBJECTINFOCLASS JobInformationClass, PVOID JobInformation, ULONG JobInformationLength, PULONG ReturnLength` | ntoskrnl/ps/job.c:485 |
| 160 | NtQueryInformationPort | `IN HANDLE PortHandle, IN PORT_INFORMATION_CLASS PortInformationClass, OUT PVOID PortInformation, IN ULONG PortInformationLength, OUT PULONG ReturnLength` | ntoskrnl/lpc/port.c:285 |
| 161 | NtQueryInformationProcess | `IN HANDLE ProcessHandle, IN PROCESSINFOCLASS ProcessInformationClass, OUT PVOID ProcessInformation, IN ULONG ProcessInformationLength, OUT PULONG ReturnLength OPTIONAL` | ntoskrnl/ps/query.c:211 |
| 162 | NtQueryInformationThread | `IN HANDLE ThreadHandle, IN THREADINFOCLASS ThreadInformationClass, OUT PVOID ThreadInformation, IN ULONG ThreadInformationLength, OUT PULONG ReturnLength OPTIONAL` | ntoskrnl/ps/query.c:2881 |
| 163 | NtQueryInformationToken | `IN HANDLE TokenHandle, IN TOKEN_INFORMATION_CLASS TokenInformationClass, OUT PVOID TokenInformation, IN ULONG TokenInformationLength, OUT PULONG ReturnLength` | ntoskrnl/se/tokencls.c:473 |
| 164 | NtQueryInstallUILanguage | `OUT LANGID* LanguageId` | ntoskrnl/ex/locale.c:611 |
| 165 | NtQueryIntervalProfile | `IN KPROFILE_SOURCE ProfileSource, OUT PULONG Interval` | ntoskrnl/ex/profile.c:471 |
| 166 | NtQueryIoCompletion | `IN HANDLE IoCompletionHandle, IN IO_COMPLETION_INFORMATION_CLASS IoCompletionInformationClass, OUT PVOID IoCompletionInformation, IN ULONG IoCompletionInformationLength, OUT PULONG ResultLength OPTIONAL` | ntoskrnl/io/iomgr/iocomp.c:382 |
| 167 | NtQueryKey | `IN HANDLE KeyHandle, IN KEY_INFORMATION_CLASS KeyInformationClass, OUT PVOID KeyInformation, IN ULONG Length, OUT PULONG ResultLength` | ntoskrnl/config/ntapi.c:632 |
| 168 | NtQueryMultipleValueKey | `IN HANDLE KeyHandle, IN OUT PKEY_VALUE_ENTRY ValueList, IN ULONG NumberOfValues, OUT PVOID Buffer, IN OUT PULONG Length, OUT PULONG ReturnLength` | ntoskrnl/config/ntapi.c:1476 |
| 169 | NtQueryMutant | `IN HANDLE MutantHandle, IN MUTANT_INFORMATION_CLASS MutantInformationClass, OUT PVOID MutantInformation, IN ULONG MutantInformationLength, OUT PULONG ResultLength OPTIONAL` | ntoskrnl/ex/mutant.c:224 |
| 170 | NtQueryObject | `IN HANDLE ObjectHandle, IN OBJECT_INFORMATION_CLASS ObjectInformationClass, OUT PVOID ObjectInformation, IN ULONG Length, OUT PULONG ResultLength OPTIONAL` | ntoskrnl/ob/oblife.c:1566 |
| 171 | NtQueryOpenSubKeys | `IN POBJECT_ATTRIBUTES TargetKey, OUT PULONG HandleCount` | ntoskrnl/config/ntapi.c:1489 |
| 172 | NtQueryOpenSubKeysEx | `IN POBJECT_ATTRIBUTES TargetKey, IN ULONG BufferLength, IN PVOID Buffer, IN PULONG RequiredSize` | ntoskrnl/config/ntapi.c:1594 |
| 173 | NtQueryPerformanceCounter | `OUT PLARGE_INTEGER PerformanceCounter, OUT PLARGE_INTEGER PerformanceFrequency OPTIONAL` | ntoskrnl/ex/profile.c:278 |
| 174 | NtQueryQuotaInformationFile | `IN HANDLE FileHandle, OUT PIO_STATUS_BLOCK IoStatusBlock, OUT PVOID Buffer, IN ULONG Length, IN BOOLEAN ReturnSingleEntry, IN PVOID SidList OPTIONAL, IN ULONG SidListLength, IN PSID StartSid OPTIONAL, IN BOOLEAN RestartScan` | ntoskrnl/io/iomgr/iofunc.c:2686 |
| 175 | NtQuerySection | `IN HANDLE SectionHandle, IN SECTION_INFORMATION_CLASS SectionInformationClass, OUT PVOID SectionInformation, IN SIZE_T SectionInformationLength, OUT PSIZE_T ResultLength OPTIONAL` | ntoskrnl/mm/section.c:3808 |
| 176 | NtQuerySecurityObject | `IN HANDLE Handle, IN SECURITY_INFORMATION SecurityInformation, OUT PSECURITY_DESCRIPTOR SecurityDescriptor, IN ULONG Length, OUT PULONG ResultLength` | ntoskrnl/ob/obsecure.c:803 |
| 177 | NtQuerySemaphore | `IN HANDLE SemaphoreHandle, IN SEMAPHORE_INFORMATION_CLASS SemaphoreInformationClass, OUT PVOID SemaphoreInformation, IN ULONG SemaphoreInformationLength, OUT PULONG ReturnLength OPTIONAL` | ntoskrnl/ex/sem.c:222 |
| 178 | NtQuerySymbolicLinkObject | `IN HANDLE LinkHandle, OUT PUNICODE_STRING LinkTarget, OUT PULONG ResultLength OPTIONAL` | ntoskrnl/ob/oblink.c:903 |
| 179 | NtQuerySystemEnvironmentValue | `IN PUNICODE_STRING VariableName, OUT PWSTR ValueBuffer, IN ULONG ValueBufferLength, IN OUT PULONG ReturnLength OPTIONAL` | ntoskrnl/ex/sysinfo.c:385 |
| 180 | NtQuerySystemEnvironmentValueEx | `IN PUNICODE_STRING VariableName, IN LPGUID VendorGuid, OUT PVOID Value OPTIONAL, IN OUT PULONG ReturnLength, OUT PULONG Attributes OPTIONAL` | ntoskrnl/ex/sysinfo.c:567 |
| 181 | NtQuerySystemInformation | `IN SYSTEM_INFORMATION_CLASS SystemInformationClass, OUT PVOID SystemInformation, IN ULONG SystemInformationLength, OUT PULONG ReturnLength OPTIONAL` | ntoskrnl/ex/sysinfo.c:2938 |
| 182 | NtQuerySystemTime | `OUT PLARGE_INTEGER SystemTime` | ntoskrnl/ex/time.c:569 |
| 183 | NtQueryTimer | `IN HANDLE TimerHandle, IN TIMER_INFORMATION_CLASS TimerInformationClass, OUT PVOID TimerInformation, IN ULONG TimerInformationLength, OUT PULONG ReturnLength OPTIONAL` | ntoskrnl/ex/timer.c:518 |
| 184 | NtQueryTimerResolution | `OUT PULONG MinimumResolution, OUT PULONG MaximumResolution, OUT PULONG ActualResolution` | ntoskrnl/ex/time.c:633 |
| 185 | NtQueryValueKey | `IN HANDLE KeyHandle, IN PUNICODE_STRING ValueName, IN KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass, OUT PVOID KeyValueInformation, IN ULONG Length, OUT PULONG ResultLength` | ntoskrnl/config/ntapi.c:744 |
| 186 | NtQueryVirtualMemory | `IN HANDLE ProcessHandle, IN PVOID BaseAddress, IN MEMORY_INFORMATION_CLASS MemoryInformationClass, OUT PVOID MemoryInformation, IN SIZE_T MemoryInformationLength, OUT PSIZE_T ReturnLength` | ntoskrnl/mm/ARM3/virtual.c:4374 |
| 187 | NtQueryVolumeInformationFile | `IN HANDLE FileHandle, OUT PIO_STATUS_BLOCK IoStatusBlock, OUT PVOID FsInformation, IN ULONG Length, IN FS_INFORMATION_CLASS FsInformationClass` | ntoskrnl/io/iomgr/iofunc.c:4151 |
| 188 | NtQueueApcThread | `IN HANDLE ThreadHandle, IN PKNORMAL_ROUTINE ApcRoutine, IN PVOID NormalContext, IN PVOID SystemArgument1, IN PVOID SystemArgument2` | ntoskrnl/ps/state.c:600 |
| 189 | NtRaiseException | `IN PEXCEPTION_RECORD ExceptionRecord, IN PCONTEXT Context, IN BOOLEAN FirstChance` | ntoskrnl/ke/except.c:173 |
| 190 | NtRaiseHardError | `IN NTSTATUS ErrorStatus, IN ULONG NumberOfParameters, IN ULONG UnicodeStringParameterMask, IN PULONG_PTR Parameters, IN ULONG ValidResponseOptions, OUT PULONG Response` | ntoskrnl/ex/harderr.c:551 |
| 191 | NtReadFile | `IN HANDLE FileHandle, IN HANDLE Event OPTIONAL, IN PIO_APC_ROUTINE ApcRoutine OPTIONAL, IN PVOID ApcContext OPTIONAL, OUT PIO_STATUS_BLOCK IoStatusBlock, OUT PVOID Buffer, IN ULONG Length, IN PLARGE_INTEGER ByteOffset OPTIONAL, IN PULONG Key OPTIONAL` | ntoskrnl/io/iomgr/iofunc.c:2705 |
| 192 | NtReadFileScatter | `IN HANDLE FileHandle, IN HANDLE Event OPTIONAL, IN PIO_APC_ROUTINE UserApcRoutine OPTIONAL, IN PVOID UserApcContext OPTIONAL, OUT PIO_STATUS_BLOCK UserIoStatusBlock, IN FILE_SEGMENT_ELEMENT BufferDescription[], IN ULONG BufferLength, IN PLARGE_INTEGER ByteOffset, IN PULONG Key OPTIONAL` | ntoskrnl/io/iomgr/iofunc.c:3063 |
| 193 | NtReadRequestData | `IN HANDLE PortHandle, IN PPORT_MESSAGE Message, IN ULONG Index, IN PVOID Buffer, IN ULONG BufferLength, OUT PULONG ReturnLength` | ntoskrnl/lpc/reply.c:965 |
| 194 | NtReadVirtualMemory | `IN HANDLE ProcessHandle, IN PVOID BaseAddress, OUT PVOID Buffer, IN SIZE_T NumberOfBytesToRead, OUT PSIZE_T NumberOfBytesRead OPTIONAL` | ntoskrnl/mm/ARM3/virtual.c:2781 |
| 195 | NtRegisterThreadTerminatePort | `IN HANDLE PortHandle` | ntoskrnl/ps/kill.c:1342 |
| 196 | NtReleaseMutant | `IN HANDLE MutantHandle, IN PLONG PreviousCount OPTIONAL` | ntoskrnl/ex/mutant.c:296 |
| 197 | NtReleaseSemaphore | `IN HANDLE SemaphoreHandle, IN LONG ReleaseCount, OUT PLONG PreviousCount OPTIONAL` | ntoskrnl/ex/sem.c:295 |
| 198 | NtRemoveIoCompletion | `IN HANDLE IoCompletionHandle, OUT PVOID *KeyContext, OUT PVOID *ApcContext, OUT PIO_STATUS_BLOCK IoStatusBlock, IN PLARGE_INTEGER Timeout OPTIONAL` | ntoskrnl/io/iomgr/iocomp.c:445 |
| 199 | NtRemoveProcessDebug | `IN HANDLE ProcessHandle, IN HANDLE DebugHandle` | ntoskrnl/dbgk/dbgkobj.c:1873 |
| 200 | NtRenameKey | `IN HANDLE KeyHandle, IN PUNICODE_STRING ReplacementName` | ntoskrnl/config/ntapi.c:1605 |
| 201 | NtReplaceKey | `IN POBJECT_ATTRIBUTES ObjectAttributes, IN HANDLE Key, IN POBJECT_ATTRIBUTES ReplacedObjectAttributes` | ntoskrnl/config/ntapi.c:1614 |
| 202 | NtReplyPort | `IN HANDLE PortHandle, IN PPORT_MESSAGE ReplyMessage` | ntoskrnl/lpc/reply.c:190 |
| 203 | NtReplyWaitReceivePort | `IN HANDLE PortHandle, OUT PVOID *PortContext OPTIONAL, IN PPORT_MESSAGE ReplyMessage OPTIONAL, OUT PPORT_MESSAGE ReceiveMessage` | ntoskrnl/lpc/reply.c:743 |
| 204 | NtReplyWaitReceivePortEx | `IN HANDLE PortHandle, OUT PVOID *PortContext OPTIONAL, IN PPORT_MESSAGE ReplyMessage OPTIONAL, OUT PPORT_MESSAGE ReceiveMessage, IN PLARGE_INTEGER Timeout OPTIONAL` | ntoskrnl/lpc/reply.c:360 |
| 205 | NtReplyWaitReplyPort | `IN HANDLE PortHandle, IN PPORT_MESSAGE ReplyMessage` | ntoskrnl/lpc/reply.c:761 |
| 206 | NtRequestDeviceWakeup | `IN HANDLE DeviceHandle` | ntoskrnl/io/iomgr/iofunc.c:4642 |
| 207 | NtRequestPort | `IN HANDLE PortHandle, IN PPORT_MESSAGE LpcRequest` | ntoskrnl/lpc/send.c:440 |
| 208 | NtRequestWaitReplyPort | `IN HANDLE PortHandle, IN PPORT_MESSAGE LpcRequest, IN OUT PPORT_MESSAGE LpcReply` | ntoskrnl/lpc/send.c:696 |
| 209 | NtRequestWakeupLatency | `IN LATENCY_TIME Latency` | ntoskrnl/po/power.c:1062 |
| 210 | NtResetEvent | `IN HANDLE EventHandle, OUT PLONG PreviousState OPTIONAL` | ntoskrnl/ex/event.c:394 |
| 211 | NtResetWriteWatch | `IN HANDLE ProcessHandle, IN PVOID BaseAddress, IN SIZE_T RegionSize` | ntoskrnl/mm/ARM3/virtual.c:4293 |
| 212 | NtRestoreKey | `IN HANDLE KeyHandle, IN HANDLE FileHandle, IN ULONG RestoreFlags` | ntoskrnl/config/ntapi.c:1624 |
| 213 | NtResumeProcess | `IN HANDLE ProcessHandle` | ntoskrnl/ps/state.c:438 |
| 214 | NtResumeThread | `IN HANDLE ThreadHandle, OUT PULONG SuspendCount OPTIONAL` | ntoskrnl/ps/state.c:290 |
| 215 | NtSaveKey | `IN HANDLE KeyHandle, IN HANDLE FileHandle` | ntoskrnl/config/ntapi.c:1634 |
| 216 | NtSaveKeyEx | `IN HANDLE KeyHandle, IN HANDLE FileHandle, IN ULONG Flags` | ntoskrnl/config/ntapi.c:1643 |
| 217 | NtSaveMergedKeys | `IN HANDLE HighPrecedenceKeyHandle, IN HANDLE LowPrecedenceKeyHandle, IN HANDLE FileHandle` | ntoskrnl/config/ntapi.c:1706 |
| 218 | NtSecureConnectPort | `OUT PHANDLE PortHandle, IN PUNICODE_STRING PortName, IN PSECURITY_QUALITY_OF_SERVICE SecurityQos, IN OUT PPORT_VIEW ClientView OPTIONAL, IN PSID ServerSid OPTIONAL, IN OUT PREMOTE_PORT_VIEW ServerView OPTIONAL, OUT PULONG MaxMessageLength OPTIONAL, IN OUT PVOID ConnectionInformation OPTIONAL, IN OUT PULONG ConnectionInformationLength OPTIONAL` | ntoskrnl/lpc/connect.c:80 |
| 219 | NtSetBootEntryOrder | `IN PULONG Ids, IN PULONG Count` | ntoskrnl/ex/efi.c:114 |
| 220 | NtSetBootOptions | `IN PBOOT_OPTIONS BootOptions, IN ULONG FieldsToChange` | ntoskrnl/ex/efi.c:132 |
| 221 | NtSetContextThread | `IN HANDLE ThreadHandle, IN PCONTEXT ThreadContext` | ntoskrnl/ps/debug.c:387 |
| 222 | NtSetDebugFilterState | `IN ULONG ComponentId, IN ULONG Level, IN BOOLEAN State` | ntoskrnl/kd64/kdapi.c:2766 |
| 223 | NtSetDefaultHardErrorPort | `IN HANDLE PortHandle` | ntoskrnl/ex/harderr.c:740 |
| 224 | NtSetDefaultLocale | `IN BOOLEAN UserProfile, IN LCID DefaultLocaleId` | ntoskrnl/ex/locale.c:437 |
| 225 | NtSetDefaultUILanguage | `IN LANGID LanguageId` | ntoskrnl/ex/locale.c:692 |
| 226 | NtSetDriverEntryOrder | `IN PULONG Ids, IN PULONG Count` | ntoskrnl/ex/efi.c:123 |
| 227 | NtSetEaFile | `IN HANDLE FileHandle, IN PIO_STATUS_BLOCK IoStatusBlock, IN PVOID EaBuffer, IN ULONG EaBufferSize` | ntoskrnl/io/iomgr/iofunc.c:3082 |
| 228 | NtSetEvent | `IN HANDLE EventHandle, OUT PLONG PreviousState OPTIONAL` | ntoskrnl/ex/event.c:463 |
| 229 | NtSetEventBoostPriority | `IN HANDLE EventHandle` | ntoskrnl/ex/event.c:529 |
| 230 | NtSetHighEventPair | `IN HANDLE EventPairHandle` | ntoskrnl/ex/evtpair.c:194 |
| 231 | NtSetHighWaitLowEventPair | `IN HANDLE EventPairHandle` | ntoskrnl/ex/evtpair.c:226 |
| 232 | NtSetInformationDebugObject | `IN HANDLE DebugHandle, IN DEBUGOBJECTINFOCLASS DebugObjectInformationClass, IN PVOID DebugInformation, IN ULONG DebugInformationLength, OUT PULONG ReturnLength OPTIONAL` | ntoskrnl/dbgk/dbgkobj.c:1921 |
| 233 | NtSetInformationFile | `IN HANDLE FileHandle, OUT PIO_STATUS_BLOCK IoStatusBlock, IN PVOID FileInformation, IN ULONG Length, IN FILE_INFORMATION_CLASS FileInformationClass` | ntoskrnl/io/iomgr/iofunc.c:3096 |
| 234 | NtSetInformationJobObject | `HANDLE JobHandle, JOBOBJECTINFOCLASS JobInformationClass, PVOID JobInformation, ULONG JobInformationLength` | ntoskrnl/ps/job.c:752 |
| 235 | NtSetInformationKey | `IN HANDLE KeyHandle, IN KEY_SET_INFORMATION_CLASS KeyInformationClass, IN PVOID KeyInformation, IN ULONG KeyInformationLength` | ntoskrnl/config/ntapi.c:1778 |
| 236 | NtSetInformationObject | `IN HANDLE ObjectHandle, IN OBJECT_INFORMATION_CLASS ObjectInformationClass, IN PVOID ObjectInformation, IN ULONG Length` | ntoskrnl/ob/oblife.c:1824 |
| 237 | NtSetInformationProcess | `IN HANDLE ProcessHandle, IN PROCESSINFOCLASS ProcessInformationClass, IN PVOID ProcessInformation, IN ULONG ProcessInformationLength` | ntoskrnl/ps/query.c:1389 |
| 238 | NtSetInformationThread | `IN HANDLE ThreadHandle, IN THREADINFOCLASS ThreadInformationClass, IN PVOID ThreadInformation, IN ULONG ThreadInformationLength` | ntoskrnl/ps/query.c:2268 |
| 239 | NtSetInformationToken | `IN HANDLE TokenHandle, IN TOKEN_INFORMATION_CLASS TokenInformationClass, IN PVOID TokenInformation, IN ULONG TokenInformationLength` | ntoskrnl/se/tokencls.c:1125 |
| 240 | NtSetIntervalProfile | `IN ULONG Interval, IN KPROFILE_SOURCE Source` | ntoskrnl/ex/profile.c:518 |
| 241 | NtSetIoCompletion | `IN HANDLE IoCompletionPortHandle, IN PVOID CompletionKey, IN PVOID CompletionContext, IN NTSTATUS CompletionStatus, IN ULONG CompletionInformation` | ntoskrnl/io/iomgr/iocomp.c:569 |
| 242 | NtSetLdtEntries | `ULONG Selector1, LDT_ENTRY LdtEntry1, ULONG Selector2, LDT_ENTRY LdtEntry2` | ntoskrnl/ke/amd64/stubs.c:189 |
| 243 | NtSetLowEventPair | `IN HANDLE EventPairHandle` | ntoskrnl/ex/evtpair.c:265 |
| 244 | NtSetLowWaitHighEventPair | `IN HANDLE EventPairHandle` | ntoskrnl/ex/evtpair.c:297 |
| 245 | NtSetQuotaInformationFile | `IN HANDLE FileHandle, OUT PIO_STATUS_BLOCK IoStatusBlock, IN PVOID Buffer, IN ULONG BufferLength` | ntoskrnl/io/iomgr/iofunc.c:3537 |
| 246 | NtSetSecurityObject | `IN HANDLE Handle, IN SECURITY_INFORMATION SecurityInformation, IN PSECURITY_DESCRIPTOR SecurityDescriptor` | ntoskrnl/ob/obsecure.c:903 |
| 247 | NtSetSystemEnvironmentValue | `IN PUNICODE_STRING VariableName, IN PUNICODE_STRING Value` | ntoskrnl/ex/sysinfo.c:487 |
| 248 | NtSetSystemEnvironmentValueEx | `IN PUNICODE_STRING VariableName, IN LPGUID VendorGuid, IN PVOID Value OPTIONAL, IN ULONG ValueLength, IN ULONG Attributes` | ntoskrnl/ex/sysinfo.c:580 |
| 249 | NtSetSystemInformation | `IN SYSTEM_INFORMATION_CLASS SystemInformationClass, IN PVOID SystemInformation, IN ULONG SystemInformationLength` | ntoskrnl/ex/sysinfo.c:3015 |
| 250 | NtSetSystemPowerState | `IN POWER_ACTION SystemAction, IN SYSTEM_POWER_STATE MinSystemState, IN ULONG Flags` | ntoskrnl/po/power.c:1127 |
| 251 | NtSetSystemTime | `IN PLARGE_INTEGER SystemTime, OUT PLARGE_INTEGER PreviousTime OPTIONAL` | ntoskrnl/ex/time.c:458 |
| 252 | NtSetThreadExecutionState | `IN EXECUTION_STATE esFlags, OUT EXECUTION_STATE *PreviousFlags` | ntoskrnl/po/power.c:1070 |
| 253 | NtSetTimer | `IN HANDLE TimerHandle, IN PLARGE_INTEGER DueTime, IN PTIMER_APC_ROUTINE TimerApcRoutine OPTIONAL, IN PVOID TimerContext OPTIONAL, IN BOOLEAN WakeTimer, IN LONG Period OPTIONAL, OUT PBOOLEAN PreviousState OPTIONAL` | ntoskrnl/ex/timer.c:583 |
| 254 | NtSetTimerResolution | `IN ULONG DesiredResolution, IN BOOLEAN SetResolution, OUT PULONG CurrentResolution` | ntoskrnl/ex/time.c:684 |
| 255 | NtSetUuidSeed | `IN PUCHAR Seed` | ntoskrnl/ex/uuid.c:547 |
| 256 | NtSetValueKey | `IN HANDLE KeyHandle, IN PUNICODE_STRING ValueName, IN ULONG TitleIndex, IN ULONG Type, IN PVOID Data, IN ULONG DataSize` | ntoskrnl/config/ntapi.c:859 |
| 257 | NtSetVolumeInformationFile | `IN HANDLE FileHandle, OUT PIO_STATUS_BLOCK IoStatusBlock, IN PVOID FsInformation, IN ULONG Length, IN FS_INFORMATION_CLASS FsInformationClass` | ntoskrnl/io/iomgr/iofunc.c:4438 |
| 258 | NtShutdownSystem | `IN SHUTDOWN_ACTION Action` | ntoskrnl/ex/shutdown.c:43 |
| 259 | NtSignalAndWaitForSingleObject | `IN HANDLE ObjectHandleToSignal, IN HANDLE WaitableObjectHandle, IN BOOLEAN Alertable, IN PLARGE_INTEGER TimeOut OPTIONAL` | ntoskrnl/ob/obwait.c:473 |
| 260 | NtStartProfile | `IN HANDLE ProfileHandle` | ntoskrnl/ex/profile.c:328 |
| 261 | NtStopProfile | `IN HANDLE ProfileHandle` | ntoskrnl/ex/profile.c:420 |
| 262 | NtSuspendProcess | `IN HANDLE ProcessHandle` | ntoskrnl/ps/state.c:411 |
| 263 | NtSuspendThread | `IN HANDLE ThreadHandle, OUT PULONG PreviousSuspendCount OPTIONAL` | ntoskrnl/ps/state.c:352 |
| 264 | NtSystemDebugControl | `IN SYSDBG_COMMAND Command, IN PVOID InputBuffer, IN ULONG InputBufferLength, OUT PVOID OutputBuffer, IN ULONG OutputBufferLength, OUT PULONG ReturnLength OPTIONAL` | ntoskrnl/ex/dbgctrl.c:209 |
| 265 | NtTerminateJobObject | `HANDLE JobHandle, NTSTATUS ExitStatus` | ntoskrnl/ps/job.c:850 |
| 266 | NtTerminateProcess | `IN HANDLE ProcessHandle OPTIONAL, IN NTSTATUS ExitStatus` | ntoskrnl/ps/kill.c:1161 |
| 267 | NtTerminateThread | `IN HANDLE ThreadHandle, IN NTSTATUS ExitStatus` | ntoskrnl/ps/kill.c:1279 |
| 268 | NtTestAlert | `VOID` | ntoskrnl/ps/state.c:465 |
| 269 | NtTraceEvent | `IN ULONG TraceHandle, IN ULONG Flags, IN ULONG TraceHeaderLength, IN struct _EVENT_TRACE_HEADER* TraceHeader` | ntoskrnl/wmi/wmi.c:440 |
| 270 | NtTranslateFilePath | `PFILE_PATH InputFilePath, ULONG OutputType, PFILE_PATH OutputFilePath, ULONG OutputFilePathLength` | ntoskrnl/ex/efi.c:141 |
| 271 | NtUnloadDriver | `IN PUNICODE_STRING DriverServiceName` | ntoskrnl/io/iomgr/driver.c:2226 |
| 272 | NtUnloadKey | `IN POBJECT_ATTRIBUTES KeyObjectAttributes` | ntoskrnl/config/ntapi.c:1789 |
| 273 | NtUnloadKey2 | `IN POBJECT_ATTRIBUTES TargetKey, IN ULONG Flags` | ntoskrnl/config/ntapi.c:1796 |
| 274 | NtUnloadKeyEx | `IN POBJECT_ATTRIBUTES TargetKey, IN HANDLE Event` | ntoskrnl/config/ntapi.c:1946 |
| 275 | NtUnlockFile | `IN HANDLE FileHandle, OUT PIO_STATUS_BLOCK IoStatusBlock, IN PLARGE_INTEGER ByteOffset, IN PLARGE_INTEGER Length, IN ULONG Key OPTIONAL` | ntoskrnl/io/iomgr/iofunc.c:3551 |
| 276 | NtUnlockVirtualMemory | `IN HANDLE ProcessHandle, IN OUT PVOID *BaseAddress, IN OUT PSIZE_T NumberOfBytesToUnlock, IN ULONG MapType` | ntoskrnl/mm/ARM3/virtual.c:3830 |
| 277 | NtUnmapViewOfSection | `IN HANDLE ProcessHandle, IN PVOID BaseAddress` | ntoskrnl/mm/ARM3/section.c:3483 |
| 278 | NtVdmControl | `IN ULONG ControlCode, IN PVOID ControlData` | ntoskrnl/vdm/vdmmain.c:173 |
| 279 | NtWaitForDebugEvent | `IN HANDLE DebugHandle, IN BOOLEAN Alertable, IN PLARGE_INTEGER Timeout OPTIONAL, OUT PDBGUI_WAIT_STATE_CHANGE StateChange` | ntoskrnl/dbgk/dbgkobj.c:2001 |
| 280 | NtWaitForMultipleObjects | `IN ULONG ObjectCount, IN PHANDLE HandleArray, IN WAIT_TYPE WaitType, IN BOOLEAN Alertable, IN PLARGE_INTEGER TimeOut OPTIONAL` | ntoskrnl/ob/obwait.c:46 |
| 281 | NtWaitForSingleObject | `IN HANDLE ObjectHandle, IN BOOLEAN Alertable, IN PLARGE_INTEGER TimeOut OPTIONAL` | ntoskrnl/ob/obwait.c:369 |
| 282 | NtWaitHighEventPair | `IN HANDLE EventPairHandle` | ntoskrnl/ex/evtpair.c:373 |
| 283 | NtWaitLowEventPair | `IN HANDLE EventPairHandle` | ntoskrnl/ex/evtpair.c:337 |
| 284 | NtWriteFile | `IN HANDLE FileHandle, IN HANDLE Event OPTIONAL, IN PIO_APC_ROUTINE ApcRoutine OPTIONAL, IN PVOID ApcContext OPTIONAL, OUT PIO_STATUS_BLOCK IoStatusBlock, IN PVOID Buffer, IN ULONG Length, IN PLARGE_INTEGER ByteOffset OPTIONAL, IN PULONG Key OPTIONAL` | ntoskrnl/io/iomgr/iofunc.c:3763 |
| 285 | NtWriteFileGather | `IN HANDLE FileHandle, IN HANDLE Event OPTIONAL, IN PIO_APC_ROUTINE UserApcRoutine OPTIONAL, IN PVOID UserApcContext OPTIONAL, OUT PIO_STATUS_BLOCK UserIoStatusBlock, IN FILE_SEGMENT_ELEMENT BufferDescription [], IN ULONG BufferLength, IN PLARGE_INTEGER ByteOffset, IN PULONG Key OPTIONAL` | ntoskrnl/io/iomgr/iofunc.c:4132 |
| 286 | NtWriteRequestData | `IN HANDLE PortHandle, IN PPORT_MESSAGE Message, IN ULONG Index, IN PVOID Buffer, IN ULONG BufferLength, OUT PULONG ReturnLength` | ntoskrnl/lpc/reply.c:987 |
| 287 | NtWriteVirtualMemory | `IN HANDLE ProcessHandle, IN PVOID BaseAddress, IN PVOID Buffer, IN SIZE_T NumberOfBytesToWrite, OUT PSIZE_T NumberOfBytesWritten OPTIONAL` | ntoskrnl/mm/ARM3/virtual.c:2895 |
| 288 | NtYieldExecution | `VOID` | ntoskrnl/ke/thrdschd.c:887 |
| 289 | NtCreateKeyedEvent | `OUT PHANDLE OutHandle, IN ACCESS_MASK AccessMask, IN POBJECT_ATTRIBUTES ObjectAttributes, IN ULONG Flags` | ntoskrnl/ex/keyedevt.c:275 |
| 290 | NtOpenKeyedEvent | `OUT PHANDLE OutHandle, IN ACCESS_MASK AccessMask, IN POBJECT_ATTRIBUTES ObjectAttributes` | ntoskrnl/ex/keyedevt.c:352 |
| 291 | NtReleaseKeyedEvent | `IN HANDLE Handle OPTIONAL, IN PVOID Key, IN BOOLEAN Alertable, IN PLARGE_INTEGER Timeout OPTIONAL` | ntoskrnl/ex/keyedevt.c:473 |
| 292 | NtWaitForKeyedEvent | `IN HANDLE Handle OPTIONAL, IN PVOID Key, IN BOOLEAN Alertable, IN PLARGE_INTEGER Timeout OPTIONAL` | ntoskrnl/ex/keyedevt.c:404 |
| 293 | NtQueryPortInformationProcess | `VOID` | ntoskrnl/lpc/port.c:277 |
| 294 | NtGetCurrentProcessorNumber | `VOID` | ntoskrnl/ex/sysinfo.c:3062 |
| 295 | NtWaitForMultipleObjects32 | `IN ULONG ObjectCount, IN PLONG Handles, IN WAIT_TYPE WaitType, IN BOOLEAN Alertable, IN PLARGE_INTEGER TimeOut OPTIONAL` | ntoskrnl/ob/obwait.c:333 |
