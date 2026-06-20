__int64 __fastcall NtPlugPlayControl(unsigned int a1, void *a2, unsigned int a3)                                       | 
{                                                                                                                      | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                        | 
                                                                                                                       | 
  v4 = a3;                                                                                                             | 
  v5 = a1;                                                                                                             | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                   | 
  if ( PreviousMode && !SeSinglePrivilegeCheck(SeTcbPrivilege, 1) )                                                    | 
  {                                                                                                                    | 
    return 3221225569LL;                                                                                               | 
  }                                                                                                                    | 
  if ( (unsigned int)v5 >= 0x18 )                                                                                      | 
  {                                                                                                                    | 
    return 3221225711LL;                                                                                               | 
  }                                                                                                                    | 
  v8 = &PlugPlayHandlerTable[3 * v5];                                                                                  | 
  if ( *(_DWORD *)v8 != (_DWORD)v5 )                                                                                   | 
  {                                                                                                                    | 
    return 3221225701LL;                                                                                               | 
  }                                                                                                                    | 
  if ( !v8 )                                                                                                           | 
  {                                                                                                                    | 
    return 3221225711LL;                                                                                               | 
  }                                                                                                                    | 
  if ( !v8[1] )                                                                                                        | 
  {                                                                                                                    | 
    return 3221225474LL;                                                                                               | 
  }                                                                                                                    | 
  if ( *((_DWORD *)v8 + 1) != (_DWORD)v4 )                                                                             | 
  {                                                                                                                    | 
    return 3221225520LL;                                                                                               | 
  }                                                                                                                    | 
  if ( (unsigned __int8)PsIsCurrentThreadInServerSilo() && !*((_BYTE *)v8 + 16) )                                      | 
  {                                                                                                                    | 
    return 3221225569LL;                                                                                               | 
  }                                                                                                                    | 
  if ( PreviousMode )                                                                                                  | 
  {                                                                                                                    | 
    if ( (_DWORD)v4 )                                                                                                  | 
    {                                                                                                                  | 
      Pool2 = (void *)ExAllocatePool2(0x101uLL, v4, 0x20207050u);                                                      | 
      v9 = Pool2;                                                                                                      | 
      if ( !Pool2 )                                                                                                    | 
      {                                                                                                                | 
        return 3221225626LL;                                                                                           | 
      }                                                                                                                | 
      v15 = PiControlCopyUserModeCallersBuffer(Pool2, PreviousMode, 1);                                                | 
      if ( v15 < 0 )                                                                                                   | 
      {                                                                                                                | 
        ExFreePoolWithTag(v9, 0);                                                                                      | 
        return (unsigned int)v15;                                                                                      | 
      }                                                                                                                | 
    }                                                                                                                  | 
    else                                                                                                               | 
    {                                                                                                                  | 
      v9 = 0LL;                                                                                                        | 
    }                                                                                                                  | 
  }                                                                                                                    | 
  else                                                                                                                 | 
  {                                                                                                                    | 
    v9 = a2;                                                                                                           | 
  }                                                                                                                    | 
  v10 = (__int64 (__fastcall *)(_QWORD, _QWORD, _QWORD, _QWORD))v8[1];                                                 | 
  if ( v10 == PiControlGetDeviceInterfaceEnabled )                                                                     | 
  {                                                                                                                    | 
    DeviceInterfaceEnabled = PiControlGetDeviceInterfaceEnabled((unsigned int)v5, v9, (unsigned int)v4, PreviousMode); | 
  }                                                                                                                    | 
  else if ( v10 == PiControlGetSetDeviceStatus )                                                                       | 
  {                                                                                                                    | 
    DeviceInterfaceEnabled = PiControlGetSetDeviceStatus((unsigned int)v5, v9, (unsigned int)v4, PreviousMode);        | 
  }                                                                                                                    | 
  else                                                                                                                 | 
  {                                                                                                                    | 
    DeviceInterfaceEnabled = guard_dispatch_icall_no_overrides((unsigned int)v5, v9, (unsigned int)v4, PreviousMode);  | 
  }                                                                                                                    | 
  v12 = DeviceInterfaceEnabled;                                                                                        | 
  if ( (DeviceInterfaceEnabled & 0xC0000000) != 0xC0000000 || DeviceInterfaceEnabled == -1073741789 )                  | 
  {                                                                                                                    | 
    if ( PreviousMode )                                                                                                | 
    {                                                                                                                  | 
      if ( (_DWORD)v4 )                                                                                                | 
      {                                                                                                                | 
        if ( a2 )                                                                                                      | 
        {                                                                                                              | 
          v13 = PiControlCopyUserModeCallersBuffer(a2, PreviousMode, 0);                                               | 
          if ( v13 < 0 )                                                                                               | 
          {                                                                                                            | 
            v12 = v13;                                                                                                 | 
          }                                                                                                            | 
        }                                                                                                              | 
      }                                                                                                                | 
    }                                                                                                                  | 
  }                                                                                                                    | 
  if ( v12 == -1073741738 )                                                                                            | 
  {                                                                                                                    | 
    v12 = -1073741810;                                                                                                 | 
  }                                                                                                                    | 
  if ( PreviousMode )                                                                                                  | 
  {                                                                                                                    | 
    if ( v9 )                                                                                                          | 
    {                                                                                                                  | 
      ExFreePoolWithTag(v9, 0);                                                                                        | 
    }                                                                                                                  | 
  }                                                                                                                    | 
  return v12;                                                                                                          | 
}                                                                                                                      | 
106 row(s)
