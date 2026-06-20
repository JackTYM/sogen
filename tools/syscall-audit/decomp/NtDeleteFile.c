__int64 __fastcall NtDeleteFile(__int64 a1)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  memset_0(v19, 0, sizeof(v19));
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  memset_0(v8, 0, 0xE0uLL);
  v8[0] = 14680072;
  v10 = 4096;
  v11 = 7;
  v14 = v19;
  CurrentSilo = 0LL;
  v16 = 0LL;
  LOWORD(v16) = 40;
  v12 = 1;
  v13 = 1;
  v9 = a1;
  v15 = 32;
  v17 = 0LL;
  CurrentSilo = PsGetCurrentSilo();
  CurrentThread = KeGetCurrentThread();
  LOBYTE(v4) = PreviousMode;
  ++CurrentThread->OtherOperationCount;
  __incgsdword(0x2EE4u);
  v5 = ObOpenObjectByNameEx(a1, (_DWORD)IoFileObjectType, v4, 0, 0x10000, (__int64)v8, CurrentSilo, (__int64)v7);
  IopCleanupExtraCreateParameters(v8);
  result = v8[4];
  if ( v8[8] != -1096154543 )
  {
    return v5;
  }
  return result;
}
33 row(s)
