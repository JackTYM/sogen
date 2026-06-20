line
__int64 __fastcall NtLoadKeyEx(
        int a1,
        int a2,
        int a3,
        __int64 a4,
        __int64 a5,
        ACCESS_MASK DesiredAccess,
        __int64 a7,
        __int64 a8)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  return CmLoadDifferencingKey(
           a1,
           a2,
           a3,
           0,
           a4,
           a5,
           DesiredAccess,
           a7,
           a8,
           0LL,
           0,
           0LL,
           KeGetCurrentThread()->PreviousMode);
}
