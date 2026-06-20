__int64 __fastcall NtMapCMFModule(int a1, unsigned int a2, int *a3, unsigned int *a4, _DWORD *a5, PVOID *a6)         | 
{                                                                                                                    | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                      | 
                                                                                                                     | 
  v6 = a4;                                                                                                           | 
  v7 = a3;                                                                                                           | 
  v8 = a2;                                                                                                           | 
  v52 = 0LL;                                                                                                         | 
  v53 = 0LL;                                                                                                         | 
  *(_QWORD *)&v54 = 0LL;                                                                                             | 
  DWORD2(v54) = 0;                                                                                                   | 
  v44 = 0LL;                                                                                                         | 
  memset(&Event, 0, sizeof(Event));                                                                                  | 
  Handle = 0LL;                                                                                                      | 
  v48 = 0LL;                                                                                                         | 
  v49 = 0LL;                                                                                                         | 
  v50 = 0LL;                                                                                                         | 
  v51 = 0LL;                                                                                                         | 
  MappedBase = 0LL;                                                                                                  | 
  ViewSize = 0LL;                                                                                                    | 
  v37 = 0;                                                                                                           | 
  v40 = 0;                                                                                                           | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                 | 
  v39 = PreviousMode;                                                                                                | 
  if ( !PreviousMode || (_DWORD)InitSafeBootMode )                                                                   | 
  {                                                                                                                  | 
    v36 = -1073741823;                                                                                               | 
    goto LABEL_116;                                                                                                  | 
  }                                                                                                                  | 
  if ( (a1 & 0xFFE0FE81) != 0 )                                                                                      | 
  {                                                                                                                  | 
    goto LABEL_5;                                                                                                    | 
  }                                                                                                                  | 
  v36 = MUIInitializeResourceLock(&CMFLock);                                                                         | 
  if ( (v36 & 0xC0000000) == 0xC0000000 )                                                                            | 
  {                                                                                                                  | 
    goto LABEL_116;                                                                                                  | 
  }                                                                                                                  | 
  if ( (a1 & 0x20000) != 0 )                                                                                         | 
  {                                                                                                                  | 
    if ( (a1 & 0x180000) != 0 && (a1 & 0x40000) != 0 || (a1 & 0x180000) == 0x180000 )                                | 
    {                                                                                                                | 
LABEL_5:                                                                                                             | 
      v36 = -1073741811;                                                                                             | 
      goto LABEL_116;                                                                                                | 
    }                                                                                                                | 
    v11 = a1 & 0x1C0000;                                                                                             | 
    if ( v11 == (CMFFlagsCache & 0x1C0000) )                                                                         | 
    {                                                                                                                | 
      goto LABEL_11;                                                                                                 | 
    }                                                                                                                | 
    CurrentThread = KeGetCurrentThread();                                                                            | 
    --CurrentThread->KernelApcDisable;                                                                               | 
    ExAcquireResourceExclusiveLite(CMFLock, 1u);                                                                     | 
    if ( v11 != (CMFFlagsCache & 0x1C0000) )                                                                         | 
    {                                                                                                                | 
      if ( CMFDirectorySectionPointer )                                                                              | 
      {                                                                                                              | 
        if ( !v11 )                                                                                                  | 
        {                                                                                                            | 
          v36 = -1073741637;                                                                                         | 
LABEL_30:                                                                                                            | 
          ExReleaseResourceLite(CMFLock);                                                                            | 
          KeLeaveCriticalRegion();                                                                                   | 
          goto LABEL_116;                                                                                            | 
        }                                                                                                            | 
        if ( CMFDirectorySectionPointer )                                                                            | 
        {                                                                                                            | 
          v14 = 0x80000000;                                                                                          | 
          v46 = 0x80000000;                                                                                          | 
          if ( (CMFFlagsCache & 0x1C0000) != 0 )                                                                     | 
          {                                                                                                          | 
            v14 = -1073741824;                                                                                       | 
          }                                                                                                          | 
          v46 = v14;                                                                                                 | 
          LOBYTE(v13) = PreviousMode;                                                                                | 
          v36 = CMFCheckAccess(CMFDirectorySectionPointer, v14, v13);                                                | 
          if ( (v36 & 0xC0000000) == 0xC0000000 )                                                                    | 
          {                                                                                                          | 
            goto LABEL_30;                                                                                           | 
          }                                                                                                          | 
        }                                                                                                            | 
      }                                                                                                              | 
      v15 = CMFFlagsCache & 0xFFE3FFFF | v11;                                                                        | 
      CMFFlagsCache = v15;                                                                                           | 
      if ( CMFDirectorySectionPointer && ((v15 & 0x80000) != 0 || (v15 & 0x100000) != 0) )                           | 
      {                                                                                                              | 
        MappedBase = 0LL;                                                                                            | 
        ViewSize = 104LL;                                                                                            | 
        v44 = 0LL;                                                                                                   | 
        if ( MmMapViewInSessionSpace(CMFDirectorySectionPointer, &MappedBase, &ViewSize) >= 0 )                      | 
        {                                                                                                            | 
          v16 = MappedBase;                                                                                          | 
          if ( MappedBase )                                                                                          | 
          {                                                                                                          | 
            if ( ViewSize >= 0x68 )                                                                                  | 
            {                                                                                                        | 
              *((_DWORD *)MappedBase + 12) &= 0xFFE3FFFF;                                                            | 
              v16[12] |= (CMFFlagsCache & 0x80000) != 0 ? 0x80000 : 0x100000;                                        | 
              v16 = MappedBase;                                                                                      | 
            }                                                                                                        | 
            MiRemoveFromSystemSpace((ULONG_PTR)v16);                                                                 | 
            MappedBase = 0LL;                                                                                        | 
          }                                                                                                          | 
        }                                                                                                            | 
        CMFUnmapModules((CMFFlagsCache & 0x100000) != 0 ? 304 : 32);                                                 | 
        CMFRegisterEventTime(4LL);                                                                                   | 
      }                                                                                                              | 
    }                                                                                                                | 
    v36 = 0;                                                                                                         | 
    goto LABEL_30;                                                                                                   | 
  }                                                                                                                  | 
  v17 = KeGetCurrentThread();                                                                                        | 
  --v17->KernelApcDisable;                                                                                           | 
  ExAcquireResourceSharedLite(CMFLock, 1u);                                                                          | 
  if ( !CMFFlagsCache )                                                                                              | 
  {                                                                                                                  | 
    v19 = a1 | 1;                                                                                                    | 
LABEL_48:                                                                                                            | 
    v20 = v19 & 0xFFFFFECF;                                                                                          | 
    v40 = v20;                                                                                                       | 
    v21 = v19 & 0x130;                                                                                               | 
    v41 = (v20 & 0x40) != 0 ? 4 : 2;                                                                                 | 
    v22 = (v20 & 0x40) != 0 ? -1073741824 : 0x80000000;                                                              | 
    v43 = v22;                                                                                                       | 
    v37 = 10000;                                                                                                     | 
    switch ( v21 )                                                                                                   | 
    {                                                                                                                | 
      case 16:                                                                                                       | 
        if ( (unsigned int)CMFCacheIndex < 0x2710 )                                                                  | 
        {                                                                                                            | 
          v37 = CMFCacheIndex;                                                                                       | 
LABEL_56:                                                                                                            | 
          v23 = (v20 & 2) != 0;                                                                                      | 
          v24 = &CMFDirectorySectionPointer;                                                                         | 
LABEL_67:                                                                                                            | 
          v45 = v24;                                                                                                 | 
          v26 = *v24;                                                                                                | 
          goto LABEL_71;                                                                                             | 
        }                                                                                                            | 
        if ( v8 < 0x2710 )                                                                                           | 
        {                                                                                                            | 
          v37 = v8;                                                                                                  | 
          goto LABEL_56;                                                                                             | 
        }                                                                                                            | 
        if ( CMFFirstAccess )                                                                                        | 
        {                                                                                                            | 
          CMFRegisterEventTime(1LL);                                                                                 | 
          CMFFirstAccess = 0;                                                                                        | 
        }                                                                                                            | 
        break;                                                                                                       | 
      case 32:                                                                                                       | 
        if ( v8 < 0x1E && CMFCacheIndex != 10000 )                                                                   | 
        {                                                                                                            | 
          v37 = CMFCacheIndex;                                                                                       | 
          if ( !CMFSegmentSectionPointer )                                                                           | 
          {                                                                                                          | 
            Pool2 = (void *)ExAllocatePool2(0x100uLL, 0xF0uLL, 0x636D6650u);                                         | 
            MappedBase = Pool2;                                                                                      | 
            if ( !Pool2 )                                                                                            | 
            {                                                                                                        | 
              v36 = -1073741801;                                                                                     | 
              goto LABEL_96;                                                                                         | 
            }                                                                                                        | 
            memset_0(Pool2, 0, 0xF0uLL);                                                                             | 
            if ( _InterlockedCompareExchange64(                                                                      | 
                   (volatile signed __int64 *)&CMFSegmentSectionPointer,                                             | 
                   (signed __int64)MappedBase,                                                                       | 
                   0LL) )                                                                                            | 
            {                                                                                                        | 
              ExFreePoolWithTag(MappedBase, 0);                                                                      | 
            }                                                                                                        | 
            MappedBase = 0LL;                                                                                        | 
            v20 = v40;                                                                                               | 
          }                                                                                                          | 
          v23 = (v20 & 4) != 0;                                                                                      | 
          v24 = (PVOID *)((char *)CMFSegmentSectionPointer + 8 * v8);                                                | 
          goto LABEL_67;                                                                                             | 
        }                                                                                                            | 
        break;                                                                                                       | 
      case 256:                                                                                                      | 
        v37 = CMFCacheIndex;                                                                                         | 
        v24 = &CMFHitsSectionPointer;                                                                                | 
        v45 = &CMFHitsSectionPointer;                                                                                | 
        if ( CMFHitsSectionPointer )                                                                                 | 
        {                                                                                                            | 
          goto LABEL_85;                                                                                             | 
        }                                                                                                            | 
        CMFHitsLastFlushTime = MEMORY[0xFFFFF78000000014];                                                           | 
        v26 = 0LL;                                                                                                   | 
        v23 = (v20 & 8) != 0;                                                                                        | 
        v20 = v40;                                                                                                   | 
        v24 = v45;                                                                                                   | 
LABEL_71:                                                                                                            | 
        if ( !v26 )                                                                                                  | 
        {                                                                                                            | 
          ExReleaseResourceLite(CMFLock);                                                                            | 
          ExAcquireResourceExclusiveLite(CMFLock, 1u);                                                               | 
          if ( ((CMFFlagsCache ^ v20) & 0x1C0000) != 0 && (CMFFlagsCache & 0x180000) != 0 )                          | 
          {                                                                                                          | 
            v36 = -1073741823;                                                                                       | 
            goto LABEL_96;                                                                                           | 
          }                                                                                                          | 
          v22 = v43;                                                                                                 | 
          if ( *v24 )                                                                                                | 
          {                                                                                                          | 
            v27 = v41;                                                                                               | 
LABEL_84:                                                                                                            | 
            ExConvertExclusiveToSharedLite(CMFLock);                                                                 | 
LABEL_86:                                                                                                            | 
            MappedBase = 0LL;                                                                                        | 
            ViewSize = 0LL;                                                                                          | 
            v44 = 0LL;                                                                                               | 
            if ( a6 )                                                                                                | 
            {                                                                                                        | 
              if ( v21 == 256                                                                                        | 
                || (LOBYTE(v18) = v39, v36 = CMFCheckAccess(*v24, v22, v18), (v36 & 0xC0000000) != 0xC0000000) )     | 
              {                                                                                                      | 
                v57 = KeGetCurrentThread();                                                                          | 
                v36 = MmMapViewOfSection(                                                                            | 
                        (unsigned int)*v45,                                                                          | 
                        v57->ApcState.Process,                                                                       | 
                        (unsigned int)&MappedBase,                                                                   | 
                        0,                                                                                           | 
                        0LL,                                                                                         | 
                        (__int64)&v44,                                                                               | 
                        (__int64)&ViewSize,                                                                          | 
                        1,                                                                                           | 
                        0x400000,                                                                                    | 
                        v27);                                                                                        | 
                if ( (v36 & 0xC0000000) == 0xC0000000 )                                                              | 
                {                                                                                                    | 
                  MappedBase = 0LL;                                                                                  | 
                }                                                                                                    | 
                else if ( v21 == 256 )                                                                               | 
                {                                                                                                    | 
                  v29 = CMFHitsLastFlushTime;                                                                        | 
                  v30 = MEMORY[0xFFFFF78000000014];                                                                  | 
                  if ( MEMORY[0xFFFFF78000000014] > CMFHitsLastFlushTime + 36000000000LL                             | 
                    && v29 == _InterlockedCompareExchange64(                                                         | 
                                &CMFHitsLastFlushTime,                                                               | 
                                MEMORY[0xFFFFF78000000014],                                                          | 
                                CMFHitsLastFlushTime)                                                                | 
                    && (int)CMFFlushHitsFile(MappedBase, ViewSize) < 0 )                                             | 
                  {                                                                                                  | 
                    _InterlockedCompareExchange64(&CMFHitsLastFlushTime, v29, v30);                                  | 
                  }                                                                                                  | 
                }                                                                                                    | 
              }                                                                                                      | 
            }                                                                                                        | 
            goto LABEL_96;                                                                                           | 
          }                                                                                                          | 
          LODWORD(v48) = 0x8000000;                                                                                  | 
          *(_QWORD *)((char *)&v48 + 4) = __PAIR64__(v43, v37);                                                      | 
          BYTE12(v48) = v23;                                                                                         | 
          *((_QWORD *)&v49 + 1) = __PAIR64__(a2, v20);                                                               | 
          v27 = v41;                                                                                                 | 
          LODWORD(v50) = v41;                                                                                        | 
          *((_QWORD *)&v50 + 1) = v24;                                                                               | 
          LODWORD(v51) = v21;                                                                                        | 
          KeInitializeEvent(&Event, SynchronizationEvent, 0);                                                        | 
          *(_QWORD *)&v49 = &Event;                                                                                  | 
          LODWORD(v52) = 48;                                                                                         | 
          *((_QWORD *)&v52 + 1) = 0LL;                                                                               | 
          DWORD2(v53) = 512;                                                                                         | 
          *(_QWORD *)&v53 = 0LL;                                                                                     | 
          v54 = 0LL;                                                                                                 | 
          v36 = PsCreateSystemThreadEx(&Handle, 0x1FFFFFLL, &v52, 0LL, 0LL, CMFSystemThreadRoutine, &v48, 0LL, 0LL); | 
          if ( (v36 & 0xC0000000) != 0xC0000000 )                                                                    | 
          {                                                                                                          | 
            ZwClose(Handle);                                                                                         | 
            v28 = KeWaitForSingleObject(&Event, Executive, 0, 0, 0LL);                                               | 
            v36 = v28;                                                                                               | 
            if ( v28 )                                                                                               | 
            {                                                                                                        | 
              if ( v28 >= 0 )                                                                                        | 
              {                                                                                                      | 
                v36 = -1073741823;                                                                                   | 
              }                                                                                                      | 
              goto LABEL_96;                                                                                         | 
            }                                                                                                        | 
            v40 = DWORD2(v49);                                                                                       | 
            v36 = DWORD1(v50);                                                                                       | 
            if ( (DWORD1(v50) & 0xC0000000) != 0xC0000000 )                                                          | 
            {                                                                                                        | 
              if ( v21 == 16 )                                                                                       | 
              {                                                                                                      | 
                CMFRegisterEventTime(2LL);                                                                           | 
              }                                                                                                      | 
              goto LABEL_84;                                                                                         | 
            }                                                                                                        | 
          }                                                                                                          | 
LABEL_96:                                                                                                            | 
          v7 = a3;                                                                                                   | 
          goto LABEL_97;                                                                                             | 
        }                                                                                                            | 
LABEL_85:                                                                                                            | 
        v27 = v41;                                                                                                   | 
        goto LABEL_86;                                                                                               | 
    }                                                                                                                | 
    v36 = -1073741811;                                                                                               | 
    goto LABEL_96;                                                                                                   | 
  }                                                                                                                  | 
  if ( (CMFFlagsCache & 0xF) != 0 )                                                                                  | 
  {                                                                                                                  | 
    v19 = CMFFlagsCache & 0xF | a1 & 0xFFFFFFF0;                                                                     | 
  }                                                                                                                  | 
  else                                                                                                               | 
  {                                                                                                                  | 
    v19 = a1 | 1;                                                                                                    | 
  }                                                                                                                  | 
  if ( (CMFFlagsCache & 0x100000) != 0 )                                                                             | 
  {                                                                                                                  | 
    if ( (v19 & 0x10000) == 0 )                                                                                      | 
    {                                                                                                                | 
      v36 = -1073741672;                                                                                             | 
      goto LABEL_98;                                                                                                 | 
    }                                                                                                                | 
    v19 |= 0x100000u;                                                                                                | 
    goto LABEL_46;                                                                                                   | 
  }                                                                                                                  | 
  if ( (CMFFlagsCache & 0x80000) == 0 )                                                                              | 
  {                                                                                                                  | 
    if ( (CMFFlagsCache & 0x40000) != 0 )                                                                            | 
    {                                                                                                                | 
      v19 |= 0x40000u;                                                                                               | 
    }                                                                                                                | 
    goto LABEL_46;                                                                                                   | 
  }                                                                                                                  | 
  if ( (v19 & 0x130) != 0x20 || (v19 & 0x10000) != 0 )                                                               | 
  {                                                                                                                  | 
    v19 |= 0x80000u;                                                                                                 | 
LABEL_46:                                                                                                            | 
    v8 = a2;                                                                                                         | 
    goto LABEL_48;                                                                                                   | 
  }                                                                                                                  | 
  v36 = -1073741672;                                                                                                 | 
LABEL_97:                                                                                                            | 
  v6 = a4;                                                                                                           | 
LABEL_98:                                                                                                            | 
  ExReleaseResourceLite(CMFLock);                                                                                    | 
  KeLeaveCriticalRegion();                                                                                           | 
  if ( (v36 & 0xC0000000) != 0xC0000000 )                                                                            | 
  {                                                                                                                  | 
    v31 = 0x7FFFFFFF0000LL;                                                                                          | 
    if ( a6 )                                                                                                        | 
    {                                                                                                                | 
      v32 = 0x7FFFFFFF0000LL;                                                                                        | 
      if ( (unsigned __int64)a6 < 0x7FFFFFFF0000LL )                                                                 | 
      {                                                                                                              | 
        v32 = (__int64)a6;                                                                                           | 
      }                                                                                                              | 
      *(_QWORD *)v32 = *(_QWORD *)v32;                                                                               | 
      *a6 = MappedBase;                                                                                              | 
      MappedBase = 0LL;                                                                                              | 
    }                                                                                                                | 
    if ( a5 )                                                                                                        | 
    {                                                                                                                | 
      v33 = 0x7FFFFFFF0000LL;                                                                                        | 
      if ( (unsigned __int64)a5 < 0x7FFFFFFF0000LL )                                                                 | 
      {                                                                                                              | 
        v33 = (__int64)a5;                                                                                           | 
      }                                                                                                              | 
      *(_DWORD *)v33 = *(_DWORD *)v33;                                                                               | 
      *a5 = ViewSize;                                                                                                | 
    }                                                                                                                | 
    if ( v7 )                                                                                                        | 
    {                                                                                                                | 
      v34 = 0x7FFFFFFF0000LL;                                                                                        | 
      if ( (unsigned __int64)v7 < 0x7FFFFFFF0000LL )                                                                 | 
      {                                                                                                              | 
        v34 = (__int64)v7;                                                                                           | 
      }                                                                                                              | 
      *(_DWORD *)v34 = *(_DWORD *)v34;                                                                               | 
      *v7 = v37;                                                                                                     | 
    }                                                                                                                | 
    if ( v6 )                                                                                                        | 
    {                                                                                                                | 
      if ( (unsigned __int64)v6 < 0x7FFFFFFF0000LL )                                                                 | 
      {                                                                                                              | 
        v31 = (__int64)v6;                                                                                           | 
      }                                                                                                              | 
      *(_DWORD *)v31 = *(_DWORD *)v31;                                                                               | 
      *v6 = v40;                                                                                                     | 
    }                                                                                                                | 
LABEL_11:                                                                                                            | 
    v36 = 0;                                                                                                         | 
  }                                                                                                                  | 
LABEL_116:                                                                                                           | 
  if ( MappedBase )                                                                                                  | 
  {                                                                                                                  | 
    MiUnmapViewOfSection(KeGetCurrentThread()->ApcState.Process);                                                    | 
  }                                                                                                                  | 
  return v36;                                                                                                        | 
}                                                                                                                    | 
395 row(s)
