line
__int64 NtUnloadKey()
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v6 = 0LL;
  v5 = 0LL;
  CmpInitializeThreadInfo(&v5);
  v1 = CmUnloadKey(v0, 0LL, 0LL, 0LL);
  CmpCleanupThreadInfo(&v5, v2, v1);
  return v3;
}
