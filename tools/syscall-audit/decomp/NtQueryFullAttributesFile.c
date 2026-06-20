__int64 __fastcall NtQueryFullAttributesFile(__int64 a1, unsigned __int64 a2)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  memset_0(v29, 0, sizeof(v29));
  Handle[0] = 0LL;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  if ( PreviousMode )
  {
    if ( (a2 & 7) != 0 )
    {
      ExRaiseDatatypeMisalignment();
    }
    v5 = 0x7FFFFFFF0000LL;
    if ( a2 < 0x7FFFFFFF0000LL )
    {
      v5 = a2;
    }
    *(_BYTE *)v5 = *(_BYTE *)v5;
    *(_BYTE *)(v5 + 55) = *(_BYTE *)(v5 + 55);
  }
  memset_0(v10, 0, 0xE0uLL);
  v25 = 0LL;
  v26 = 0LL;
  v27 = 0LL;
  v28 = 0LL;
  v10[0] = 14680072;
  v15 = 7;
  v16 = 1;
  v14 = 2113536;
  v18 = 1;
  v19 = 1;
  v20 = v29;
  v13 = a1;
  v21 = 32;
  if ( PreviousMode )
  {
    v17 = &v25;
  }
  else
  {
    v17 = (__int128 *)a2;
  }
  v22 = 0LL;
  v23 = 0LL;
  CurrentSilo = 0LL;
  LOWORD(v22) = 40;
  CurrentSilo = PsGetCurrentSilo();
  CurrentThread = KeGetCurrentThread();
  ++CurrentThread->OtherOperationCount;
  __incgsdword(0x2EE4u);
  v7 = ObOpenObjectByNameEx(
         a1,
         (_DWORD)IoFileObjectType,
         PreviousMode,
         0,
         128,
         (__int64)v10,
         CurrentSilo,
         (__int64)Handle);
  IopCleanupExtraCreateParameters(v10);
  if ( v12 == -1096154543 )
  {
    result = (unsigned int)v11;
    if ( v11 >= 0 && PreviousMode )
    {
      *(_OWORD *)a2 = v25;
      *(_OWORD *)(a2 + 16) = v26;
      *(_OWORD *)(a2 + 32) = v27;
      *(_QWORD *)(a2 + 48) = v28;
    }
  }
  else
  {
    if ( v7 >= 0 )
    {
      ObCloseHandle(Handle[0], PreviousMode);
      return (unsigned int)-1073741788;
    }
    return (unsigned int)v7;
  }
  return result;
}
83 row(s)
