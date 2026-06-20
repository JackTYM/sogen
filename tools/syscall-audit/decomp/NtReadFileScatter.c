__int64 __fastcall NtReadFileScatter(
        int a1,
        __int64 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        void *Src,
        ULONG Length,
        __int64 a8,
        __int64 a9)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  BugCheckParameter2 = 0LL;
  result = IopReferenceFileObject(a1, 1, KeGetCurrentThread()->PreviousMode, (int)&BugCheckParameter2, 0LL);
  if ( (int)result >= 0 )
  {
    return IopReadFileScatter(
             BugCheckParameter2,
             a5,
             (Length >> 12) + ((Length & 0xFFF) != 0),
             Src,
             Length,
             a8,
             a9,
             0LL,
             0LL);
  }
  return result;
}
30 row(s)
