__int64 __fastcall NtUserRegisterClassExWOW(                                                                           | 
        void *a1,                                                                                                      | 
        ULONG64 a2,                                                                                                    | 
        ULONG64 a3,                                                                                                    | 
        void *a4,                                                                                                      | 
        unsigned __int16 a5,                                                                                           | 
        unsigned int a6)                                                                                               | 
{                                                                                                                      | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                        | 
                                                                                                                       | 
  v56 = 0LL;                                                                                                           | 
  memset_0(&v49, 0, 0x60uLL);                                                                                          | 
  v47 = 0LL;                                                                                                           | 
  v48 = 0LL;                                                                                                           | 
  EnterCrit(0LL, 0LL);                                                                                                 | 
  v10 = PtiCurrent();                                                                                                  | 
  v42 = v10;                                                                                                           | 
  if ( (a6 & 0xFFFFFF3D) != 0 )                                                                                        | 
  {                                                                                                                    | 
    UserSetLastError(1004LL, v9, v10);                                                                                 | 
    v11 = 0;                                                                                                           | 
    goto LABEL_81;                                                                                                     | 
  }                                                                                                                    | 
  v11 = 0;                                                                                                             | 
  if ( !a5 )                                                                                                           | 
  {                                                                                                                    | 
LABEL_8:                                                                                                               | 
    if ( (*(_DWORD *)(*((_QWORD *)v10 + 58) + 12LL) & 0x10000000) == 0 && !(unsigned int)RegisterIconTitleClass() )    | 
    {                                                                                                                  | 
      goto LABEL_81;                                                                                                   | 
    }                                                                                                                  | 
    tagTLBLOCK::_unnamed_type_list_::_unnamed_type_list_((tagTLBLOCK::_unnamed_type_list_ *)BugCheckParameter2);       | 
    tagTLBLOCK::_unnamed_type_list_::_unnamed_type_list_((tagTLBLOCK::_unnamed_type_list_ *)v45);                      | 
    if ( a2 >= MmUserProbeAddress )                                                                                    | 
    {                                                                                                                  | 
      a2 = MmUserProbeAddress;                                                                                         | 
    }                                                                                                                  | 
    v14 = *(_DWORD *)a2;                                                                                               | 
    v37 = *(_DWORD *)a2;                                                                                               | 
    v15 = *(wchar_t **)(a2 + 8);                                                                                       | 
    Str1_8 = v15;                                                                                                      | 
    if ( a3 >= MmUserProbeAddress )                                                                                    | 
    {                                                                                                                  | 
      a3 = MmUserProbeAddress;                                                                                         | 
    }                                                                                                                  | 
    v16 = *(_DWORD *)a3;                                                                                               | 
    v38 = *(_DWORD *)a3;                                                                                               | 
    v17 = *(_QWORD *)(a3 + 8);                                                                                         | 
    v40 = v17;                                                                                                         | 
    v43 = 0LL;                                                                                                         | 
    v44 = 0LL;                                                                                                         | 
    RtlCopyFromUser(&v43, a4, 0x18uLL);                                                                                | 
    v47 = v43;                                                                                                         | 
    v48 = v44;                                                                                                         | 
    v18 = v44;                                                                                                         | 
    if ( v44 >= MmUserProbeAddress )                                                                                   | 
    {                                                                                                                  | 
      v18 = MmUserProbeAddress;                                                                                        | 
    }                                                                                                                  | 
    v36 = *(_DWORD *)v18;                                                                                              | 
    LODWORD(v56) = *(_DWORD *)v18;                                                                                     | 
    v41 = *(_QWORD *)(v18 + 8);                                                                                        | 
    *((_QWORD *)&v56 + 1) = v41;                                                                                       | 
    memset_0(v57, 0, sizeof(v57));                                                                                     | 
    RtlCopyFromUser(v57, a1, 0x50uLL);                                                                                 | 
    v49 = v57[0];                                                                                                      | 
    v50 = v57[1];                                                                                                      | 
    v51 = v57[2];                                                                                                      | 
    v52 = v57[3];                                                                                                      | 
    v53 = v57[4];                                                                                                      | 
    if ( ((unsigned __int64)v15 & 0xFFFFFFFFFFFF0000uLL) != 0 )                                                        | 
    {                                                                                                                  | 
      if ( ((unsigned __int8)v15 & 1) != 0 )                                                                           | 
      {                                                                                                                | 
        ExRaiseDatatypeMisalignment();                                                                                 | 
      }                                                                                                                | 
      v19 = (ULONG64)v15 + (unsigned __int16)v14 + 2;                                                                  | 
      if ( v19 <= (unsigned __int64)v15 || (v20 = (unsigned __int64 *)MmUserProbeAddress, v19 >= MmUserProbeAddress) ) | 
      {                                                                                                                | 
        ExRaiseAccessViolation();                                                                                      | 
      }                                                                                                                | 
      if ( (unsigned __int16)v14 > HIWORD(v37) || (v14 & 1) != 0 )                                                     | 
      {                                                                                                                | 
        if ( (v14 & 1) != 0 )                                                                                          | 
        {                                                                                                              | 
          MicrosoftTelemetryAssertTriggeredArgsKM("IXPTelAssert", 0x20000LL, 10062LL);                                 | 
        }                                                                                                              | 
        ExRaiseAccessViolation();                                                                                      | 
      }                                                                                                                | 
    }                                                                                                                  | 
    else                                                                                                               | 
    {                                                                                                                  | 
      v20 = (unsigned __int64 *)MmUserProbeAddress;                                                                    | 
    }                                                                                                                  | 
    if ( ((unsigned __int64)v15 & 0xFFFFFFFFFFFF0000uLL) != 0 )                                                        | 
    {                                                                                                                  | 
      if ( (unsigned __int16)(v14 + 2) < (unsigned __int16)v14 )                                                       | 
      {                                                                                                                | 
        ExRaiseStatus(-1073741675);                                                                                    | 
      }                                                                                                                | 
      v21 = Win32AllocPoolWithQuotaZInit((unsigned __int16)(v14 + 2), 2020897621LL);                                   | 
      v22 = (void *)v21;                                                                                               | 
      Str1_8 = (wchar_t *)v21;                                                                                         | 
      if ( !v21 )                                                                                                      | 
      {                                                                                                                | 
        ExRaiseStatus(-1073741801);                                                                                    | 
      }                                                                                                                | 
      Win32RawLockedItemBase<unsigned short,&void Win32FreePool(void *),1,1,1>::ManualLock<void>(                      | 
        (ULONG_PTR)BugCheckParameter2,                                                                                 | 
        v21);                                                                                                          | 
      memmove(v22, v15, (unsigned __int16)v14);                                                                        | 
      *((_WORD *)v22 + ((unsigned __int64)(unsigned __int16)v14 >> 1)) = 0;                                            | 
      v20 = (unsigned __int64 *)MmUserProbeAddress;                                                                    | 
    }                                                                                                                  | 
    if ( (v17 & 0xFFFFFFFFFFFF0000uLL) != 0 )                                                                          | 
    {                                                                                                                  | 
      if ( (v17 & 1) != 0 )                                                                                            | 
      {                                                                                                                | 
        ExRaiseDatatypeMisalignment();                                                                                 | 
      }                                                                                                                | 
      v23 = (char *)(v17 + (unsigned __int16)v16 + 2LL);                                                               | 
      if ( (unsigned __int64)v23 <= v17 || (unsigned __int64)v23 >= *v20 )                                             | 
      {                                                                                                                | 
        ExRaiseAccessViolation();                                                                                      | 
      }                                                                                                                | 
      if ( (unsigned __int16)v16 > HIWORD(v38) || (v16 & 1) != 0 )                                                     | 
      {                                                                                                                | 
        if ( (v16 & 1) != 0 )                                                                                          | 
        {                                                                                                              | 
          MicrosoftTelemetryAssertTriggeredArgsKM("IXPTelAssert", 0x20000LL, 10084LL);                                 | 
        }                                                                                                              | 
        ExRaiseAccessViolation();                                                                                      | 
      }                                                                                                                | 
    }                                                                                                                  | 
    if ( (v17 & 0xFFFFFFFFFFFF0000uLL) != 0 )                                                                          | 
    {                                                                                                                  | 
      if ( (unsigned __int16)(v16 + 2) < (unsigned __int16)v16 )                                                       | 
      {                                                                                                                | 
        ExRaiseStatus(-1073741675);                                                                                    | 
      }                                                                                                                | 
      v24 = Win32AllocPoolWithQuotaZInit((unsigned __int16)(v16 + 2), 2020897621LL);                                   | 
      v25 = (void *)v24;                                                                                               | 
      v40 = v24;                                                                                                       | 
      if ( !v24 )                                                                                                      | 
      {                                                                                                                | 
        ExRaiseStatus(-1073741801);                                                                                    | 
      }                                                                                                                | 
      Win32RawLockedItemBase<unsigned short,&void Win32FreePool(void *),1,1,1>::ManualLock<void>((ULONG_PTR)v45, v24); | 
      memmove(v25, (const void *)v17, (unsigned __int16)v16);                                                          | 
      *((_WORD *)v25 + ((unsigned __int64)(unsigned __int16)v16 >> 1)) = 0;                                            | 
      v20 = (unsigned __int64 *)MmUserProbeAddress;                                                                    | 
    }                                                                                                                  | 
    if ( (v41 & 0xFFFFFFFFFFFF0000uLL) != 0 )                                                                          | 
    {                                                                                                                  | 
      if ( (v41 & 1) != 0 )                                                                                            | 
      {                                                                                                                | 
        ExRaiseDatatypeMisalignment();                                                                                 | 
      }                                                                                                                | 
      v26 = v41 + (unsigned __int16)v36 + 2LL;                                                                         | 
      if ( v26 <= v41 || v26 >= *v20 )                                                                                 | 
      {                                                                                                                | 
        ExRaiseAccessViolation();                                                                                      | 
      }                                                                                                                | 
      if ( (unsigned __int16)v36 > HIWORD(v36) || (v36 & 1) != 0 )                                                     | 
      {                                                                                                                | 
        if ( (v36 & 1) != 0 )                                                                                          | 
        {                                                                                                              | 
          MicrosoftTelemetryAssertTriggeredArgsKM("IXPTelAssert", 0x20000LL, 10106LL);                                 | 
        }                                                                                                              | 
        ExRaiseAccessViolation();                                                                                      | 
      }                                                                                                                | 
    }                                                                                                                  | 
    *(_QWORD *)&v53 = Str1_8;                                                                                          | 
    v54 = v40;                                                                                                         | 
    *((_QWORD *)&v52 + 1) = v41;                                                                                       | 
    if ( (*(_DWORD *)(*((_QWORD *)v42 + 58) + 12LL) & 0x2000) == 0 && !(unsigned int)RegisterDefaultClass(Str1_8) )    | 
    {                                                                                                                  | 
LABEL_65:                                                                                                              | 
      Win32RawLockedNtObject<tagDESKTOP>::~Win32RawLockedNtObject<tagDESKTOP>(v45);                                    | 
      Win32RawLockedNtObject<tagDESKTOP>::~Win32RawLockedNtObject<tagDESKTOP>(BugCheckParameter2);                     | 
      goto LABEL_81;                                                                                                   | 
    }                                                                                                                  | 
    if ( (v54 & 0xFFFFFFFFFFFF0000uLL) != 0 )                                                                          | 
    {                                                                                                                  | 
      Atom = UserFindAtom();                                                                                           | 
    }                                                                                                                  | 
    else                                                                                                               | 
    {                                                                                                                  | 
      Atom = v54;                                                                                                      | 
    }                                                                                                                  | 
    v55 = 0LL;                                                                                                         | 
    if ( Atom )                                                                                                        | 
    {                                                                                                                  | 
      v28 = 0;                                                                                                         | 
      v29 = (unsigned __int16 *)&unk_140348CA6;                                                                        | 
      while ( 1 )                                                                                                      | 
      {                                                                                                                | 
        if ( Atom == *(_WORD *)(*(_QWORD *)(W32GetUserSessionState() + 20016)                                          | 
                              + 2LL * ((*(_DWORD *)(v29 - 3) >> 3) & 0x1F)                                             | 
                              + 868) )                                                                                 | 
        {                                                                                                              | 
          v31 = *v29;                                                                                                  | 
          if ( (_WORD)v31 )                                                                                            | 
          {                                                                                                            | 
            break;                                                                                                     | 
          }                                                                                                            | 
        }                                                                                                              | 
        ++v28;                                                                                                         | 
        v29 += 24;                                                                                                     | 
        if ( v28 >= 5 )                                                                                                | 
        {                                                                                                              | 
          goto LABEL_80;                                                                                               | 
        }                                                                                                              | 
      }                                                                                                                | 
      a6 |= v31;                                                                                                       | 
      if ( (a6 & 0x200) != 0 && SDWORD1(v50) < gDefaultServerClasses[12 * v28 + 6] )                                   | 
      {                                                                                                                | 
        UserSetLastError(5LL, gDefaultServerClasses, v30);                                                             | 
        UserSetLastError(0LL, v32, v33);                                                                               | 
        goto LABEL_65;                                                                                                 | 
      }                                                                                                                | 
      v34 = gDefaultServerClasses[12 * v28 + 6];                                                                       | 
      if ( SDWORD1(v50) >= v34 )                                                                                       | 
      {                                                                                                                | 
        LODWORD(v55) = gDefaultServerClasses[12 * v28 + 6];                                                            | 
        DWORD1(v50) -= v34;                                                                                            | 
      }                                                                                                                | 
    }                                                                                                                  | 
LABEL_80:                                                                                                              | 
    v11 = RegisterClassEx(&v49, &v47, a5, a6);                                                                         | 
    goto LABEL_65;                                                                                                     | 
  }                                                                                                                    | 
  if ( (unsigned __int16)(a5 - 666) <= 0x1Eu )                                                                         | 
  {                                                                                                                    | 
    v12 = 0;                                                                                                           | 
    v13 = &unk_140348CA4;                                                                                              | 
    while ( *v13 != a5 )                                                                                               | 
    {                                                                                                                  | 
      ++v12;                                                                                                           | 
      v13 += 24;                                                                                                       | 
      if ( v12 >= 5 )                                                                                                  | 
      {                                                                                                                | 
        goto LABEL_8;                                                                                                  | 
      }                                                                                                                | 
    }                                                                                                                  | 
    UserSetLastError(87LL, a5, v10);                                                                                   | 
  }                                                                                                                    | 
LABEL_81:                                                                                                              | 
  UserSessionSwitchLeaveCrit();                                                                                        | 
  return v11;                                                                                                          | 
}                                                                                                                      | 
251 row(s)
