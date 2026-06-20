NTSTATUS __fastcall NtSignalAndWaitForSingleObject(void *a1, void *a2, BOOLEAN a3, unsigned __int64 a4)                                                                                             | 
{                                                                                                                                                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                                                                                                     | 
                                                                                                                                                                                                    | 
  v6 = 0;                                                                                                                                                                                           | 
  HandleInformation = 0LL;                                                                                                                                                                          | 
  SystemArgument1 = 0LL;                                                                                                                                                                            | 
  v30[0] = 0LL;                                                                                                                                                                                     | 
  Object = 0LL;                                                                                                                                                                                     | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                                                                                                | 
  v8 = (LARGE_INTEGER *)a4;                                                                                                                                                                         | 
  if ( a4 && PreviousMode )                                                                                                                                                                         | 
  {                                                                                                                                                                                                 | 
    v21 = 0x7FFFFFFF0000LL;                                                                                                                                                                         | 
    if ( a4 < 0x7FFFFFFF0000LL )                                                                                                                                                                    | 
    {                                                                                                                                                                                               | 
      v21 = a4;                                                                                                                                                                                     | 
    }                                                                                                                                                                                               | 
    v30[0] = *(_QWORD *)v21;                                                                                                                                                                        | 
    v8 = (LARGE_INTEGER *)v30;                                                                                                                                                                      | 
  }                                                                                                                                                                                                 | 
  result = ObReferenceObjectByHandleWithTag(a1, 0, 0LL, PreviousMode, 0x7457624Fu, &SystemArgument1, &HandleInformation);                                                                           | 
  if ( result >= 0 )                                                                                                                                                                                | 
  {                                                                                                                                                                                                 | 
    v25 = ObReferenceObjectByHandleWithTag(a2, 0x100000u, 0LL, PreviousMode, 0x7457624Fu, &Object, 0LL);                                                                                            | 
    if ( v25 < 0 )                                                                                                                                                                                  | 
    {                                                                                                                                                                                               | 
LABEL_16:                                                                                                                                                                                           | 
      v18 = (char *)SystemArgument1 - 48;                                                                                                                                                           | 
      if ( ObpTraceFlags )                                                                                                                                                                          | 
      {                                                                                                                                                                                             | 
        ObpPushStackInfo((_DWORD)SystemArgument1 - 48);                                                                                                                                             | 
      }                                                                                                                                                                                             | 
      v19 = _InterlockedExchangeAdd64((volatile signed __int64 *)v18, 0xFFFFFFFFFFFFFFFFuLL);                                                                                                       | 
      v16 = v19 <= 1;                                                                                                                                                                               | 
      v20 = v19 - 1;                                                                                                                                                                                | 
      if ( v16 )                                                                                                                                                                                    | 
      {                                                                                                                                                                                             | 
        if ( *((_QWORD *)v18 + 1) )                                                                                                                                                                 | 
        {                                                                                                                                                                                           | 
          KeBugCheckEx(                                                                                                                                                                             | 
            0x18u,                                                                                                                                                                                  | 
            ObTypeIndexTable[(unsigned __int8)ObHeaderCookie ^ (unsigned __int8)v18[24] ^ (unsigned __int64)BYTE1(v18)],                                                                            | 
            (ULONG_PTR)SystemArgument1,                                                                                                                                                             | 
            1uLL,                                                                                                                                                                                   | 
            *((_QWORD *)v18 + 1));                                                                                                                                                                  | 
        }                                                                                                                                                                                           | 
        if ( v20 < 0 )                                                                                                                                                                              | 
        {                                                                                                                                                                                           | 
          KeBugCheckEx(0x18u, 0LL, (ULONG_PTR)SystemArgument1, 2uLL, v20);                                                                                                                          | 
        }                                                                                                                                                                                           | 
        if ( KeAreAllApcsDisabled() )                                                                                                                                                               | 
        {                                                                                                                                                                                           | 
          ObpDeferObjectDeletion(v18);                                                                                                                                                              | 
        }                                                                                                                                                                                           | 
        else                                                                                                                                                                                        | 
        {                                                                                                                                                                                           | 
          v22 = OBJECT_HEADER_TO_HANDLE_REVOCATION_INFO(v18);                                                                                                                                       | 
          if ( v22 )                                                                                                                                                                                | 
          {                                                                                                                                                                                         | 
            ObpHandleRevocationBlockRemoveObject(v22);                                                                                                                                              | 
          }                                                                                                                                                                                         | 
          LOBYTE(v6) = ObpTraceFlags != 0;                                                                                                                                                          | 
          if ( v6 )                                                                                                                                                                                 | 
          {                                                                                                                                                                                         | 
            ObpDeregisterObject(v18);                                                                                                                                                               | 
          }                                                                                                                                                                                         | 
          ObpRemoveObjectRoutine(v18, 0LL);                                                                                                                                                         | 
        }                                                                                                                                                                                           | 
      }                                                                                                                                                                                             | 
      return v25;                                                                                                                                                                                   | 
    }                                                                                                                                                                                               | 
    v10 = (char *)Object - 48;                                                                                                                                                                      | 
    v30[2] = (char *)Object - 48;                                                                                                                                                                   | 
    v26 = (unsigned __int8)((unsigned __int16)((_WORD)Object - 48) >> 8);                                                                                                                           | 
    v31 = (unsigned __int8 *)Object - 24;                                                                                                                                                           | 
    v11 = (struct _OBJECT_TYPE *)ObTypeIndexTable[*v31 ^ v26 ^ (unsigned __int8)ObHeaderCookie];                                                                                                    | 
    DefaultObject = (__int64)v11->DefaultObject;                                                                                                                                                    | 
    if ( (DefaultObject & 1) == 0 )                                                                                                                                                                 | 
    {                                                                                                                                                                                               | 
      if ( DefaultObject < 0 )                                                                                                                                                                      | 
      {                                                                                                                                                                                             | 
        goto LABEL_7;                                                                                                                                                                               | 
      }                                                                                                                                                                                             | 
      goto LABEL_6;                                                                                                                                                                                 | 
    }                                                                                                                                                                                               | 
    if ( (DefaultObject & 2) != 0 )                                                                                                                                                                 | 
    {                                                                                                                                                                                               | 
      if ( (*(_DWORD *)((_BYTE *)Object + v11->TypeInfo.WaitObjectFlagOffset) & v11->TypeInfo.WaitObjectFlagMask) != v11->TypeInfo.WaitObjectFlagMask )                                             | 
      {                                                                                                                                                                                             | 
        DefaultObject -= 3LL;                                                                                                                                                                       | 
LABEL_6:                                                                                                                                                                                            | 
        DefaultObject += (__int64)Object;                                                                                                                                                           | 
        goto LABEL_7;                                                                                                                                                                               | 
      }                                                                                                                                                                                             | 
      DefaultObject = *(_QWORD *)((char *)Object + v11->TypeInfo.WaitObjectPointerOffset);                                                                                                          | 
    }                                                                                                                                                                                               | 
    else                                                                                                                                                                                            | 
    {                                                                                                                                                                                               | 
      DefaultObject = *(_QWORD *)((char *)Object + DefaultObject - 1);                                                                                                                              | 
    }                                                                                                                                                                                               | 
LABEL_7:                                                                                                                                                                                            | 
    if ( ExCrossVmMutantObjectType != v11 )                                                                                                                                                         | 
    {                                                                                                                                                                                               | 
      v13 = (POBJECT_TYPE *)ObTypeIndexTable[(unsigned __int8)ObHeaderCookie ^ (unsigned __int8)*((char *)SystemArgument1                                                                           | 
                                                                                                - 24) ^ (unsigned __int64)(unsigned __int8)((unsigned __int16)((_WORD)SystemArgument1 - 48) >> 8)]; | 
      v25 = -1073741790;                                                                                                                                                                            | 
      if ( v13 == ExEventObjectType )                                                                                                                                                               | 
      {                                                                                                                                                                                             | 
        if ( !PreviousMode || (~LOBYTE(HandleInformation.GrantedAccess) & 2) == 0 )                                                                                                                 | 
        {                                                                                                                                                                                           | 
          KeSetEvent((PRKEVENT)SystemArgument1, 1, 1u);                                                                                                                                             | 
LABEL_12:                                                                                                                                                                                           | 
          v25 = KeWaitForSingleObject((PVOID)DefaultObject, UserRequest, PreviousMode, a3, v8);                                                                                                     | 
          v14 = v26;                                                                                                                                                                                | 
LABEL_13:                                                                                                                                                                                           | 
          if ( ObpTraceFlags )                                                                                                                                                                      | 
          {                                                                                                                                                                                         | 
            ObpPushStackInfo((_DWORD)v10);                                                                                                                                                          | 
          }                                                                                                                                                                                         | 
          v15 = _InterlockedExchangeAdd64((volatile signed __int64 *)v10, 0xFFFFFFFFFFFFFFFFuLL);                                                                                                   | 
          v16 = v15 <= 1;                                                                                                                                                                           | 
          v17 = v15 - 1;                                                                                                                                                                            | 
          if ( v16 )                                                                                                                                                                                | 
          {                                                                                                                                                                                         | 
            if ( *((_QWORD *)v10 + 1) )                                                                                                                                                             | 
            {                                                                                                                                                                                       | 
              KeBugCheckEx(                                                                                                                                                                         | 
                0x18u,                                                                                                                                                                              | 
                ObTypeIndexTable[*v31 ^ (unsigned __int8)ObHeaderCookie ^ (unsigned __int64)v14],                                                                                                   | 
                (ULONG_PTR)Object,                                                                                                                                                                  | 
                1uLL,                                                                                                                                                                               | 
                *((_QWORD *)v10 + 1));                                                                                                                                                              | 
            }                                                                                                                                                                                       | 
            if ( v17 < 0 )                                                                                                                                                                          | 
            {                                                                                                                                                                                       | 
              KeBugCheckEx(0x18u, 0LL, (ULONG_PTR)Object, 2uLL, v17);                                                                                                                               | 
            }                                                                                                                                                                                       | 
            if ( KeAreAllApcsDisabled() )                                                                                                                                                           | 
            {                                                                                                                                                                                       | 
              ObpDeferObjectDeletion(v10);                                                                                                                                                          | 
            }                                                                                                                                                                                       | 
            else                                                                                                                                                                                    | 
            {                                                                                                                                                                                       | 
              v23 = OBJECT_HEADER_TO_HANDLE_REVOCATION_INFO(v10);                                                                                                                                   | 
              if ( v23 )                                                                                                                                                                            | 
              {                                                                                                                                                                                     | 
                ObpHandleRevocationBlockRemoveObject(v23);                                                                                                                                          | 
              }                                                                                                                                                                                     | 
              if ( ObpTraceFlags )                                                                                                                                                                  | 
              {                                                                                                                                                                                     | 
                ObpDeregisterObject(v10);                                                                                                                                                           | 
              }                                                                                                                                                                                     | 
              ObpRemoveObjectRoutine(v10, 0LL);                                                                                                                                                     | 
            }                                                                                                                                                                                       | 
          }                                                                                                                                                                                         | 
          goto LABEL_16;                                                                                                                                                                            | 
        }                                                                                                                                                                                           | 
LABEL_62:                                                                                                                                                                                           | 
        v14 = v26;                                                                                                                                                                                  | 
        goto LABEL_13;                                                                                                                                                                              | 
      }                                                                                                                                                                                             | 
      if ( v13 == (POBJECT_TYPE *)ExMutantObjectType )                                                                                                                                              | 
      {                                                                                                                                                                                             | 
        v24 = KeReleaseMutantEx((ULONG_PTR)SystemArgument1);                                                                                                                                        | 
        v25 = v24;                                                                                                                                                                                  | 
        if ( v24 != 128 && v24 != -1073741754 )                                                                                                                                                     | 
        {                                                                                                                                                                                           | 
          goto LABEL_12;                                                                                                                                                                            | 
        }                                                                                                                                                                                           | 
        goto LABEL_62;                                                                                                                                                                              | 
      }                                                                                                                                                                                             | 
      if ( v13 == ExSemaphoreObjectType )                                                                                                                                                           | 
      {                                                                                                                                                                                             | 
        if ( !PreviousMode || (~LOBYTE(HandleInformation.GrantedAccess) & 2) == 0 )                                                                                                                 | 
        {                                                                                                                                                                                           | 
          v25 = KeReleaseSemaphoreEx(SystemArgument1, 1, 0LL);                                                                                                                                      | 
          if ( v25 != -1073741753 )                                                                                                                                                                 | 
          {                                                                                                                                                                                         | 
            goto LABEL_12;                                                                                                                                                                          | 
          }                                                                                                                                                                                         | 
        }                                                                                                                                                                                           | 
        goto LABEL_62;                                                                                                                                                                              | 
      }                                                                                                                                                                                             | 
    }                                                                                                                                                                                               | 
    v25 = -1073741788;                                                                                                                                                                              | 
    goto LABEL_62;                                                                                                                                                                                  | 
  }                                                                                                                                                                                                 | 
  return result;                                                                                                                                                                                    | 
}                                                                                                                                                                                                   | 
190 row(s)
