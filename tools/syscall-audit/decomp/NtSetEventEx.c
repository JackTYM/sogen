__int64 __fastcall NtSetEventEx(HANDLE Handle, LONG *a2, __int64 a3)                                                | 
{                                                                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                     | 
                                                                                                                    | 
  v15 = 0LL;                                                                                                        | 
  v16 = 0;                                                                                                          | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                | 
  if ( a2 && PreviousMode )                                                                                         | 
  {                                                                                                                 | 
    v7 = 0x7FFFFFFF0000LL;                                                                                          | 
    if ( (unsigned __int64)a2 < 0x7FFFFFFF0000LL )                                                                  | 
    {                                                                                                               | 
      v7 = (__int64)a2;                                                                                             | 
    }                                                                                                               | 
    *(_DWORD *)v7 = *(_DWORD *)v7;                                                                                  | 
  }                                                                                                                 | 
  SystemArgument1 = 0LL;                                                                                            | 
  v8 = ObReferenceObjectByHandle(Handle, 2u, (POBJECT_TYPE)ExEventObjectType, PreviousMode, &SystemArgument1, 0LL); | 
  v9 = v8;                                                                                                          | 
  v10 = (struct _KEVENT *)SystemArgument1;                                                                          | 
  v14 = SystemArgument1;                                                                                            | 
  if ( v8 < 0 )                                                                                                     | 
  {                                                                                                                 | 
    if ( v8 == -1073741788 )                                                                                        | 
    {                                                                                                               | 
      if ( ExCrossVmEventObjectType )                                                                               | 
      {                                                                                                             | 
        SystemArgument1 = 0LL;                                                                                      | 
        v9 = ObReferenceObjectByHandle(Handle, 2u, ExCrossVmEventObjectType, PreviousMode, &SystemArgument1, 0LL);  | 
        v10 = (struct _KEVENT *)SystemArgument1;                                                                    | 
        v14 = SystemArgument1;                                                                                      | 
        if ( v9 >= 0 )                                                                                              | 
        {                                                                                                           | 
          v9 = ExpSetCrossVmEvent(SystemArgument1, &v16);                                                           | 
        }                                                                                                           | 
      }                                                                                                             | 
    }                                                                                                               | 
    v11 = v16;                                                                                                      | 
  }                                                                                                                 | 
  else                                                                                                              | 
  {                                                                                                                 | 
    if ( (unsigned int)KeAbIsValidUserModeLockAddress(a3) )                                                         | 
    {                                                                                                               | 
      v11 = KeSetEventBoostPriorityEx(SystemArgument1, 0, 1);                                                       | 
    }                                                                                                               | 
    else                                                                                                            | 
    {                                                                                                               | 
      v11 = KeSetEvent(v10, 1, 0);                                                                                  | 
    }                                                                                                               | 
    v16 = v11;                                                                                                      | 
  }                                                                                                                 | 
  if ( v9 >= 0 && a2 )                                                                                              | 
  {                                                                                                                 | 
    *a2 = v11;                                                                                                      | 
  }                                                                                                                 | 
  if ( v10 )                                                                                                        | 
  {                                                                                                                 | 
    ObfDereferenceObject(v10);                                                                                      | 
  }                                                                                                                 | 
  return (unsigned int)v9;                                                                                          | 
}                                                                                                                   | 
61 row(s)
