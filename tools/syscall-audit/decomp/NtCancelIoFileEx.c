__int64 __fastcall NtCancelIoFileEx(int a1, __int64 a2, unsigned __int64 a3)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v3 = a3;
  Object = 0LL;
  LOBYTE(a3) = KeGetCurrentThread()->PreviousMode;
  if ( (_BYTE)a3 )
  {
    v5 = 0x7FFFFFFF0000LL;
    if ( v3 < 0x7FFFFFFF0000LL )
    {
      v5 = v3;
    }
    *(_DWORD *)v5 = *(_DWORD *)v5;
  }
  result = IopReferenceFileObject(a1, 0, a3, (int)&Object, 0LL);
  if ( (int)result >= 0 )
  {
    v7 = Object;
    v9 = IopCancelIoFile(Object, a2);
    *(_DWORD *)v3 = v9;
    *(_QWORD *)(v3 + 8) = 0LL;
    ObfDereferenceObject(v7);
    return v9;
  }
  return result;
}
28 row(s)
