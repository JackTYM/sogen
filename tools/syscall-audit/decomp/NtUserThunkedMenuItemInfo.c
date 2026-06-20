__int64 __fastcall NtUserThunkedMenuItemInfo(__int64 a1, int a2, int a3, int a4, ULONG64 a5, ULONG64 a6)          | 
{                                                                                                                 | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                   | 
                                                                                                                  | 
  memset_0(v26, 0, sizeof(v26));                                                                                  | 
  DestinationString = 0LL;                                                                                        | 
  EnterCrit(0LL, 0LL);                                                                                            | 
  SmartObjStackRef<tagMENU>::SmartObjStackRef<tagMENU>(v23);                                                      | 
  v10 = (_OWORD *)a5;                                                                                             | 
  if ( a5 >= MmUserProbeAddress )                                                                                 | 
  {                                                                                                               | 
    v10 = (_OWORD *)MmUserProbeAddress;                                                                           | 
  }                                                                                                               | 
  v26[0] = *v10;                                                                                                  | 
  v26[1] = v10[1];                                                                                                | 
  v26[2] = v10[2];                                                                                                | 
  v26[3] = v10[3];                                                                                                | 
  v26[4] = v10[4];                                                                                                | 
  v11 = (unsigned int *)a6;                                                                                       | 
  v12 = 0;                                                                                                        | 
  if ( a6 )                                                                                                       | 
  {                                                                                                               | 
    if ( a6 >= MmUserProbeAddress )                                                                               | 
    {                                                                                                             | 
      v11 = (unsigned int *)MmUserProbeAddress;                                                                   | 
    }                                                                                                             | 
    v13 = *v11;                                                                                                   | 
    v22 = *v11;                                                                                                   | 
    *(_DWORD *)&DestinationString.Length = *v11;                                                                  | 
    v14 = (WCHAR *)*((_QWORD *)v11 + 1);                                                                          | 
    DestinationString.Buffer = v14;                                                                               | 
    if ( ((unsigned __int8)v14 & 1) != 0 )                                                                        | 
    {                                                                                                             | 
      ExRaiseDatatypeMisalignment();                                                                              | 
    }                                                                                                             | 
    v15 = (ULONG64)v14 + (unsigned __int16)v13 + 2;                                                               | 
    if ( v15 <= (unsigned __int64)v14 || v15 >= MmUserProbeAddress )                                              | 
    {                                                                                                             | 
      ExRaiseAccessViolation();                                                                                   | 
    }                                                                                                             | 
    if ( (unsigned __int16)v13 > HIWORD(v22) || (v13 & 1) != 0 )                                                  | 
    {                                                                                                             | 
      if ( (v13 & 1) != 0 )                                                                                       | 
      {                                                                                                           | 
        MicrosoftTelemetryAssertTriggeredArgsKM("IXPTelAssert", 0x20000LL, 4830LL);                               | 
      }                                                                                                           | 
      ExRaiseAccessViolation();                                                                                   | 
    }                                                                                                             | 
  }                                                                                                               | 
  else                                                                                                            | 
  {                                                                                                               | 
    RtlInitUnicodeString(&DestinationString, 0LL);                                                                | 
  }                                                                                                               | 
  if ( (BYTE4(v26[0]) & 1) == 0 || (HIDWORD(v26[0]) & 0xFFFFEF74) == 0 )                                          | 
  {                                                                                                               | 
    if ( a4 )                                                                                                     | 
    {                                                                                                             | 
      v16 = ValidateHmenu(a1, 16LL);                                                                              | 
      v24 = 0LL;                                                                                                  | 
      SmartObjStackRefBase<tagMENU>::operator=(v23, v16);                                                         | 
      if ( (unsigned __int8)SmartObjStackRef<tagMENU>::operator==(v23)                                            | 
        || (*(_DWORD *)(*(_QWORD *)(*(_QWORD *)v23[0] + 40LL) + 40LL) & 0x40) != 0 )                              | 
      {                                                                                                           | 
        goto LABEL_19;                                                                                            | 
      }                                                                                                           | 
      v17 = (*(_DWORD *)(*(_QWORD *)(*(_QWORD *)v23[0] + 40LL) + 40LL) & 0x200) == 0;                             | 
    }                                                                                                             | 
    else                                                                                                          | 
    {                                                                                                             | 
      v19 = AccessMaskFromMenuItemInfo(v13, DWORD1(v26[0]), HIDWORD(v26[0]));                                     | 
      v20 = ValidateHmenu(a1, v19);                                                                               | 
      v24 = 0LL;                                                                                                  | 
      SmartObjStackRefBase<tagMENU>::operator=(v23, v20);                                                         | 
      if ( (unsigned __int8)SmartObjStackRef<tagMENU>::operator==(v23) )                                          | 
      {                                                                                                           | 
        goto LABEL_19;                                                                                            | 
      }                                                                                                           | 
      v17 = (*(_DWORD *)(*(_QWORD *)(*(_QWORD *)v23[0] + 40LL) + 40LL) & 0x40) == 0;                              | 
    }                                                                                                             | 
    if ( v17 )                                                                                                    | 
    {                                                                                                             | 
      Win32HMThreadLock<tagMENU>::Win32HMThreadLock<tagMENU>(BugCheckParameter3, v23);                            | 
      if ( a4 )                                                                                                   | 
      {                                                                                                           | 
        inserted = xxxInsertMenuItem((unsigned int)v23, a2, a3, (unsigned int)v26, (__int64)&DestinationString);  | 
      }                                                                                                           | 
      else                                                                                                        | 
      {                                                                                                           | 
        inserted = xxxSetMenuItemInfo((unsigned int)v23, a2, a3, (unsigned int)v26, (__int64)&DestinationString); | 
      }                                                                                                           | 
      v12 = inserted;                                                                                             | 
      Win32HMThreadLock<tagCURSOR>::~Win32HMThreadLock<tagCURSOR>((ULONG_PTR)BugCheckParameter3);                 | 
    }                                                                                                             | 
  }                                                                                                               | 
LABEL_19:                                                                                                         | 
  SmartObjStackRef<tagMENU>::~SmartObjStackRef<tagMENU>(v23);                                                     | 
  UserSessionSwitchLeaveCrit();                                                                                   | 
  return v12;                                                                                                     | 
}                                                                                                                 | 
99 row(s)
