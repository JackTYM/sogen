NTSTATUS __stdcall NtQuerySecurityObject(                                                                                                                                                 | 
        HANDLE Handle,                                                                                                                                                                    | 
        SECURITY_INFORMATION SecurityInformation,                                                                                                                                         | 
        PSECURITY_DESCRIPTOR SecurityDescriptor,                                                                                                                                          | 
        ULONG Length,                                                                                                                                                                     | 
        PULONG LengthNeeded)                                                                                                                                                              | 
{                                                                                                                                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                                                                                           | 
                                                                                                                                                                                          | 
  Lengtha = Length;                                                                                                                                                                       | 
  v30 = SecurityInformation;                                                                                                                                                              | 
  v29 = 0LL;                                                                                                                                                                              | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                                                                                      | 
  if ( PreviousMode )                                                                                                                                                                     | 
  {                                                                                                                                                                                       | 
    v8 = 0x7FFFFFFF0000LL;                                                                                                                                                                | 
    v9 = LengthNeeded;                                                                                                                                                                    | 
    if ( (unsigned __int64)LengthNeeded < 0x7FFFFFFF0000LL )                                                                                                                              | 
    {                                                                                                                                                                                     | 
      v8 = (__int64)LengthNeeded;                                                                                                                                                         | 
    }                                                                                                                                                                                     | 
    *(_DWORD *)v8 = *(_DWORD *)v8;                                                                                                                                                        | 
    if ( Lengtha )                                                                                                                                                                        | 
    {                                                                                                                                                                                     | 
      v10 = (unsigned __int64)SecurityDescriptor;                                                                                                                                         | 
      if ( ((unsigned __int8)SecurityDescriptor & 3) != 0 )                                                                                                                               | 
      {                                                                                                                                                                                   | 
        ExRaiseDatatypeMisalignment();                                                                                                                                                    | 
      }                                                                                                                                                                                   | 
      v11 = (unsigned __int64)SecurityDescriptor + Lengtha - 1;                                                                                                                           | 
      if ( (unsigned __int64)SecurityDescriptor > v11 || v11 >= 0x7FFFFFFF0000LL )                                                                                                        | 
      {                                                                                                                                                                                   | 
        ExRaiseAccessViolation();                                                                                                                                                         | 
      }                                                                                                                                                                                   | 
      v12 = (v11 & 0xFFFFFFFFFFFFF000uLL) + 4096;                                                                                                                                         | 
      do                                                                                                                                                                                  | 
      {                                                                                                                                                                                   | 
        *(_BYTE *)v10 = *(_BYTE *)v10;                                                                                                                                                    | 
        v10 = (v10 & 0xFFFFFFFFFFFFF000uLL) + 4096;                                                                                                                                       | 
      }                                                                                                                                                                                   | 
      while ( v10 != v12 );                                                                                                                                                               | 
    }                                                                                                                                                                                     | 
  }                                                                                                                                                                                       | 
  else                                                                                                                                                                                    | 
  {                                                                                                                                                                                       | 
    v9 = LengthNeeded;                                                                                                                                                                    | 
  }                                                                                                                                                                                       | 
  v13 = 0;                                                                                                                                                                                | 
  if ( (v30 & 0x10000) != 0 )                                                                                                                                                             | 
  {                                                                                                                                                                                       | 
    v13 = 16908288;                                                                                                                                                                       | 
  }                                                                                                                                                                                       | 
  v14 = v13 | 0x20000;                                                                                                                                                                    | 
  if ( (v30 & 0x17) == 0 )                                                                                                                                                                | 
  {                                                                                                                                                                                       | 
    v14 = v13;                                                                                                                                                                            | 
  }                                                                                                                                                                                       | 
  v15 = v14 | 0x20000;                                                                                                                                                                    | 
  if ( (v30 & 0x20) == 0 )                                                                                                                                                                | 
  {                                                                                                                                                                                       | 
    v15 = v14;                                                                                                                                                                            | 
  }                                                                                                                                                                                       | 
  v16 = v15 | 0x20000;                                                                                                                                                                    | 
  if ( (v30 & 0x40) == 0 )                                                                                                                                                                | 
  {                                                                                                                                                                                       | 
    v16 = v15;                                                                                                                                                                            | 
  }                                                                                                                                                                                       | 
  v17 = v16 | 0x20000;                                                                                                                                                                    | 
  if ( (v30 & 0x80u) == 0 )                                                                                                                                                               | 
  {                                                                                                                                                                                       | 
    v17 = v16;                                                                                                                                                                            | 
  }                                                                                                                                                                                       | 
  if ( (v30 & 0x100) != 0 )                                                                                                                                                               | 
  {                                                                                                                                                                                       | 
    v17 |= 0x20000u;                                                                                                                                                                      | 
  }                                                                                                                                                                                       | 
  Object = 0LL;                                                                                                                                                                           | 
  v18 = v17 | 0x1000000;                                                                                                                                                                  | 
  if ( (v30 & 8) == 0 )                                                                                                                                                                   | 
  {                                                                                                                                                                                       | 
    v18 = v17;                                                                                                                                                                            | 
  }                                                                                                                                                                                       | 
  result = ObReferenceObjectByHandle(Handle, v18, 0LL, PreviousMode, &Object, &v29);                                                                                                      | 
  if ( result >= 0 )                                                                                                                                                                      | 
  {                                                                                                                                                                                       | 
    v20 = Object;                                                                                                                                                                         | 
    v21 = ObTypeIndexTable[(unsigned __int8)ObHeaderCookie ^ (unsigned __int8)*((char *)Object - 24) ^ (unsigned __int64)(unsigned __int8)((unsigned __int16)((_WORD)Object - 48) >> 8)]; | 
    v22 = *(__int64 (__usercall **)@<rax>(ULONG_PTR@<rcx>, PULONG, int, int, __int64, char))(v21 + 152);                                                                                  | 
    v27 = v21 + 76;                                                                                                                                                                       | 
    v26 = *(_DWORD *)(v21 + 100);                                                                                                                                                         | 
    HandleInformation = (POBJECT_HANDLE_INFORMATION)((char *)Object - 8);                                                                                                                 | 
    if ( (char *)v22 == (char *)SeDefaultObjectMethod )                                                                                                                                   | 
    {                                                                                                                                                                                     | 
      SetSecurityObject = SeDefaultObjectMethod(Object, 1LL, &v30, SecurityDescriptor, &Lengtha);                                                                                         | 
    }                                                                                                                                                                                     | 
    else if ( v22 == IopGetSetSecurityObject )                                                                                                                                            | 
    {                                                                                                                                                                                     | 
      SetSecurityObject = IopGetSetSecurityObject(                                                                                                                                        | 
                            (ULONG_PTR)Object,                                                                                                                                            | 
                            &Lengtha,                                                                                                                                                     | 
                            (int)HandleInformation,                                                                                                                                       | 
                            v26,                                                                                                                                                          | 
                            v27,                                                                                                                                                          | 
                            PreviousMode);                                                                                                                                                | 
    }                                                                                                                                                                                     | 
    else if ( (char *)v22 == (char *)CmpSecurityMethod )                                                                                                                                  | 
    {                                                                                                                                                                                     | 
      SetSecurityObject = CmpSecurityMethod(                                                                                                                                              | 
                            (_DWORD)Object,                                                                                                                                               | 
                            1,                                                                                                                                                            | 
                            (unsigned int)&v30,                                                                                                                                           | 
                            (_DWORD)SecurityDescriptor,                                                                                                                                   | 
                            (__int64)&Lengtha,                                                                                                                                            | 
                            (__int64)HandleInformation,                                                                                                                                   | 
                            v26,                                                                                                                                                          | 
                            v27);                                                                                                                                                         | 
    }                                                                                                                                                                                     | 
    else                                                                                                                                                                                  | 
    {                                                                                                                                                                                     | 
      SetSecurityObject = guard_dispatch_icall_no_overrides(Object, 1LL, &v30, SecurityDescriptor);                                                                                       | 
    }                                                                                                                                                                                     | 
    v24 = SetSecurityObject;                                                                                                                                                              | 
    *v9 = Lengtha;                                                                                                                                                                        | 
    ObfDereferenceObject(v20);                                                                                                                                                            | 
    return v24;                                                                                                                                                                           | 
  }                                                                                                                                                                                       | 
  return result;                                                                                                                                                                          | 
}                                                                                                                                                                                         | 
