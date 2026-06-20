__int64 __fastcall NtIsProcessInJob(ULONG_PTR a1, void *a2)                                    | 
{                                                                                              | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                | 
                                                                                               | 
  CurrentThread = KeGetCurrentThread();                                                        | 
  Object = 0LL;                                                                                | 
  PreviousMode = CurrentThread->PreviousMode;                                                  | 
  if ( a1 == -1LL )                                                                            | 
  {                                                                                            | 
    p_Lock = &CurrentThread->ApcState.Process->Header.Lock;                                    | 
    Object = p_Lock;                                                                           | 
  }                                                                                            | 
  else                                                                                         | 
  {                                                                                            | 
    result = ObpReferenceObjectByHandleWithTag(a1, 0x624A7350u, (__int64)&Object, 0LL, 0LL);   | 
    if ( (int)result < 0 )                                                                     | 
    {                                                                                          | 
      return result;                                                                           | 
    }                                                                                          | 
    p_Lock = Object;                                                                           | 
  }                                                                                            | 
  if ( !a2 )                                                                                   | 
  {                                                                                            | 
    v9 = (PVOID)p_Lock[84];                                                                    | 
    p_Lock = Object;                                                                           | 
LABEL_6:                                                                                       | 
    IsProcessInJob = PspIsProcessInJob(p_Lock, v9);                                            | 
    if ( a2 )                                                                                  | 
    {                                                                                          | 
      ObfDereferenceObject(v11);                                                               | 
    }                                                                                          | 
    goto LABEL_8;                                                                              | 
  }                                                                                            | 
  Object = 0LL;                                                                                | 
  v8 = ObReferenceObjectByHandle(a2, 4u, (POBJECT_TYPE)PsJobType, PreviousMode, &Object, 0LL); | 
  v9 = Object;                                                                                 | 
  IsProcessInJob = v8;                                                                         | 
  if ( v8 >= 0 )                                                                               | 
  {                                                                                            | 
    goto LABEL_6;                                                                              | 
  }                                                                                            | 
LABEL_8:                                                                                       | 
  if ( a1 != -1LL )                                                                            | 
  {                                                                                            | 
    ObfDereferenceObjectWithTag(p_Lock, 0x624A7350u);                                          | 
  }                                                                                            | 
  return IsProcessInJob;                                                                       | 
}                                                                                              | 
48 row(s)
