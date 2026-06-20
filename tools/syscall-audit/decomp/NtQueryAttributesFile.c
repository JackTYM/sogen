__int64 __fastcall NtQueryAttributesFile(__int64 a1, unsigned __int64 a2)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  memset_0(v25, 0, sizeof(v25));
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
    *(_BYTE *)(v5 + 39) = *(_BYTE *)(v5 + 39);
  }
  memset_0(v10, 0, 0xE0uLL);
  memset(v23, 0, sizeof(v23));
  v24 = 0LL;
  v10[0] = 14680072;
  v13 = 7;
  v14 = 1;
  v12 = 2113536;
  v15 = a2;
  v16 = v23;
  v17 = 1;
  v18 = v25;
  v11 = a1;
  v19 = 32;
  v20 = 0LL;
  v21 = 0LL;
  CurrentSilo = 0LL;
  LOWORD(v20) = 40;
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
  if ( *((_QWORD *)&v20 + 1) && (unsigned __int8)FsRtlpCleanupEcps() )
  {
    *((_QWORD *)&v20 + 1) = 0LL;
  }
  if ( v10[8] == -1096154543 )
  {
    return v10[4];
  }
  if ( v7 >= 0 )
  {
    ObCloseHandle(Handle[0], PreviousMode);
    return (unsigned int)-1073741788;
  }
  return (unsigned int)v7;
}
66 row(s)
