__int64 __fastcall NtQueryDirectoryFileEx(
        void *a1,
        __int64 a2,
        __int64 a3,
        __int64 a4,
        __int64 a5,
        volatile void *a6,
        SIZE_T a7,
        int a8,
        char a9,
        __int64 a10)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v16 = 0;
  v20 = 0LL;
  v19 = 0LL;
  v18 = 0LL;
  v17[0] = 0;
  LODWORD(Length) = a7;
  result = BuildQueryDirectoryIrp(
             a1,
             a5,
             a6,
             Length,
             a8,
             a9,
             a10,
             v15,
             (__int64)&v16,
             (__int64)&v20,
             (__int64)&v19,
             (__int64)&v18,
             (__int64)v17);
  if ( !(_DWORD)result )
  {
    LOBYTE(v11) = 1;
    LOBYTE(v13) = v16;
    LOBYTE(v12) = v17[0];
    return IopSynchronousServiceTail(v20, v19, v18, v11, v12, v13, 2);
  }
  return result;
}
43 row(s)
