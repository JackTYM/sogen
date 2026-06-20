__int64 __fastcall NtNotifyChangeDirectoryFile(                                 | 
        int a1,                                                                 | 
        void *a2,                                                               | 
        __int64 a3,                                                             | 
        __int64 a4,                                                             | 
        __int64 a5,                                                             | 
        volatile void *Address,                                                 | 
        SIZE_T a7,                                                              | 
        int a8,                                                                 | 
        char a9)                                                                | 
{                                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                 | 
                                                                                | 
  LODWORD(Length) = a7;                                                         | 
  return NtNotifyChangeDirectoryFileEx(a1, a2, a5, Address, Length, a8, a9, 1); | 
}                                                                               | 
16 row(s)
