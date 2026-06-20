__int64 __fastcall NtCreateWaitCompletionPacket(__int64 a1, __int64 a2, int a3)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v3 = (_QWORD *)a1;
  v9 = 0LL;
  Object = 0LL;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  if ( PreviousMode )
  {
    a1 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)v3 < 0x7FFFFFFF0000LL )
    {
      a1 = (__int64)v3;
    }
    *(_QWORD *)a1 = *(_QWORD *)a1;
  }
  LOBYTE(a1) = PreviousMode;
  v5 = ObCreateObjectEx(a1, (_DWORD)IopWaitCompletionPacketObjectType, a3, PreviousMode);
  inserted = v5;
  if ( v5 >= 0 )
  {
    v7 = Object;
    *((_QWORD *)Object + 12) = 0LL;
    v7[104] = 0;
    if ( (unsigned int)Feature_1806129466__private_IsEnabledDeviceUsageNoInline((unsigned int)v5) )
    {
      v7[105] = 0;
    }
    *((_QWORD *)v7 + 11) = 0LL;
    inserted = ObInsertObjectEx(v7, 0, 0LL, (__int64)&v9);
    if ( inserted >= 0 )
    {
      *v3 = v9;
    }
  }
  return (unsigned int)inserted;
}
