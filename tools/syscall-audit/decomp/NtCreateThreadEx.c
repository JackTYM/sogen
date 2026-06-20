__int64 __fastcall NtCreateThreadEx(                                                                        | 
        unsigned __int64 a1,                                                                                | 
        int a2,                                                                                             | 
        __int64 a3,                                                                                         | 
        ULONG_PTR a4,                                                                                       | 
        __int64 a5,                                                                                         | 
        __int64 a6,                                                                                         | 
        int a7,                                                                                             | 
        __int64 a8,                                                                                         | 
        __int64 a9,                                                                                         | 
        __int64 a10,                                                                                        | 
        __int64 a11)                                                                                        | 
{                                                                                                           | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                             | 
                                                                                                            | 
  v39 = a3;                                                                                                 | 
  LODWORD(v33) = a2;                                                                                        | 
  v35 = a5;                                                                                                 | 
  v34 = a6;                                                                                                 | 
  Object = 0LL;                                                                                             | 
  v36 = 0LL;                                                                                                | 
  v31 = 0;                                                                                                  | 
  v13 = 0LL;                                                                                                | 
  v14 = 0LL;                                                                                                | 
  v15 = 0LL;                                                                                                | 
  if ( (a7 & 0xFFFFFF80) != 0 )                                                                             | 
  {                                                                                                         | 
    return 3221225717LL;                                                                                    | 
  }                                                                                                         | 
  if ( KeGetCurrentThread()->PreviousMode )                                                                 | 
  {                                                                                                         | 
    v16 = 0x7FFFFFFF0000LL;                                                                                 | 
    if ( a1 < 0x7FFFFFFF0000LL )                                                                            | 
    {                                                                                                       | 
      v16 = a1;                                                                                             | 
    }                                                                                                       | 
    *(_QWORD *)v16 = *(_QWORD *)v16;                                                                        | 
  }                                                                                                         | 
  memset_0(v40, 0, 0x210uLL);                                                                               | 
  if ( !a11                                                                                                 | 
    || (LOBYTE(v17) = KeGetCurrentThread()->PreviousMode,                                                   | 
        result = PspBuildCreateProcessContext(a11, v17, 1LL, v40),                                          | 
        (int)result >= 0) )                                                                                 | 
  {                                                                                                         | 
    result = ObpReferenceObjectByHandleWithTag(a4, 0x72437350u, (__int64)&Object, 0LL, 0LL);                | 
    if ( (int)result >= 0 )                                                                                 | 
    {                                                                                                       | 
      v19 = Object;                                                                                         | 
      if ( (*((_BYTE *)Object + 368) & 1) == 0 )                                                            | 
      {                                                                                                     | 
        v14 = &v27;                                                                                         | 
        v27 = 0;                                                                                            | 
        v29 = a9;                                                                                           | 
        v30 = a10;                                                                                          | 
        v28 = a8;                                                                                           | 
        v38 = v24;                                                                                          | 
        memset_0(v24, 0, 0x48uLL);                                                                          | 
        v20 = (v19[469] & 0x4000) != 0 ? 0x800 : 0;                                                         | 
        LODWORD(Object) = (v19[469] & 0x4000) != 0 ? 1048651 : 1048587;                                     | 
        v37 = qword_140FC64C0;                                                                              | 
        RtlGetExtendedContextLength2((unsigned int)Object, &v31, (unsigned int)v20);                        | 
        v21 = v31 + 15LL;                                                                                   | 
        if ( v21 <= v31 )                                                                                   | 
        {                                                                                                   | 
          v21 = 0xFFFFFFFFFFFFFF0LL;                                                                        | 
        }                                                                                                   | 
        v22 = alloca(v21 & 0xFFFFFFFFFFFFFFF0uLL);                                                          | 
        v15 = v24;                                                                                          | 
        memset_0(v24, 0, v31);                                                                              | 
        RtlInitializeExtendedContext2(v24, (unsigned int)Object, &v36, v20);                                | 
        v42 = v37;                                                                                          | 
        v33 = v35;                                                                                          | 
        *(_QWORD *)&v26[1] = 0x2B0053002B002BLL;                                                            | 
        v26[5] = 43;                                                                                        | 
        strcpy((char *)v26, "3");                                                                           | 
        HIDWORD(v25) = 8064;                                                                                | 
        v43 = 639;                                                                                          | 
        v44 = 8064;                                                                                         | 
        v13 = v38;                                                                                          | 
      }                                                                                                     | 
      ObfDereferenceObjectWithTag(v19, 0x72437350u);                                                        | 
      Thread = PspCreateThread(a1, (unsigned int)v33, v39, a4, 0LL, v40, v41, v15, v13, a7, v35, v34, v14); | 
      PspDeleteCreateProcessContext(v40);                                                                   | 
      return Thread;                                                                                        | 
    }                                                                                                       | 
  }                                                                                                         | 
  return result;                                                                                            | 
}                                                                                                           | 
