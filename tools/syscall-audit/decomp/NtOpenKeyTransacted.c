line
__int64 __fastcall NtOpenKeyTransacted(int a1, int a2, int a3, void *Handle)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  return NtOpenKeyTransactedEx(a1, a2, a3, 0, Handle);
}
