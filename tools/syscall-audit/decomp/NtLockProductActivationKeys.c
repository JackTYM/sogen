NTSTATUS __fastcall NtLockProductActivationKeys(__int64 a1, _DWORD *a2)                                      | 
{                                                                                                            | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                              | 
                                                                                                             | 
  DestinationString = 0LL;                                                                                   | 
  v3 = 0;                                                                                                    | 
  *(&ObjectAttributes.Length + 1) = 0;                                                                       | 
  *(&ObjectAttributes.Attributes + 1) = 0;                                                                   | 
  KeyHandle = 0LL;                                                                                           | 
  ResultLength = 0;                                                                                          | 
  *(_OWORD *)SourceString = xmmword_14001A290;                                                               | 
  v30 = xmmword_14001A2A0;                                                                                   | 
  v31 = xmmword_14001A2B0;                                                                                   | 
  v32 = 0x2C7EFB57828734DALL;                                                                                | 
  v33 = 593434642;                                                                                           | 
  v4 = -21647;                                                                                               | 
  for ( i = 0LL; i < 0x1E; ++i )                                                                             | 
  {                                                                                                          | 
    v6 = SourceString[i] ^ (v4 + SourceString[i]);                                                           | 
    v4 = v6;                                                                                                 | 
  }                                                                                                          | 
  if ( v6 != -18097 )                                                                                        | 
  {                                                                                                          | 
    return -1073741823;                                                                                      | 
  }                                                                                                          | 
  for ( j = 52LL; j >= 0; --j )                                                                              | 
  {                                                                                                          | 
    v8 = *(_DWORD *)((char *)SourceString + j);                                                              | 
    v9 = *(_DWORD *)((char *)&SourceString[2] + j);                                                          | 
    v10 = -957401312;                                                                                        | 
    v11 = 32;                                                                                                | 
    v12 = 3337565984LL;                                                                                      | 
    do                                                                                                       | 
    {                                                                                                        | 
      v9 -= (v10 + *(_DWORD *)&abWPAStringKey[4 * ((v12 >> 11) & 3)]) ^ (v8 + ((16 * v8) ^ (v8 >> 5)));      | 
      v10 += 1640531527;                                                                                     | 
      v12 = v10;                                                                                             | 
      v8 -= (v10 + *(_DWORD *)&abWPAStringKey[4 * (v10 & 3)]) ^ (v9 + ((16 * v9) ^ (v9 >> 5)));              | 
      --v11;                                                                                                 | 
    }                                                                                                        | 
    while ( v11 );                                                                                           | 
    *(_DWORD *)((char *)SourceString + j) = v8;                                                              | 
    *(_DWORD *)((char *)&SourceString[2] + j) = v9;                                                          | 
  }                                                                                                          | 
  RtlInitUnicodeString(&DestinationString, SourceString);                                                    | 
  if ( KeGetCurrentThread()->PreviousMode )                                                                  | 
  {                                                                                                          | 
    if ( a2 )                                                                                                | 
    {                                                                                                        | 
      v20 = 0x7FFFFFFF0000LL;                                                                                | 
      if ( (unsigned __int64)a2 < 0x7FFFFFFF0000LL )                                                         | 
      {                                                                                                      | 
        v20 = (__int64)a2;                                                                                   | 
      }                                                                                                      | 
      *(_DWORD *)v20 = *(_DWORD *)v20;                                                                       | 
      *a2 = InitSafeBootMode;                                                                                | 
    }                                                                                                        | 
  }                                                                                                          | 
  else if ( a2 )                                                                                             | 
  {                                                                                                          | 
    *a2 = InitSafeBootMode;                                                                                  | 
  }                                                                                                          | 
  ObjectAttributes.Length = 48;                                                                              | 
  ObjectAttributes.RootDirectory = 0LL;                                                                      | 
  ObjectAttributes.Attributes = 576;                                                                         | 
  ObjectAttributes.ObjectName = &DestinationString;                                                          | 
  *(_OWORD *)&ObjectAttributes.SecurityDescriptor = 0LL;                                                     | 
  result = ZwOpenKey(&KeyHandle, 0x20019u, &ObjectAttributes);                                               | 
  v14 = result;                                                                                              | 
  if ( result >= 0 )                                                                                         | 
  {                                                                                                          | 
    v15 = 0;                                                                                                 | 
    while ( 1 )                                                                                              | 
    {                                                                                                        | 
      v16 = ZwEnumerateKey(KeyHandle, v15, KeyBasicInformation, KeyInformation, 0x400u, &ResultLength);      | 
      if ( v16 == -2147483622 )                                                                              | 
      {                                                                                                      | 
        break;                                                                                               | 
      }                                                                                                      | 
      v27 = 0LL;                                                                                             | 
      memset(&v25, 0, 44);                                                                                   | 
      Handle = 0LL;                                                                                          | 
      if ( v16 < 0 )                                                                                         | 
      {                                                                                                      | 
        v14 = v16;                                                                                           | 
        break;                                                                                               | 
      }                                                                                                      | 
      if ( v35 + 64 > 0x400 )                                                                                | 
      {                                                                                                      | 
        v14 = -1073741801;                                                                                   | 
      }                                                                                                      | 
      else                                                                                                   | 
      {                                                                                                      | 
        wcscpy_s(Dst, 0x200uLL, SourceString);                                                               | 
        wcsncat_s(Dst, 0x200uLL, Src, (unsigned __int64)v35 >> 1);                                           | 
        wcscat_s(Dst, 0x200uLL, L"\\");                                                                      | 
        RtlInitUnicodeString(&v27, Dst);                                                                     | 
        v25.Length = 48;                                                                                     | 
        v25.RootDirectory = 0LL;                                                                             | 
        v25.Attributes = 576;                                                                                | 
        v25.ObjectName = &v27;                                                                               | 
        *(_OWORD *)&v25.SecurityDescriptor = 0LL;                                                            | 
        v17 = ZwOpenKey(&Handle, 0x20019u, &v25);                                                            | 
        if ( v17 < 0 )                                                                                       | 
        {                                                                                                    | 
          v14 = v17;                                                                                         | 
        }                                                                                                    | 
        else                                                                                                 | 
        {                                                                                                    | 
          Object = 0LL;                                                                                      | 
          v18 = ObReferenceObjectByHandle(Handle, 0x20019u, (POBJECT_TYPE)CmKeyObjectType, 0, &Object, 0LL); | 
          if ( v18 >= 0 )                                                                                    | 
          {                                                                                                  | 
            v19 = *(_BYTE *)(*((_QWORD *)Object + 1) + 8LL) & 0x80;                                          | 
            ObfDereferenceObject(Object);                                                                    | 
            if ( !v19 )                                                                                      | 
            {                                                                                                | 
              v18 = ZwLockRegistryKey(Handle);                                                               | 
            }                                                                                                | 
          }                                                                                                  | 
          ZwClose(Handle);                                                                                   | 
          if ( v18 >= 0 )                                                                                    | 
          {                                                                                                  | 
            v18 = v14;                                                                                       | 
          }                                                                                                  | 
          v14 = v18;                                                                                         | 
        }                                                                                                    | 
        ++v15;                                                                                               | 
      }                                                                                                      | 
    }                                                                                                        | 
    ZwClose(KeyHandle);                                                                                      | 
    if ( v16 != -2147483622 )                                                                                | 
    {                                                                                                        | 
      return v14;                                                                                            | 
    }                                                                                                        | 
    return v3;                                                                                               | 
  }                                                                                                          | 
  return result;                                                                                             | 
}                                                                                                            | 
139 row(s)
