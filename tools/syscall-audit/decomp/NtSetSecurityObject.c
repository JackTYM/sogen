NTSTATUS __stdcall NtSetSecurityObject(                                                                                                                                                       | 
        HANDLE Handle,                                                                                                                                                                        | 
        SECURITY_INFORMATION SecurityInformation,                                                                                                                                             | 
        PSECURITY_DESCRIPTOR SecurityDescriptor)                                                                                                                                              | 
{                                                                                                                                                                                             | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                                                                                               | 
                                                                                                                                                                                              | 
  v3 = (int)SecurityDescriptor;                                                                                                                                                               | 
  v35 = 0LL;                                                                                                                                                                                  | 
  v4 = SecurityInformation;                                                                                                                                                                   | 
  HandleInformation = 0LL;                                                                                                                                                                    | 
  v32 = 0LL;                                                                                                                                                                                  | 
  SecurityDescriptora = 0LL;                                                                                                                                                                  | 
  P = 0LL;                                                                                                                                                                                    | 
  v33 = 0LL;                                                                                                                                                                                  | 
  if ( !SecurityDescriptor )                                                                                                                                                                  | 
  {                                                                                                                                                                                           | 
    return -1073741819;                                                                                                                                                                       | 
  }                                                                                                                                                                                           | 
  v6 = SecurityInformation & 0x100;                                                                                                                                                           | 
  v7 = SecurityInformation & 0x80;                                                                                                                                                            | 
  if ( (v4 & 0x10000) != 0 )                                                                                                                                                                  | 
  {                                                                                                                                                                                           | 
    v26 = v4 | 0x1FF;                                                                                                                                                                         | 
    v4 = v4 & 0xFFFFFE00 | 0x17F;                                                                                                                                                             | 
    if ( v7 )                                                                                                                                                                                 | 
    {                                                                                                                                                                                         | 
      v4 = v26;                                                                                                                                                                               | 
    }                                                                                                                                                                                         | 
    if ( !v6 )                                                                                                                                                                                | 
    {                                                                                                                                                                                         | 
      v4 &= ~0x100u;                                                                                                                                                                          | 
    }                                                                                                                                                                                         | 
  }                                                                                                                                                                                           | 
  v8 = ((v4 & 0x10000) != 0 ? 0x1040000 : 0) | 0x80000;                                                                                                                                       | 
  if ( (v4 & 0x13) == 0 )                                                                                                                                                                     | 
  {                                                                                                                                                                                           | 
    v8 = (v4 & 0x10000) != 0 ? 0x10C0000 : 0;                                                                                                                                                 | 
  }                                                                                                                                                                                           | 
  v9 = v8 | 0x40000;                                                                                                                                                                          | 
  if ( (v4 & 4) == 0 )                                                                                                                                                                        | 
  {                                                                                                                                                                                           | 
    v9 = v8;                                                                                                                                                                                  | 
  }                                                                                                                                                                                           | 
  v10 = v9 | 0x40000;                                                                                                                                                                         | 
  if ( (v4 & 0x20) == 0 )                                                                                                                                                                     | 
  {                                                                                                                                                                                           | 
    v10 = v9;                                                                                                                                                                                 | 
  }                                                                                                                                                                                           | 
  v11 = v10 | 0x40000;                                                                                                                                                                        | 
  if ( (v4 & 0x80u) == 0 )                                                                                                                                                                    | 
  {                                                                                                                                                                                           | 
    v11 = v10;                                                                                                                                                                                | 
  }                                                                                                                                                                                           | 
  v12 = v11 | 0x40000;                                                                                                                                                                        | 
  if ( (v4 & 0x100) == 0 )                                                                                                                                                                    | 
  {                                                                                                                                                                                           | 
    v12 = v11;                                                                                                                                                                                | 
  }                                                                                                                                                                                           | 
  v13 = v12 | 0x1000000;                                                                                                                                                                      | 
  if ( (v4 & 0x40) == 0 )                                                                                                                                                                     | 
  {                                                                                                                                                                                           | 
    v13 = v12;                                                                                                                                                                                | 
  }                                                                                                                                                                                           | 
  v14 = v13 | 0x1000000;                                                                                                                                                                      | 
  if ( (v4 & 8) == 0 )                                                                                                                                                                        | 
  {                                                                                                                                                                                           | 
    v14 = v13;                                                                                                                                                                                | 
  }                                                                                                                                                                                           | 
  v15 = v14;                                                                                                                                                                                  | 
  if ( (v4 & 0x1F8) != 0 && (v4 & 0x50000000) != 0 )                                                                                                                                          | 
  {                                                                                                                                                                                           | 
    v15 = v14 | 0x1000000;                                                                                                                                                                    | 
  }                                                                                                                                                                                           | 
  Object = 0LL;                                                                                                                                                                               | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                                                                                          | 
  v39 = PreviousMode;                                                                                                                                                                         | 
  SecurityDescriptorInfo = ObReferenceObjectByHandle(Handle, v15, 0LL, PreviousMode, &Object, &HandleInformation);                                                                            | 
  if ( SecurityDescriptorInfo < 0 )                                                                                                                                                           | 
  {                                                                                                                                                                                           | 
    return SecurityDescriptorInfo;                                                                                                                                                            | 
  }                                                                                                                                                                                           | 
  LOBYTE(v19) = 1;                                                                                                                                                                            | 
  LOBYTE(v17) = PreviousMode;                                                                                                                                                                 | 
  v20 = SeCaptureSecurityDescriptor(v3, v17, 1, v19, (__int64)&v35);                                                                                                                          | 
  v21 = Object;                                                                                                                                                                               | 
  SecurityDescriptorInfo = v20;                                                                                                                                                               | 
  if ( v20 < 0 )                                                                                                                                                                              | 
  {                                                                                                                                                                                           | 
LABEL_40:                                                                                                                                                                                     | 
    ObfDereferenceObject(v21);                                                                                                                                                                | 
    return SecurityDescriptorInfo;                                                                                                                                                            | 
  }                                                                                                                                                                                           | 
  v22 = v35;                                                                                                                                                                                  | 
  if ( (*((_BYTE *)v35 + 2) & 0x10) == 0 && (v4 & 0x10000) != 0 )                                                                                                                             | 
  {                                                                                                                                                                                           | 
    v4 &= 0xFFFFFE07;                                                                                                                                                                         | 
  }                                                                                                                                                                                           | 
  if ( ((v4 & 1) == 0 || *((_DWORD *)v35 + 1)) && ((v4 & 2) == 0 || *((_DWORD *)v35 + 2)) )                                                                                                   | 
  {                                                                                                                                                                                           | 
    if ( (HandleInformation.HandleAttributes & 4) == 0 )                                                                                                                                      | 
    {                                                                                                                                                                                         | 
      v15 &= 0xFFF3FFFF;                                                                                                                                                                      | 
    }                                                                                                                                                                                         | 
    if ( !v15 )                                                                                                                                                                               | 
    {                                                                                                                                                                                         | 
      goto LABEL_26;                                                                                                                                                                          | 
    }                                                                                                                                                                                         | 
    SecurityDescriptorInfo = ObpAllocateAndQuerySecurityDescriptorInfo((ULONG_PTR)Object);                                                                                                    | 
    if ( SecurityDescriptorInfo < 0 )                                                                                                                                                         | 
    {                                                                                                                                                                                         | 
      v24 = v32;                                                                                                                                                                              | 
    }                                                                                                                                                                                         | 
    else                                                                                                                                                                                      | 
    {                                                                                                                                                                                         | 
      if ( (v15 & 0xFEFFFFFF) == 0 )                                                                                                                                                          | 
      {                                                                                                                                                                                       | 
LABEL_26:                                                                                                                                                                                     | 
        v23 = SecurityDescriptora;                                                                                                                                                            | 
        goto LABEL_27;                                                                                                                                                                        | 
      }                                                                                                                                                                                       | 
      v23 = SecurityDescriptora;                                                                                                                                                              | 
      LOBYTE(Object) = 0;                                                                                                                                                                     | 
      SaclSecurityDescriptor = RtlGetSaclSecurityDescriptor(                                                                                                                                  | 
                                 SecurityDescriptora,                                                                                                                                         | 
                                 (PBOOLEAN)&Object,                                                                                                                                           | 
                                 (PACL *)&HandleInformation,                                                                                                                                  | 
                                 SaclDefaulted);                                                                                                                                              | 
      SecurityDescriptorInfo = SaclSecurityDescriptor;                                                                                                                                        | 
      v29 = (char)Object;                                                                                                                                                                     | 
      if ( !(_BYTE)Object )                                                                                                                                                                   | 
      {                                                                                                                                                                                       | 
        Object = 0LL;                                                                                                                                                                         | 
        LOBYTE(v28) = 1;                                                                                                                                                                      | 
        v29 = (unsigned int)SepRmGlobalSaclFind(                                                                                                                                              | 
                              &Object,                                                                                                                                                        | 
                              0LL,                                                                                                                                                            | 
                              ObTypeIndexTable[(unsigned __int8)(ObHeaderCookie ^ *((_BYTE *)v21 - 24) ^ ((unsigned __int16)((_WORD)v21 - 48) >> 8))]                                         | 
                            + 16,                                                                                                                                                             | 
                              v28) != -1073741772;                                                                                                                                            | 
      }                                                                                                                                                                                       | 
      if ( SecurityDescriptorInfo >= 0 )                                                                                                                                                      | 
      {                                                                                                                                                                                       | 
        if ( !v29 )                                                                                                                                                                           | 
        {                                                                                                                                                                                     | 
          v15 &= 0xFFF3FFFF;                                                                                                                                                                  | 
LABEL_27:                                                                                                                                                                                     | 
          v24 = v32;                                                                                                                                                                          | 
          if ( (v4 & 0x40) == 0                                                                                                                                                               | 
            || ((unsigned __int8)ObHeaderCookie ^ (unsigned __int8)(*((_BYTE *)v21 - 24) ^ ((unsigned __int16)((_WORD)v21 - 48) >> 8))) == *((_BYTE *)IoFileObjectType + 40)                  | 
            && (*(_DWORD *)(v21[1] + 52LL) & 0x10) != 0                                                                                                                                       | 
            || (SecurityDescriptorInfo = ObpAllocateAndQuerySecurityDescriptorInfo((ULONG_PTR)v21),                                                                                           | 
                SecurityDescriptorInfo >= 0) )                                                                                                                                                | 
          {                                                                                                                                                                                   | 
            if ( (LODWORD(Object) = v4 & 0x20, (v4 & 0x20) == 0)                                                                                                                              | 
              || ((unsigned __int8)ObHeaderCookie ^ (unsigned __int8)(*((_BYTE *)v21 - 24) ^ ((unsigned __int16)((_WORD)v21 - 48) >> 8))) == *((_BYTE *)IoFileObjectType + 40)                | 
              && (*(_DWORD *)(v21[1] + 52LL) & 0x10) != 0                                                                                                                                     | 
              || (SecurityDescriptorInfo = ObpAllocateAndQuerySecurityDescriptorInfo((ULONG_PTR)v21),                                                                                         | 
                  SecurityDescriptorInfo >= 0) )                                                                                                                                              | 
            {                                                                                                                                                                                 | 
              SecurityDescriptorInfo = ObSetSecurityObjectByPointer(v21, v4, v22);                                                                                                            | 
              if ( SecurityDescriptorInfo >= 0 && (v15 || (_DWORD)Object) )                                                                                                                   | 
              {                                                                                                                                                                               | 
                SeSecurityDescriptorChangedAuditAlarm(                                                                                                                                        | 
                  0,                                                                                                                                                                          | 
                  (_DWORD)v21,                                                                                                                                                                | 
                  LODWORD(ObTypeIndexTable[(unsigned __int8)ObHeaderCookie ^ *((unsigned __int8 *)v21 - 24) ^ (unsigned __int64)(unsigned __int8)((unsigned __int16)((_WORD)v21 - 48) >> 8)]) | 
                + 16,                                                                                                                                                                         | 
                  0,                                                                                                                                                                          | 
                  (__int64)Handle,                                                                                                                                                            | 
                  0,                                                                                                                                                                          | 
                  v15,                                                                                                                                                                        | 
                  v4,                                                                                                                                                                         | 
                  (__int64)v24,                                                                                                                                                               | 
                  (__int64)v23,                                                                                                                                                               | 
                  (__int64)P,                                                                                                                                                                 | 
                  (__int64)v33,                                                                                                                                                               | 
                  (__int64)v22);                                                                                                                                                              | 
              }                                                                                                                                                                               | 
            }                                                                                                                                                                                 | 
          }                                                                                                                                                                                   | 
          if ( v33 )                                                                                                                                                                          | 
          {                                                                                                                                                                                   | 
            ExFreePoolWithTag(v33, 0);                                                                                                                                                        | 
          }                                                                                                                                                                                   | 
          if ( P )                                                                                                                                                                            | 
          {                                                                                                                                                                                   | 
            ExFreePoolWithTag(P, 0);                                                                                                                                                          | 
          }                                                                                                                                                                                   | 
          goto LABEL_36;                                                                                                                                                                      | 
        }                                                                                                                                                                                     | 
        SecurityDescriptorInfo = ObpAllocateAndQuerySecurityDescriptorInfo((ULONG_PTR)v21);                                                                                                   | 
        if ( SecurityDescriptorInfo >= 0 )                                                                                                                                                    | 
        {                                                                                                                                                                                     | 
          goto LABEL_27;                                                                                                                                                                      | 
        }                                                                                                                                                                                     | 
      }                                                                                                                                                                                       | 
      v24 = v32;                                                                                                                                                                              | 
    }                                                                                                                                                                                         | 
LABEL_36:                                                                                                                                                                                     | 
    if ( v24 )                                                                                                                                                                                | 
    {                                                                                                                                                                                         | 
      ExFreePoolWithTag(v24, 0);                                                                                                                                                              | 
    }                                                                                                                                                                                         | 
    if ( (unsigned __int8)v39 <= 1u )                                                                                                                                                         | 
    {                                                                                                                                                                                         | 
      ExFreePoolWithTag(v22, 0);                                                                                                                                                              | 
    }                                                                                                                                                                                         | 
    goto LABEL_40;                                                                                                                                                                            | 
  }                                                                                                                                                                                           | 
  if ( (unsigned __int8)PreviousMode <= 1u )                                                                                                                                                  | 
  {                                                                                                                                                                                           | 
    ExFreePoolWithTag(v35, 0);                                                                                                                                                                | 
  }                                                                                                                                                                                           | 
  ObfDereferenceObject(Object);                                                                                                                                                               | 
  return -1073741703;                                                                                                                                                                         | 
}                                                                                                                                                                                             | 
