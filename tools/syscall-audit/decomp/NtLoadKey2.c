line
__int64 __fastcall NtLoadKey2(int a1, int a2, int a3)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  return CmLoadDifferencingKey(a1, a2, a3, 0, 0LL, 0LL, 0, 0LL, 0, 0LL, 0, 0LL, KeGetCurrentThread()->PreviousMode);
}
