__int64 __fastcall NtAllocateReserveObject(__int64 *a1, int a2, signed int a3)                                       | 
{                                                                                                                    | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                      | 
                                                                                                                     | 
  v3 = a3;                                                                                                           | 
  v11 = 0LL;                                                                                                         | 
  Object = 0LL;                                                                                                      | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                 | 
  if ( PreviousMode )                                                                                                | 
  {                                                                                                                  | 
    v10 = 0x7FFFFFFF0000LL;                                                                                          | 
    if ( (unsigned __int64)a1 < 0x7FFFFFFF0000LL )                                                                   | 
    {                                                                                                                | 
      v10 = (__int64)a1;                                                                                             | 
    }                                                                                                                | 
    *(_QWORD *)v10 = *(_QWORD *)v10;                                                                                 | 
  }                                                                                                                  | 
  if ( (unsigned int)a3 > 1 )                                                                                        | 
  {                                                                                                                  | 
    return 3221225485LL;                                                                                             | 
  }                                                                                                                  | 
  p_Object = &Object;                                                                                                | 
  LOBYTE(p_Object) = PreviousMode;                                                                                   | 
  result = ObCreateObjectEx((_DWORD)p_Object, (unsigned int)*(&PspMemoryReserveObjectTypes + a3), a2, PreviousMode); | 
  if ( (int)result >= 0 )                                                                                            | 
  {                                                                                                                  | 
    v8 = Object;                                                                                                     | 
    memset_0(Object, 0, PspMemoryReserveObjectSizes[v3]);                                                            | 
    if ( (_DWORD)v3 == 1 )                                                                                           | 
    {                                                                                                                | 
      v8[6] = 4;                                                                                                     | 
      *((_QWORD *)v8 + 8) = &PspIoMiniPacketCallbackRoutine;                                                         | 
      *((_QWORD *)v8 + 9) = v8;                                                                                      | 
      *((_BYTE *)v8 + 80) = 0;                                                                                       | 
    }                                                                                                                | 
    inserted = ObInsertObjectEx(v8, 0, 0LL, (__int64)&v11);                                                          | 
    LODWORD(Object) = inserted;                                                                                      | 
    if ( inserted >= 0 )                                                                                             | 
    {                                                                                                                | 
      *a1 = v11;                                                                                                     | 
    }                                                                                                                | 
    return (unsigned int)inserted;                                                                                   | 
  }                                                                                                                  | 
  return result;                                                                                                     | 
}                                                                                                                    | 
45 row(s)
