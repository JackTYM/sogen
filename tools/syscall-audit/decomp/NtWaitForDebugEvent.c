NTSTATUS __fastcall NtWaitForDebugEvent(HANDLE Handle, BOOLEAN a2, LARGE_INTEGER *p_QuadPart, _OWORD *a4) | 
{                                                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                           | 
                                                                                                          | 
  v5 = a2;                                                                                                | 
  v7 = 0LL;                                                                                               | 
  QuadPart = 0LL;                                                                                         | 
  v24 = 0LL;                                                                                              | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                      | 
  memset_0(v25, 0, 0xB8uLL);                                                                              | 
  v9 = 0x7FFFFFFF0000LL;                                                                                  | 
  if ( p_QuadPart )                                                                                       | 
  {                                                                                                       | 
    QuadPart = p_QuadPart->QuadPart;                                                                      | 
    p_QuadPart = (LARGE_INTEGER *)&QuadPart;                                                              | 
    v24 = MEMORY[0xFFFFF78000000014];                                                                     | 
  }                                                                                                       | 
  if ( PreviousMode )                                                                                     | 
  {                                                                                                       | 
    if ( (unsigned __int64)a4 < 0x7FFFFFFF0000LL )                                                        | 
    {                                                                                                     | 
      v9 = (__int64)a4;                                                                                   | 
    }                                                                                                     | 
    *(_BYTE *)v9 = *(_BYTE *)v9;                                                                          | 
    *(_BYTE *)(v9 + 183) = *(_BYTE *)(v9 + 183);                                                          | 
  }                                                                                                       | 
  Object = 0LL;                                                                                           | 
  result = ObReferenceObjectByHandle(Handle, 1u, DbgkDebugObjectType, PreviousMode, &Object, 0LL);        | 
  if ( result >= 0 )                                                                                      | 
  {                                                                                                       | 
    v11 = 0LL;                                                                                            | 
    v12 = (char *)Object;                                                                                 | 
    while ( 1 )                                                                                           | 
    {                                                                                                     | 
      v13 = KeWaitForSingleObject(v12, Executive, PreviousMode, v5, p_QuadPart);                          | 
      v14 = v13;                                                                                          | 
      if ( v13 <= -1 || v13 == 192 || (unsigned int)(v13 - 257) <= 1 )                                    | 
      {                                                                                                   | 
        break;                                                                                            | 
      }                                                                                                   | 
      v16 = 0;                                                                                            | 
      ExAcquireFastMutex((PKGUARDED_MUTEX)(v12 + 24));                                                    | 
      if ( (*((_DWORD *)v12 + 24) & 1) != 0 )                                                             | 
      {                                                                                                   | 
        v14 = -1073740972;                                                                                | 
      }                                                                                                   | 
      else                                                                                                | 
      {                                                                                                   | 
        v17 = (__int64 **)(v12 + 80);                                                                     | 
        for ( i = (__int64 *)*((_QWORD *)v12 + 10); ; i = (__int64 *)*i )                                 | 
        {                                                                                                 | 
          if ( i == (__int64 *)v17 )                                                                      | 
          {                                                                                               | 
            KeResetEvent((PRKEVENT)v12);                                                                  | 
            goto LABEL_26;                                                                                | 
          }                                                                                               | 
          v19 = i;                                                                                        | 
          if ( (*((_DWORD *)i + 19) & 5) == 0 )                                                           | 
          {                                                                                               | 
            v16 = 1;                                                                                      | 
            for ( j = *v17; j != i; j = (__int64 *)*j )                                                   | 
            {                                                                                             | 
              if ( i[5] == j[5] )                                                                         | 
              {                                                                                           | 
                *((_DWORD *)i + 19) |= 4u;                                                                | 
                i[10] = 0LL;                                                                              | 
                v16 = 0;                                                                                  | 
                break;                                                                                    | 
              }                                                                                           | 
            }                                                                                             | 
            if ( v16 )                                                                                    | 
            {                                                                                             | 
              break;                                                                                      | 
            }                                                                                             | 
          }                                                                                               | 
        }                                                                                                 | 
        v11 = (void *)i[7];                                                                               | 
        v7 = (void *)i[8];                                                                                | 
        ObfReferenceObjectWithTag(v7, 0x4F676244u);                                                       | 
        ObfReferenceObjectWithTag(v11, 0x4F676244u);                                                      | 
        DbgkpConvertKernelToUserStateChange(v25, v19);                                                    | 
        *((_DWORD *)v19 + 19) |= 1u;                                                                      | 
LABEL_26:                                                                                                 | 
        v14 = 0;                                                                                          | 
      }                                                                                                   | 
      KeReleaseGuardedMutex((PKGUARDED_MUTEX)(v12 + 24));                                                 | 
      if ( v14 < 0 )                                                                                      | 
      {                                                                                                   | 
        break;                                                                                            | 
      }                                                                                                   | 
      if ( v16 )                                                                                          | 
      {                                                                                                   | 
        DbgkpOpenHandles(v25, v11, v7);                                                                   | 
        ObfDereferenceObjectWithTag(v7, 0x4F676244u);                                                     | 
        ObfDereferenceObjectWithTag(v11, 0x4F676244u);                                                    | 
        break;                                                                                            | 
      }                                                                                                   | 
      v5 = a2;                                                                                            | 
      if ( QuadPart < 0 )                                                                                 | 
      {                                                                                                   | 
        v21 = MEMORY[0xFFFFF78000000014] - v24 + QuadPart < 0;                                            | 
        QuadPart += MEMORY[0xFFFFF78000000014] - v24;                                                     | 
        v24 = MEMORY[0xFFFFF78000000014];                                                                 | 
        v12 = (char *)Object;                                                                             | 
        if ( !v21 )                                                                                       | 
        {                                                                                                 | 
          v14 = 258;                                                                                      | 
          break;                                                                                          | 
        }                                                                                                 | 
      }                                                                                                   | 
    }                                                                                                     | 
    ObfDereferenceObject(v12);                                                                            | 
    *a4 = v25[0];                                                                                         | 
    a4[1] = v25[1];                                                                                       | 
    a4[2] = v25[2];                                                                                       | 
    a4[3] = v25[3];                                                                                       | 
    a4[4] = v25[4];                                                                                       | 
    a4[5] = v25[5];                                                                                       | 
    a4[6] = v25[6];                                                                                       | 
    v15 = a4 + 8;                                                                                         | 
    *(v15 - 1) = v25[7];                                                                                  | 
    *v15 = v25[8];                                                                                        | 
    v15[1] = v25[9];                                                                                      | 
    v15[2] = v25[10];                                                                                     | 
    *((_QWORD *)v15 + 6) = v26;                                                                           | 
    return v14;                                                                                           | 
  }                                                                                                       | 
  return result;                                                                                          | 
}                                                                                                         | 
129 row(s)
