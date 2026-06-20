__int64 __fastcall NtSubmitIoRing(HANDLE Handle, unsigned int a2, int a3, __int64 *a4)                               | 
{                                                                                                                    | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                      | 
                                                                                                                     | 
  v4 = a3;                                                                                                           | 
  v27 = 0;                                                                                                           | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                 | 
  v28 = PreviousMode;                                                                                                | 
  v6 = 0LL;                                                                                                          | 
  v7 = 0;                                                                                                            | 
  v32 = 0LL;                                                                                                         | 
  v25 = 0;                                                                                                           | 
  if ( (_WORD)a2 )                                                                                                   | 
  {                                                                                                                  | 
    v27 = -1069154303;                                                                                               | 
    goto LABEL_25;                                                                                                   | 
  }                                                                                                                  | 
  if ( a3 && a4 && PreviousMode )                                                                                    | 
  {                                                                                                                  | 
    v8 = 0x7FFFFFFF0000LL;                                                                                           | 
    if ( (unsigned __int64)a4 < 0x7FFFFFFF0000LL )                                                                   | 
    {                                                                                                                | 
      v8 = (__int64)a4;                                                                                              | 
    }                                                                                                                | 
    v32 = *(_QWORD *)v8;                                                                                             | 
    a4 = &v32;                                                                                                       | 
  }                                                                                                                  | 
  Object = 0LL;                                                                                                      | 
  v9 = ObReferenceObjectByHandle(Handle, 0, IoRingObjectType, PreviousMode, &Object, 0LL);                           | 
  v6 = Object;                                                                                                       | 
  v27 = v9;                                                                                                          | 
  if ( v9 >= 0 )                                                                                                     | 
  {                                                                                                                  | 
    if ( _InterlockedCompareExchange((volatile signed __int32 *)Object + 24, 1, 0) )                                 | 
    {                                                                                                                | 
      v27 = -1069154298;                                                                                             | 
      goto LABEL_25;                                                                                                 | 
    }                                                                                                                | 
    v7 = 1;                                                                                                          | 
    v12 = (int *)*((_QWORD *)v6 + 8);                                                                                | 
    v31 = *((_DWORD *)v6 + 6);                                                                                       | 
    v29 = v12[1];                                                                                                    | 
    v13 = v29 - *v12;                                                                                                | 
    v14 = Object;                                                                                                    | 
    if ( v13 > *((_DWORD *)Object + 5) )                                                                             | 
    {                                                                                                                | 
      v27 = -1069154297;                                                                                             | 
      goto LABEL_25;                                                                                                 | 
    }                                                                                                                | 
    if ( v4 )                                                                                                        | 
    {                                                                                                                | 
      v27 = IopIoRingSetupCompletionWait((_DWORD)Object, v13, v4, 0, (__int64)&v25);                                 | 
      if ( v27 < 0 )                                                                                                 | 
      {                                                                                                              | 
        goto LABEL_25;                                                                                               | 
      }                                                                                                              | 
    }                                                                                                                | 
    else                                                                                                             | 
    {                                                                                                                | 
      v14 = Object;                                                                                                  | 
      v22 = *((_QWORD *)Object + 14)                                                                                 | 
          + (unsigned int)(*(_DWORD *)(*((_QWORD *)Object + 10) + 4LL) - **((_DWORD **)Object + 10))                 | 
          - *((_QWORD *)Object + 15);                                                                                | 
      v23 = *((unsigned int *)Object + 7);                                                                           | 
      if ( v22 < v23 )                                                                                               | 
      {                                                                                                              | 
        v24 = v23 - v22;                                                                                             | 
      }                                                                                                              | 
      else                                                                                                           | 
      {                                                                                                              | 
        v24 = 0;                                                                                                     | 
      }                                                                                                              | 
      if ( v13 > v24 && v13 > (unsigned int)IopIoRingGetAvailableCqSlots(Object) )                                   | 
      {                                                                                                              | 
        v27 = -1069154296;                                                                                           | 
        goto LABEL_25;                                                                                               | 
      }                                                                                                              | 
    }                                                                                                                | 
    v15 = *v12;                                                                                                      | 
    v16 = 0;                                                                                                         | 
    v26 = 0;                                                                                                         | 
    if ( v15 != v29 )                                                                                                | 
    {                                                                                                                | 
      v17 = v29;                                                                                                     | 
      v18 = v31;                                                                                                     | 
      do                                                                                                             | 
      {                                                                                                              | 
        v19 = (unsigned __int64)(v15 & v18) << 6;                                                                    | 
        v33 = *(_OWORD *)((char *)v12 + v19 + 16);                                                                   | 
        v34 = *(_OWORD *)((char *)v12 + v19 + 32);                                                                   | 
        v35 = *(_OWORD *)((char *)v12 + v19 + 48);                                                                   | 
        v36 = *(_OWORD *)((char *)v12 + v19 + 64);                                                                   | 
        *v12 = ++v15;                                                                                                | 
        IopProcessIoRingEntry(v14);                                                                                  | 
      }                                                                                                              | 
      while ( v15 != v17 );                                                                                          | 
      v16 = v26;                                                                                                     | 
      v4 = a3;                                                                                                       | 
    }                                                                                                                | 
    if ( v25 )                                                                                                       | 
    {                                                                                                                | 
      if ( !v16                                                                                                      | 
        || (LOBYTE(v11) = 1, v27 = IopIoRingSetupCompletionWait((_DWORD)v14, 0, v4, v11, (__int64)&v25), v27 >= 0) ) | 
      {                                                                                                              | 
        if ( v25 )                                                                                                   | 
        {                                                                                                            | 
          v20 = a2;                                                                                                  | 
          LOBYTE(v20) = a2 & 1;                                                                                      | 
          LOBYTE(v10) = v28;                                                                                         | 
          v27 = IopIoRingWaitForCompletionEvent(v14, v10, v20, a4);                                                  | 
        }                                                                                                            | 
      }                                                                                                              | 
    }                                                                                                                | 
  }                                                                                                                  | 
LABEL_25:                                                                                                            | 
  if ( v7 )                                                                                                          | 
  {                                                                                                                  | 
    _InterlockedExchange((volatile __int32 *)v6 + 24, 0);                                                            | 
  }                                                                                                                  | 
  if ( v6 )                                                                                                          | 
  {                                                                                                                  | 
    ObfDereferenceObjectWithTag(v6, 0x746C6644u);                                                                    | 
  }                                                                                                                  | 
  return (unsigned int)v27;                                                                                          | 
}                                                                                                                    | 
125 row(s)
