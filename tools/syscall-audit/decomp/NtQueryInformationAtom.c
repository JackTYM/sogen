__int64 __fastcall NtQueryInformationAtom(unsigned __int16 a1, int a2, _WORD *a3, unsigned int a4, int *a5) | 
{                                                                                                           | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                             | 
                                                                                                            | 
  v5 = a4;                                                                                                  | 
  v20 = 0;                                                                                                  | 
  v18 = 0;                                                                                                  | 
  v17 = 0;                                                                                                  | 
  v21 = 0LL;                                                                                                | 
  *(_QWORD *)v22 = 0LL;                                                                                     | 
  v24 = 0;                                                                                                  | 
  v23 = 0LL;                                                                                                | 
  if ( (unsigned int)PspUpdateCalloutParameters(2LL, &v21, 0LL, 0LL) )                                      | 
  {                                                                                                         | 
    Win32Callouts = PsSessionGetWin32Callouts();                                                            | 
    ExCallCallBack(Win32Callouts, v10, &v21);                                                               | 
  }                                                                                                         | 
  v11 = *(_QWORD *)v22;                                                                                     | 
  if ( !*(_QWORD *)v22 )                                                                                    | 
  {                                                                                                         | 
    return 3221225506LL;                                                                                    | 
  }                                                                                                         | 
  CurrentThread = KeGetCurrentThread();                                                                     | 
  if ( CurrentThread->PreviousMode )                                                                        | 
  {                                                                                                         | 
    ProbeForWrite(a3, v5, 4u);                                                                              | 
    if ( a5 )                                                                                               | 
    {                                                                                                       | 
      v12 = 0x7FFFFFFF0000LL;                                                                               | 
      if ( (unsigned __int64)a5 < 0x7FFFFFFF0000LL )                                                        | 
      {                                                                                                     | 
        v12 = (__int64)a5;                                                                                  | 
      }                                                                                                     | 
      *(_DWORD *)v12 = *(_DWORD *)v12;                                                                      | 
    }                                                                                                       | 
  }                                                                                                         | 
  v13 = 0;                                                                                                  | 
  if ( a2 )                                                                                                 | 
  {                                                                                                         | 
    if ( a2 == 1 )                                                                                          | 
    {                                                                                                       | 
      v13 = 4;                                                                                              | 
      if ( (unsigned int)v5 < 4 )                                                                           | 
      {                                                                                                     | 
        return 3221225476LL;                                                                                | 
      }                                                                                                     | 
      AtomsInAtomTable = RtlQueryAtomsInAtomTable(v11, (unsigned int)(v5 - 4) >> 1, a3, a3 + 2);            | 
      v19 = AtomsInAtomTable;                                                                               | 
      if ( AtomsInAtomTable >= 0 )                                                                          | 
      {                                                                                                     | 
        v13 = 2 * *(_DWORD *)a3 + 4;                                                                        | 
      }                                                                                                     | 
    }                                                                                                       | 
    else                                                                                                    | 
    {                                                                                                       | 
      AtomsInAtomTable = -1073741821;                                                                       | 
      v19 = -1073741821;                                                                                    | 
    }                                                                                                       | 
  }                                                                                                         | 
  else                                                                                                      | 
  {                                                                                                         | 
    v13 = 6;                                                                                                | 
    if ( (unsigned int)v5 < 6 )                                                                             | 
    {                                                                                                       | 
      return 3221225476LL;                                                                                  | 
    }                                                                                                       | 
    v17 = 0;                                                                                                | 
    v18 = v5 - 6;                                                                                           | 
    AtomsInAtomTable = RtlQueryAtomInAtomTable(v11, a1, (int)&v17, (int)&v20, a3 + 3, (__int64)&v18);       | 
    v19 = AtomsInAtomTable;                                                                                 | 
    if ( AtomsInAtomTable >= 0 )                                                                            | 
    {                                                                                                       | 
      *a3 = v17;                                                                                            | 
      a3[1] = v20;                                                                                          | 
      v15 = v18;                                                                                            | 
      a3[2] = v18;                                                                                          | 
      v13 = v15 + 8;                                                                                        | 
    }                                                                                                       | 
  }                                                                                                         | 
  if ( a5 )                                                                                                 | 
  {                                                                                                         | 
    *a5 = v13;                                                                                              | 
  }                                                                                                         | 
  return (unsigned int)AtomsInAtomTable;                                                                    | 
}                                                                                                           | 
85 row(s)
