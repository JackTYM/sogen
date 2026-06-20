__int64 __fastcall NtFlushBuffersFile(int a1, __int64 a2)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
  return NtFlushBuffersFileEx(a1, 0, 0, 0, a2);
}
