NTSTATUS __stdcall NtQueryObject(                                                                                                                                                          | 
        HANDLE Handle,                                                                                                                                                                     | 
        OBJECT_INFORMATION_CLASS ObjectInformationClass,                                                                                                                                   | 
        PVOID ObjectInformation,                                                                                                                                                           | 
        ULONG ObjectInformationLength,                                                                                                                                                     | 
        PULONG ReturnLength)                                                                                                                                                               | 
{                                                                                                                                                                                          | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                                                                                            | 
                                                                                                                                                                                           | 
  v5 = ObjectInformationLength;                                                                                                                                                            | 
  v6 = (char *)ObjectInformation;                                                                                                                                                          | 
  v9 = ReturnLength;                                                                                                                                                                       | 
  v10 = 0;                                                                                                                                                                                 | 
  HandleInformation = 0LL;                                                                                                                                                                 | 
  v71 = 0LL;                                                                                                                                                                               | 
  memset(&v72, 0, sizeof(v72));                                                                                                                                                            | 
  v73 = 0LL;                                                                                                                                                                               | 
  v64 = 0;                                                                                                                                                                                 | 
  v63 = 0;                                                                                                                                                                                 | 
  v57 = 0;                                                                                                                                                                                 | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                                                                                       | 
  v58 = PreviousMode;                                                                                                                                                                      | 
  if ( !PreviousMode )                                                                                                                                                                     | 
  {                                                                                                                                                                                        | 
    goto LABEL_23;                                                                                                                                                                         | 
  }                                                                                                                                                                                        | 
  if ( ObjectInformationClass != ObjectHandleFlagInformation )                                                                                                                             | 
  {                                                                                                                                                                                        | 
    if ( ObjectInformationLength )                                                                                                                                                         | 
    {                                                                                                                                                                                      | 
      v12 = (unsigned __int64)ObjectInformation;                                                                                                                                           | 
      if ( ((unsigned __int8)ObjectInformation & 3) != 0 )                                                                                                                                 | 
      {                                                                                                                                                                                    | 
        ExRaiseDatatypeMisalignment();                                                                                                                                                     | 
      }                                                                                                                                                                                    | 
      v13 = (unsigned __int64)ObjectInformation + ObjectInformationLength - 1;                                                                                                             | 
      if ( (unsigned __int64)ObjectInformation <= v13 )                                                                                                                                    | 
      {                                                                                                                                                                                    | 
        v14 = 0x7FFFFFFF0000LL;                                                                                                                                                            | 
        if ( v13 < 0x7FFFFFFF0000LL )                                                                                                                                                      | 
        {                                                                                                                                                                                  | 
          v15 = (v13 & 0xFFFFFFFFFFFFF000uLL) + 4096;                                                                                                                                      | 
          do                                                                                                                                                                               | 
          {                                                                                                                                                                                | 
            *(_BYTE *)v12 = *(_BYTE *)v12;                                                                                                                                                 | 
            v12 = (v12 & 0xFFFFFFFFFFFFF000uLL) + 4096;                                                                                                                                    | 
          }                                                                                                                                                                                | 
          while ( v12 != v15 );                                                                                                                                                            | 
          goto LABEL_11;                                                                                                                                                                   | 
        }                                                                                                                                                                                  | 
      }                                                                                                                                                                                    | 
LABEL_15:                                                                                                                                                                                  | 
      ExRaiseAccessViolation();                                                                                                                                                            | 
    }                                                                                                                                                                                      | 
LABEL_10:                                                                                                                                                                                  | 
    v14 = 0x7FFFFFFF0000LL;                                                                                                                                                                | 
    goto LABEL_11;                                                                                                                                                                         | 
  }                                                                                                                                                                                        | 
  if ( !ObjectInformationLength )                                                                                                                                                          | 
  {                                                                                                                                                                                        | 
    goto LABEL_10;                                                                                                                                                                         | 
  }                                                                                                                                                                                        | 
  v16 = (unsigned __int64)ObjectInformation;                                                                                                                                               | 
  v17 = (unsigned __int64)ObjectInformation + ObjectInformationLength - 1;                                                                                                                 | 
  if ( (unsigned __int64)ObjectInformation > v17 )                                                                                                                                         | 
  {                                                                                                                                                                                        | 
    goto LABEL_15;                                                                                                                                                                         | 
  }                                                                                                                                                                                        | 
  v14 = 0x7FFFFFFF0000LL;                                                                                                                                                                  | 
  if ( v17 >= 0x7FFFFFFF0000LL )                                                                                                                                                           | 
  {                                                                                                                                                                                        | 
    goto LABEL_15;                                                                                                                                                                         | 
  }                                                                                                                                                                                        | 
  v18 = (v17 & 0xFFFFFFFFFFFFF000uLL) + 4096;                                                                                                                                              | 
  do                                                                                                                                                                                       | 
  {                                                                                                                                                                                        | 
    *(_BYTE *)v16 = *(_BYTE *)v16;                                                                                                                                                         | 
    v16 = (v16 & 0xFFFFFFFFFFFFF000uLL) + 4096;                                                                                                                                            | 
  }                                                                                                                                                                                        | 
  while ( v16 != v18 );                                                                                                                                                                    | 
LABEL_11:                                                                                                                                                                                  | 
  if ( ReturnLength )                                                                                                                                                                      | 
  {                                                                                                                                                                                        | 
    if ( (unsigned __int64)ReturnLength < 0x7FFFFFFF0000LL )                                                                                                                               | 
    {                                                                                                                                                                                      | 
      v14 = (__int64)ReturnLength;                                                                                                                                                         | 
    }                                                                                                                                                                                      | 
    *(_DWORD *)v14 = *(_DWORD *)v14;                                                                                                                                                       | 
  }                                                                                                                                                                                        | 
LABEL_23:                                                                                                                                                                                  | 
  if ( ObjectInformationClass == ObjectTypesInformation )                                                                                                                                  | 
  {                                                                                                                                                                                        | 
    GrantedAccess = 0;                                                                                                                                                                     | 
    v62 = 0;                                                                                                                                                                               | 
    v21 = 0LL;                                                                                                                                                                             | 
    v65 = 0LL;                                                                                                                                                                             | 
    v23 = 0LL;                                                                                                                                                                             | 
    v24 = 0LL;                                                                                                                                                                             | 
    v67 = 0LL;                                                                                                                                                                             | 
    v20 = 0;                                                                                                                                                                               | 
    v56 = 0;                                                                                                                                                                               | 
  }                                                                                                                                                                                        | 
  else                                                                                                                                                                                     | 
  {                                                                                                                                                                                        | 
    Object[0] = 0LL;                                                                                                                                                                       | 
    result = ObReferenceObjectByHandle(Handle, 0, 0LL, PreviousMode, Object, &HandleInformation);                                                                                          | 
    v20 = result;                                                                                                                                                                          | 
    v21 = (__int64 *)Object[0];                                                                                                                                                            | 
    v65 = Object[0];                                                                                                                                                                       | 
    v56 = result;                                                                                                                                                                          | 
    if ( result < 0 )                                                                                                                                                                      | 
    {                                                                                                                                                                                      | 
      return result;                                                                                                                                                                       | 
    }                                                                                                                                                                                      | 
    GrantedAccess = HandleInformation.GrantedAccess;                                                                                                                                       | 
    v62 = HandleInformation.GrantedAccess;                                                                                                                                                 | 
    v23 = (char *)Object[0] - 48;                                                                                                                                                          | 
    v24 = ObTypeIndexTable[(unsigned __int8)ObHeaderCookie ^ *((unsigned __int8 *)Object[0] - 24) ^ (unsigned __int64)(unsigned __int8)((unsigned __int16)(LOWORD(Object[0]) - 48) >> 8)]; | 
    v67 = v24;                                                                                                                                                                             | 
    v5 = ObjectInformationLength;                                                                                                                                                          | 
  }                                                                                                                                                                                        | 
  v68 = (volatile signed __int64 *)v23;                                                                                                                                                    | 
  if ( ObjectInformationClass == ObjectNameInformation )                                                                                                                                   | 
  {                                                                                                                                                                                        | 
    NameStringMode = ObQueryNameStringMode((_DWORD)v21, (_DWORD)v6, v5, (unsigned int)&v57, PreviousMode);                                                                                 | 
  }                                                                                                                                                                                        | 
  else                                                                                                                                                                                     | 
  {                                                                                                                                                                                        | 
    if ( ObjectInformationClass == ObjectBasicInformation )                                                                                                                                | 
    {                                                                                                                                                                                      | 
      if ( v5 != 56 )                                                                                                                                                                      | 
      {                                                                                                                                                                                    | 
        ObfDereferenceObject(v21);                                                                                                                                                         | 
        return -1073741820;                                                                                                                                                                | 
      }                                                                                                                                                                                    | 
      memset(&v72.m256i_u64[1], 0, 24);                                                                                                                                                    | 
      HandleAttributes = HandleInformation.HandleAttributes;                                                                                                                               | 
      LODWORD(v71) = HandleInformation.HandleAttributes;                                                                                                                                   | 
      v28 = v23[27];                                                                                                                                                                       | 
      if ( (v28 & 0x10) != 0 )                                                                                                                                                             | 
      {                                                                                                                                                                                    | 
        HandleAttributes = HandleInformation.HandleAttributes | 0x10;                                                                                                                      | 
        LODWORD(v71) = HandleInformation.HandleAttributes | 0x10;                                                                                                                          | 
      }                                                                                                                                                                                    | 
      if ( (v28 & 8) != 0 )                                                                                                                                                                | 
      {                                                                                                                                                                                    | 
        LODWORD(v71) = HandleAttributes | 0x20;                                                                                                                                            | 
      }                                                                                                                                                                                    | 
      DWORD1(v71) = GrantedAccess;                                                                                                                                                         | 
      DWORD2(v71) = *((_DWORD *)v23 + 2);                                                                                                                                                  | 
      HIDWORD(v71) = *(_DWORD *)v23;                                                                                                                                                       | 
      v29 = (__int64 *)OBJECT_HEADER_TO_QUOTA_INFO(v23);                                                                                                                                   | 
      if ( v29 )                                                                                                                                                                           | 
      {                                                                                                                                                                                    | 
        v72.m256i_i64[0] = *v29;                                                                                                                                                           | 
      }                                                                                                                                                                                    | 
      else                                                                                                                                                                                 | 
      {                                                                                                                                                                                    | 
        v72.m256i_i64[0] = 0LL;                                                                                                                                                            | 
      }                                                                                                                                                                                    | 
      if ( v31 == ObpSymbolicLinkObjectType )                                                                                                                                              | 
      {                                                                                                                                                                                    | 
        v32 = *v21;                                                                                                                                                                        | 
      }                                                                                                                                                                                    | 
      else                                                                                                                                                                                 | 
      {                                                                                                                                                                                    | 
        v32 = 0LL;                                                                                                                                                                         | 
      }                                                                                                                                                                                    | 
      v73 = v32;                                                                                                                                                                           | 
      CurrentThread = KeGetCurrentThread();                                                                                                                                                | 
      --CurrentThread->KernelApcDisable;                                                                                                                                                   | 
      v34 = (volatile signed __int64 *)(v30 + 16);                                                                                                                                         | 
      v35 = KeAbPreAcquire(v30 + 16, 0LL, 0LL);                                                                                                                                            | 
      if ( _InterlockedCompareExchange64(v34, 17LL, 0LL) )                                                                                                                                 | 
      {                                                                                                                                                                                    | 
        ExfAcquirePushLockSharedEx(v34, 0LL, v35, v34);                                                                                                                                    | 
      }                                                                                                                                                                                    | 
      if ( v35 )                                                                                                                                                                           | 
      {                                                                                                                                                                                    | 
        *(_BYTE *)(v35 + 10) = 1;                                                                                                                                                          | 
      }                                                                                                                                                                                    | 
      v36 = v68;                                                                                                                                                                           | 
      if ( (*((_BYTE *)v68 + 26) & 2) != 0 )                                                                                                                                               | 
      {                                                                                                                                                                                    | 
        v37 = (PVOID *)((char *)v68 - ObpInfoMaskToOffset[*((_BYTE *)v68 + 26) & 3]);                                                                                                      | 
        BugCheckParameter2 = (ULONG_PTR)v37;                                                                                                                                               | 
      }                                                                                                                                                                                    | 
      else                                                                                                                                                                                 | 
      {                                                                                                                                                                                    | 
        v37 = 0LL;                                                                                                                                                                         | 
        BugCheckParameter2 = 0LL;                                                                                                                                                          | 
      }                                                                                                                                                                                    | 
      if ( v37 && (v69 = *v37) != 0LL )                                                                                                                                                    | 
      {                                                                                                                                                                                    | 
        ((void (*)(void))PsReferenceSiloContext)();                                                                                                                                        | 
        v40 = (ULONG_PTR)(v36 + 2);                                                                                                                                                        | 
        if ( _InterlockedCompareExchange64(v36 + 2, 0LL, 17LL) != 17 )                                                                                                                     | 
        {                                                                                                                                                                                  | 
          ((void (*)(void))ExfReleasePushLockShared)();                                                                                                                                    | 
          v40 = (ULONG_PTR)(v36 + 2);                                                                                                                                                      | 
        }                                                                                                                                                                                  | 
        KeAbPostRelease(v40);                                                                                                                                                              | 
        KeLeaveCriticalRegionThread(KeGetCurrentThread());                                                                                                                                 | 
        v49 = (char *)v69;                                                                                                                                                                 | 
        v48 = *(unsigned __int16 *)(BugCheckParameter2 + 8) + 2;                                                                                                                           | 
        while ( 1 )                                                                                                                                                                        | 
        {                                                                                                                                                                                  | 
          v42 = v49;                                                                                                                                                                       | 
          v70 = v49 - 48;                                                                                                                                                                  | 
          v43 = KeGetCurrentThread();                                                                                                                                                      | 
          --v43->KernelApcDisable;                                                                                                                                                         | 
          BugCheckParameter2 = (ULONG_PTR)(v49 - 32);                                                                                                                                      | 
          v44 = KeAbPreAcquire(BugCheckParameter2, 0LL, 0LL);                                                                                                                              | 
          v61 = v44;                                                                                                                                                                       | 
          v45 = (volatile signed __int64 *)BugCheckParameter2;                                                                                                                             | 
          if ( _InterlockedCompareExchange64((volatile signed __int64 *)BugCheckParameter2, 17LL, 0LL) )                                                                                   | 
          {                                                                                                                                                                                | 
            ExfAcquirePushLockSharedEx(BugCheckParameter2, 0LL, v44, BugCheckParameter2);                                                                                                  | 
            v44 = v61;                                                                                                                                                                     | 
            v45 = (volatile signed __int64 *)BugCheckParameter2;                                                                                                                           | 
          }                                                                                                                                                                                | 
          if ( v44 )                                                                                                                                                                       | 
          {                                                                                                                                                                                | 
            *(_BYTE *)(v44 + 10) = 1;                                                                                                                                                      | 
          }                                                                                                                                                                                | 
          v46 = (v70[26] & 2) != 0 ? &v70[-ObpInfoMaskToOffset[v70[26] & 3]] : 0LL;                                                                                                        | 
          if ( !v46 )                                                                                                                                                                      | 
          {                                                                                                                                                                                | 
            break;                                                                                                                                                                         | 
          }                                                                                                                                                                                | 
          v47 = *(char **)v46;                                                                                                                                                             | 
          if ( !*(_QWORD *)v46 )                                                                                                                                                           | 
          {                                                                                                                                                                                | 
            break;                                                                                                                                                                         | 
          }                                                                                                                                                                                | 
          v48 += *((unsigned __int16 *)v46 + 4) + 2;                                                                                                                                       | 
          v49 = *(char **)v46;                                                                                                                                                             | 
          v69 = *(PVOID *)v46;                                                                                                                                                             | 
          PsReferenceSiloContext(v47);                                                                                                                                                     | 
          v50 = BugCheckParameter2;                                                                                                                                                        | 
          if ( _InterlockedCompareExchange64((volatile signed __int64 *)BugCheckParameter2, 0LL, 17LL) != 17 )                                                                             | 
          {                                                                                                                                                                                | 
            ExfReleasePushLockShared(BugCheckParameter2);                                                                                                                                  | 
            v50 = BugCheckParameter2;                                                                                                                                                      | 
          }                                                                                                                                                                                | 
          KeAbPostRelease(v50);                                                                                                                                                            | 
          KeLeaveCriticalRegionThread(KeGetCurrentThread());                                                                                                                               | 
          ObfDereferenceObject(v42);                                                                                                                                                       | 
        }                                                                                                                                                                                  | 
        LODWORD(v61) = v48;                                                                                                                                                                | 
        v51 = _InterlockedCompareExchange64(v45, 0LL, 17LL);                                                                                                                               | 
        v6 = (char *)ObjectInformation;                                                                                                                                                    | 
        v9 = ReturnLength;                                                                                                                                                                 | 
        v36 = v68;                                                                                                                                                                         | 
        if ( v51 != 17 )                                                                                                                                                                   | 
        {                                                                                                                                                                                  | 
          ExfReleasePushLockShared(v45);                                                                                                                                                   | 
          v45 = (volatile signed __int64 *)BugCheckParameter2;                                                                                                                             | 
        }                                                                                                                                                                                  | 
        KeAbPostRelease((ULONG_PTR)v45);                                                                                                                                                   | 
        KeLeaveCriticalRegionThread(KeGetCurrentThread());                                                                                                                                 | 
        if ( v69 )                                                                                                                                                                         | 
        {                                                                                                                                                                                  | 
          ObfDereferenceObject(v69);                                                                                                                                                       | 
        }                                                                                                                                                                                  | 
        v41 = v61 + 18;                                                                                                                                                                    | 
      }                                                                                                                                                                                    | 
      else                                                                                                                                                                                 | 
      {                                                                                                                                                                                    | 
        if ( _InterlockedCompareExchange64(v68 + 2, 0LL, 17LL) != 17 )                                                                                                                     | 
        {                                                                                                                                                                                  | 
          ExfReleasePushLockShared(v36 + 2);                                                                                                                                               | 
        }                                                                                                                                                                                  | 
        KeAbPostRelease((ULONG_PTR)(v36 + 2));                                                                                                                                             | 
        KeLeaveCriticalRegionThread(KeGetCurrentThread());                                                                                                                                 | 
        v41 = 0;                                                                                                                                                                           | 
      }                                                                                                                                                                                    | 
      v72.m256i_i32[5] = v41;                                                                                                                                                              | 
      v72.m256i_i32[6] = *(unsigned __int16 *)(v67 + 16) + 106;                                                                                                                            | 
      if ( (v62 & 0x20000) != 0 && *((_QWORD *)v36 + 5) )                                                                                                                                  | 
      {                                                                                                                                                                                    | 
        v63 = 15;                                                                                                                                                                          | 
        guard_dispatch_icall_no_overrides(v21, 1LL, &v63);                                                                                                                                 | 
      }                                                                                                                                                                                    | 
      v72.m256i_i32[7] = v64;                                                                                                                                                              | 
      *(_OWORD *)v6 = v71;                                                                                                                                                                 | 
      *(__m256i *)(v6 + 16) = v72;                                                                                                                                                         | 
      *((_QWORD *)v6 + 6) = v73;                                                                                                                                                           | 
      v57 = 56;                                                                                                                                                                            | 
      v20 = v56;                                                                                                                                                                           | 
      goto LABEL_29;                                                                                                                                                                       | 
    }                                                                                                                                                                                      | 
    v26 = ObjectInformationClass - 2;                                                                                                                                                      | 
    if ( v26 )                                                                                                                                                                             | 
    {                                                                                                                                                                                      | 
      v38 = v26 - 1;                                                                                                                                                                       | 
      if ( v38 )                                                                                                                                                                           | 
      {                                                                                                                                                                                    | 
        if ( v38 != 1 )                                                                                                                                                                    | 
        {                                                                                                                                                                                  | 
          ObfDereferenceObject(v21);                                                                                                                                                       | 
          return -1073741821;                                                                                                                                                              | 
        }                                                                                                                                                                                  | 
        v57 = 2;                                                                                                                                                                           | 
        if ( v5 < 2 )                                                                                                                                                                      | 
        {                                                                                                                                                                                  | 
          v20 = -1073741820;                                                                                                                                                               | 
        }                                                                                                                                                                                  | 
        else                                                                                                                                                                               | 
        {                                                                                                                                                                                  | 
          *v6 = 0;                                                                                                                                                                         | 
          v39 = HandleInformation.HandleAttributes;                                                                                                                                        | 
          if ( (HandleInformation.HandleAttributes & 2) != 0 )                                                                                                                             | 
          {                                                                                                                                                                                | 
            *v6 = 1;                                                                                                                                                                       | 
          }                                                                                                                                                                                | 
          v6[1] = 0;                                                                                                                                                                       | 
          if ( (v39 & 1) != 0 )                                                                                                                                                            | 
          {                                                                                                                                                                                | 
            v6[1] = 1;                                                                                                                                                                     | 
          }                                                                                                                                                                                | 
        }                                                                                                                                                                                  | 
      }                                                                                                                                                                                    | 
      else                                                                                                                                                                                 | 
      {                                                                                                                                                                                    | 
        v57 = 8;                                                                                                                                                                           | 
        Object[5] = v6;                                                                                                                                                                    | 
        if ( v5 >= 4 )                                                                                                                                                                     | 
        {                                                                                                                                                                                  | 
          *(_DWORD *)v6 = 0;                                                                                                                                                               | 
          v52 = 0;                                                                                                                                                                         | 
          v60 = 0;                                                                                                                                                                         | 
          v53 = 0;                                                                                                                                                                         | 
          while ( v52 < 0x100 )                                                                                                                                                            | 
          {                                                                                                                                                                                | 
            v67 = ObpObjectTypes[v52];                                                                                                                                                     | 
            if ( !v67 )                                                                                                                                                                    | 
            {                                                                                                                                                                              | 
              break;                                                                                                                                                                       | 
            }                                                                                                                                                                              | 
            *(_DWORD *)v6 = ++v53;                                                                                                                                                         | 
            v60 = ++v52;                                                                                                                                                                   | 
          }                                                                                                                                                                                | 
          v60 = 0;                                                                                                                                                                         | 
          while ( v10 < 0x100 )                                                                                                                                                            | 
          {                                                                                                                                                                                | 
            Object[6] = &v6[v57];                                                                                                                                                          | 
            v54 = ObpObjectTypes[v10];                                                                                                                                                     | 
            v67 = v54;                                                                                                                                                                     | 
            if ( !v54 )                                                                                                                                                                    | 
            {                                                                                                                                                                              | 
              break;                                                                                                                                                                       | 
            }                                                                                                                                                                              | 
            v55 = ObQueryTypeInfo(v54, &v6[v57], v5, &v57);                                                                                                                                | 
            v20 = v55;                                                                                                                                                                     | 
            if ( ((v55 + 0x80000000) & 0x80000000) == 0 && v55 != -1073741820 )                                                                                                            | 
            {                                                                                                                                                                              | 
              break;                                                                                                                                                                       | 
            }                                                                                                                                                                              | 
            v60 = ++v10;                                                                                                                                                                   | 
            v5 = ObjectInformationLength;                                                                                                                                                  | 
          }                                                                                                                                                                                | 
        }                                                                                                                                                                                  | 
        else                                                                                                                                                                               | 
        {                                                                                                                                                                                  | 
          v20 = -1073741820;                                                                                                                                                               | 
        }                                                                                                                                                                                  | 
      }                                                                                                                                                                                    | 
      goto LABEL_29;                                                                                                                                                                       | 
    }                                                                                                                                                                                      | 
    NameStringMode = ObQueryTypeInfo(v24, v6, v5, &v57);                                                                                                                                   | 
  }                                                                                                                                                                                        | 
  v20 = NameStringMode;                                                                                                                                                                    | 
LABEL_29:                                                                                                                                                                                  | 
  if ( v9 )                                                                                                                                                                                | 
  {                                                                                                                                                                                        | 
    *v9 = v57;                                                                                                                                                                             | 
  }                                                                                                                                                                                        | 
  if ( v21 )                                                                                                                                                                               | 
  {                                                                                                                                                                                        | 
    ObfDereferenceObject(v21);                                                                                                                                                             | 
  }                                                                                                                                                                                        | 
  return v20;                                                                                                                                                                              | 
}                                                                                                                                                                                          | 
