__int64 NtDisableLastKnownGood()                                                                                                               | 
{                                                                                                                                              | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                                                | 
                                                                                                                                               | 
  Data = 0;                                                                                                                                    | 
  *(_DWORD *)(&ValueName.MaximumLength + 1) = 0;                                                                                               | 
  v5 = 0LL;                                                                                                                                    | 
  KeyHandle = 0LL;                                                                                                                             | 
  if ( KeGetCurrentThread()->PreviousMode == 1 )                                                                                               | 
  {                                                                                                                                            | 
    if ( !SeSinglePrivilegeCheck(SeTcbPrivilege, 1) || (unsigned __int8)PsIsCurrentThreadInServerSilo() )                                      | 
    {                                                                                                                                          | 
      return (unsigned int)-1073741727;                                                                                                        | 
    }                                                                                                                                          | 
    else                                                                                                                                       | 
    {                                                                                                                                          | 
      CachedContextBaseKey = PnpCtxGetCachedContextBaseKey(*(_QWORD *)&PiPnpRtlCtx, 4LL, &v5);                                                 | 
      if ( CachedContextBaseKey >= 0 )                                                                                                         | 
      {                                                                                                                                        | 
        ValueName.Buffer = (wchar_t *)L"Control\\Pnp";                                                                                         | 
        *(_DWORD *)&ValueName.Length = 1572886;                                                                                                | 
        CachedContextBaseKey = ((__int64 (__fastcall *)(HANDLE *, __int64, UNICODE_STRING *, __int64, _DWORD, _QWORD))IopCreateRegistryKeyEx)( | 
                                 &KeyHandle,                                                                                                   | 
                                 v5,                                                                                                           | 
                                 &ValueName,                                                                                                   | 
                                 983103LL,                                                                                                     | 
                                 0,                                                                                                            | 
                                 0LL);                                                                                                         | 
        if ( CachedContextBaseKey >= 0 )                                                                                                       | 
        {                                                                                                                                      | 
          ValueName.Buffer = L"DisableLKG";                                                                                                    | 
          Data = 1;                                                                                                                            | 
          *(_DWORD *)&ValueName.Length = 1441812;                                                                                              | 
          CachedContextBaseKey = ZwSetValueKey(KeyHandle, &ValueName, 0, 4u, &Data, 4u);                                                       | 
        }                                                                                                                                      | 
        if ( KeyHandle )                                                                                                                       | 
        {                                                                                                                                      | 
          ZwClose(KeyHandle);                                                                                                                  | 
        }                                                                                                                                      | 
      }                                                                                                                                        | 
    }                                                                                                                                          | 
  }                                                                                                                                            | 
  else                                                                                                                                         | 
  {                                                                                                                                            | 
    return (unsigned int)-1073741790;                                                                                                          | 
  }                                                                                                                                            | 
  return (unsigned int)CachedContextBaseKey;                                                                                                   | 
}                                                                                                                                              | 
48 row(s)
