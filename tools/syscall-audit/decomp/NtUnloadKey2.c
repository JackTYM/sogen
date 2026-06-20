line
__int64 NtUnloadKey2()
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v7 = 0LL;
  v6 = 0LL;
  CmpInitializeThreadInfo(&v6);
  v2 = CmUnloadKey(v1, v0, 0LL, 0LL);
  CmpCleanupThreadInfo(&v6, v3, v2);
  return v4;
}
