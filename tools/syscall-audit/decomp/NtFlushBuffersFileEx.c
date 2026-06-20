__int64 __fastcall NtFlushBuffersFileEx(int a1, __int64 a2, __int64 a3, __int64 a4, __int64 a5)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  FileObject = 0LL;
  v6 = 0LL;
  result = IopReferenceFileObject(a1, 0, KeGetCurrentThread()->PreviousMode, (int)&FileObject, &v6);
  if ( (int)result >= 0 )
  {
    return IopFlushBuffersFile(FileObject, v6.GrantedAccess, 0LL, 0LL, a5);
  }
  return result;
}
13 row(s)
