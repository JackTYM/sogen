__int64 NtEnableLastKnownGood()                                                                                          | 
{                                                                                                                        | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                          | 
                                                                                                                         | 
  v17 = 0LL;                                                                                                             | 
  KeyHandle = 0LL;                                                                                                       | 
  Handle = 0LL;                                                                                                          | 
  v16 = 0LL;                                                                                                             | 
  P = 0LL;                                                                                                               | 
  DestinationString = 0LL;                                                                                               | 
  SourceString = 0LL;                                                                                                    | 
  v13 = 0LL;                                                                                                             | 
  v11 = 0LL;                                                                                                             | 
  memset(&ObjectAttributes, 0, 44);                                                                                      | 
  ValueName = 0LL;                                                                                                       | 
  if ( KeGetCurrentThread()->PreviousMode == 1 )                                                                         | 
  {                                                                                                                      | 
    if ( !SeSinglePrivilegeCheck(SeTcbPrivilege, 1)                                                                      | 
      || (unsigned __int8)PsIsCurrentThreadInServerSilo(                                                                 | 
                            v2,                                                                                          | 
                            v1,                                                                                          | 
                            v3,                                                                                          | 
                            v4,                                                                                          | 
                            *(_QWORD *)&ObjectAttributes.Length,                                                         | 
                            ObjectAttributes.RootDirectory,                                                              | 
                            ObjectAttributes.ObjectName,                                                                 | 
                            *(_QWORD *)&ObjectAttributes.Attributes) )                                                   | 
    {                                                                                                                    | 
      CachedContextBaseKey = -1073741727;                                                                                | 
    }                                                                                                                    | 
    else                                                                                                                 | 
    {                                                                                                                    | 
      CachedContextBaseKey = PnpCtxGetCachedContextBaseKey(*(_QWORD *)&PiPnpRtlCtx, 4LL, &v17);                          | 
      if ( CachedContextBaseKey >= 0 )                                                                                   | 
      {                                                                                                                  | 
        ValueName.Buffer = (wchar_t *)L"Control\\Pnp";                                                                   | 
        *(_DWORD *)&ValueName.Length = 1572886;                                                                          | 
        CachedContextBaseKey = IopOpenRegistryKeyEx(&KeyHandle, v17, &ValueName, 983103LL);                              | 
        if ( CachedContextBaseKey >= 0 )                                                                                 | 
        {                                                                                                                | 
          v5 = 0;                                                                                                        | 
          if ( (int)IopGetRegistryValue(KeyHandle) < 0 )                                                                 | 
          {                                                                                                              | 
            goto LABEL_19;                                                                                               | 
          }                                                                                                              | 
          if ( *((_DWORD *)P + 1) == 4 && *((_DWORD *)P + 3) == 4 )                                                      | 
          {                                                                                                              | 
            v5 = *((_BYTE *)P + *((unsigned int *)P + 2));                                                               | 
          }                                                                                                              | 
          ExFreePoolWithTag(P, 0);                                                                                       | 
          if ( v5 )                                                                                                      | 
          {                                                                                                              | 
            *(_DWORD *)&ValueName.Length = 1441812;                                                                      | 
            ValueName.Buffer = L"DisableLKG";                                                                            | 
            CachedContextBaseKey = ZwDeleteValueKey(KeyHandle, &ValueName);                                              | 
            if ( CachedContextBaseKey >= 0 )                                                                             | 
            {                                                                                                            | 
              RtlInitUnicodeString(&DestinationString, L"\\Registry\\Machine\\System\\LastKnownGoodRecovery\\LastGood"); | 
              ObjectAttributes.RootDirectory = 0LL;                                                                      | 
              ObjectAttributes.ObjectName = &DestinationString;                                                          | 
              ObjectAttributes.Length = 48;                                                                              | 
              ObjectAttributes.Attributes = 576;                                                                         | 
              *(_OWORD *)&ObjectAttributes.SecurityDescriptor = 0LL;                                                     | 
              if ( ZwOpenKey(&Handle, 0xF003Fu, &ObjectAttributes) >= 0 )                                                | 
              {                                                                                                          | 
                ZwDeleteKey(Handle);                                                                                     | 
              }                                                                                                          | 
              RtlInitUnicodeString(&v13, L"\\Registry\\Machine\\System\\LastKnownGoodRecovery\\LastGood.Tmp");           | 
              ObjectAttributes.RootDirectory = 0LL;                                                                      | 
              ObjectAttributes.ObjectName = &v13;                                                                        | 
              ObjectAttributes.Length = 48;                                                                              | 
              ObjectAttributes.Attributes = 576;                                                                         | 
              *(_OWORD *)&ObjectAttributes.SecurityDescriptor = 0LL;                                                     | 
              if ( ZwOpenKey(&v16, 0xF003Fu, &ObjectAttributes) >= 0 )                                                   | 
              {                                                                                                          | 
                ZwDeleteKey(v16);                                                                                        | 
              }                                                                                                          | 
              RtlInitUnicodeString(&SourceString, L"\\SystemRoot\\LastGood");                                            | 
              IopFileUtilWalkDirectoryTreeBottomUp(&SourceString);                                                       | 
              ObjectAttributes.RootDirectory = 0LL;                                                                      | 
              ObjectAttributes.ObjectName = &SourceString;                                                               | 
              ObjectAttributes.Length = 48;                                                                              | 
              ObjectAttributes.Attributes = 64;                                                                          | 
              *(_OWORD *)&ObjectAttributes.SecurityDescriptor = 0LL;                                                     | 
              ZwDeleteFile(&ObjectAttributes);                                                                           | 
              RtlInitUnicodeString(&v11, L"\\SystemRoot\\LastGood.Tmp");                                                 | 
              IopFileUtilWalkDirectoryTreeBottomUp(&v11);                                                                | 
              ObjectAttributes.RootDirectory = 0LL;                                                                      | 
              ObjectAttributes.ObjectName = &v11;                                                                        | 
              ObjectAttributes.Length = 48;                                                                              | 
              *(_OWORD *)&ObjectAttributes.SecurityDescriptor = 0LL;                                                     | 
              ObjectAttributes.Attributes = 64;                                                                          | 
              ZwDeleteFile(&ObjectAttributes);                                                                           | 
              CachedContextBaseKey = 0;                                                                                  | 
            }                                                                                                            | 
          }                                                                                                              | 
          else                                                                                                           | 
          {                                                                                                              | 
LABEL_19:                                                                                                                | 
            CachedContextBaseKey = -1073741823;                                                                          | 
          }                                                                                                              | 
        }                                                                                                                | 
      }                                                                                                                  | 
    }                                                                                                                    | 
    if ( Handle )                                                                                                        | 
    {                                                                                                                    | 
      ZwClose(Handle);                                                                                                   | 
    }                                                                                                                    | 
    if ( v16 )                                                                                                           | 
    {                                                                                                                    | 
      ZwClose(v16);                                                                                                      | 
    }                                                                                                                    | 
    if ( KeyHandle )                                                                                                     | 
    {                                                                                                                    | 
      ZwClose(KeyHandle);                                                                                                | 
    }                                                                                                                    | 
  }                                                                                                                      | 
  else                                                                                                                   | 
  {                                                                                                                      | 
    return (unsigned int)-1073741790;                                                                                    | 
  }                                                                                                                      | 
  return (unsigned int)CachedContextBaseKey;                                                                             | 
}                                                                                                                        | 
123 row(s)
