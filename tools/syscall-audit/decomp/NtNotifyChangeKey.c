line
__int64 __fastcall NtNotifyChangeKey(
        int a1,
        int a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        int a6,
        char a7,
        __int64 a8,
        int a9,
        char a10)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  return NtNotifyChangeMultipleKeys(a1, 0, 0, a2, a3, a4, a5, a6, a7, a8, a9, a10);
}
