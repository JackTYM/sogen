NTSTATUS __fastcall NtDebugContinue(void *a1, __int128 *a2, int a3)                                                  | 
{                                                                                                                    | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                      | 
                                                                                                                     | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                 | 
  v13 = *a2;                                                                                                         | 
  if ( a3 != 65538 && a3 != -2147418111 && a3 != 65537 && a3 != 1073807361 && a3 != 1073807363 && a3 != 1073807364 ) | 
  {                                                                                                                  | 
    return -1073741811;                                                                                              | 
  }                                                                                                                  | 
  Object = 0LL;                                                                                                      | 
  result = ObReferenceObjectByHandle(a1, 1u, DbgkDebugObjectType, PreviousMode, &Object, 0LL);                       | 
  v6 = result;                                                                                                       | 
  if ( result >= 0 )                                                                                                 | 
  {                                                                                                                  | 
    v7 = 0;                                                                                                          | 
    v8 = 0LL;                                                                                                        | 
    v9 = (char *)Object;                                                                                             | 
    ExAcquireFastMutex((PKGUARDED_MUTEX)((char *)Object + 24));                                                      | 
    for ( i = (char *)*((_QWORD *)v9 + 10); i != v9 + 80; i = *(char **)i )                                          | 
    {                                                                                                                | 
      if ( *((_QWORD *)i + 5) == (_QWORD)v13 )                                                                       | 
      {                                                                                                              | 
        if ( v7 )                                                                                                    | 
        {                                                                                                            | 
          *((_DWORD *)i + 19) &= ~4u;                                                                                | 
          KeSetEvent((PRKEVENT)v9, 0, 0);                                                                            | 
          break;                                                                                                     | 
        }                                                                                                            | 
        if ( *((_QWORD *)i + 6) == *((_QWORD *)&v13 + 1) && (*((_DWORD *)i + 19) & 1) != 0 )                         | 
        {                                                                                                            | 
          v11 = *(char **)i;                                                                                         | 
          v12 = (void **)*((_QWORD *)i + 1);                                                                         | 
          if ( *(char **)(*(_QWORD *)i + 8LL) != i || *v12 != i )                                                    | 
          {                                                                                                          | 
            __fastfail(3u);                                                                                          | 
          }                                                                                                          | 
          *v12 = v11;                                                                                                | 
          *((_QWORD *)v11 + 1) = v12;                                                                                | 
          v8 = i;                                                                                                    | 
          v7 = 1;                                                                                                    | 
        }                                                                                                            | 
      }                                                                                                              | 
    }                                                                                                                | 
    KeReleaseGuardedMutex((PKGUARDED_MUTEX)(v9 + 24));                                                               | 
    ObfDereferenceObject(v9);                                                                                        | 
    if ( !v7 )                                                                                                       | 
    {                                                                                                                | 
      return -1073741811;                                                                                            | 
    }                                                                                                                | 
    if ( (PerfGlobalGroupMask & 0x400000) != 0 )                                                                     | 
    {                                                                                                                | 
      EtwTraceDebuggerEvent(*((_QWORD *)v8 + 7), *((_QWORD *)v8 + 8), 2LL);                                          | 
    }                                                                                                                | 
    *((_DWORD *)v8 + 33) = a3;                                                                                       | 
    *((_DWORD *)v8 + 18) = 0;                                                                                        | 
    DbgkpWakeTarget(v8);                                                                                             | 
    return v6;                                                                                                       | 
  }                                                                                                                  | 
  return result;                                                                                                     | 
}                                                                                                                    | 
61 row(s)
