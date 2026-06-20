__int64 __fastcall NtCreateTokenEx(                                                                              | 
        _QWORD *a1,                                                                                              | 
        int a2,                                                                                                  | 
        __int64 a3,                                                                                              | 
        int a4,                                                                                                  | 
        __int64 *a5,                                                                                             | 
        __int64 *a6,                                                                                             | 
        __int64 a7,                                                                                              | 
        unsigned int *a8,                                                                                        | 
        int *a9,                                                                                                 | 
        unsigned __int64 a10,                                                                                    | 
        __int64 a11,                                                                                             | 
        int *a12,                                                                                                | 
        int *a13,                                                                                                | 
        __int64 a14,                                                                                             | 
        void **a15,                                                                                              | 
        void **a16,                                                                                              | 
        __int64 a17)                                                                                             | 
{                                                                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                  | 
                                                                                                                 | 
  v42 = a4;                                                                                                      | 
  *(_QWORD *)v64 = a3;                                                                                           | 
  v55 = a2;                                                                                                      | 
  v66 = a1;                                                                                                      | 
  v71 = a13;                                                                                                     | 
  v56 = a5;                                                                                                      | 
  v60 = a15;                                                                                                     | 
  v17 = (void **)a14;                                                                                            | 
  v70 = a14;                                                                                                     | 
  v61 = a17;                                                                                                     | 
  v59 = a9;                                                                                                      | 
  v58 = a8;                                                                                                      | 
  v57 = a6;                                                                                                      | 
  v69 = a7;                                                                                                      | 
  *(_QWORD *)v65 = 0LL;                                                                                          | 
  v32 = 0;                                                                                                       | 
  v67 = 0LL;                                                                                                     | 
  v68 = 0;                                                                                                       | 
  v63 = 0LL;                                                                                                     | 
  v62 = 0LL;                                                                                                     | 
  v48 = 0LL;                                                                                                     | 
  v52 = 0;                                                                                                       | 
  v41 = 0;                                                                                                       | 
  v49 = 0LL;                                                                                                     | 
  v36 = 0;                                                                                                       | 
  v40 = 0;                                                                                                       | 
  v50 = 0LL;                                                                                                     | 
  v53 = 0LL;                                                                                                     | 
  v51 = 0LL;                                                                                                     | 
  P = 0LL;                                                                                                       | 
  v44 = 0LL;                                                                                                     | 
  *(_OWORD *)v72 = 0LL;                                                                                          | 
  v38 = 0;                                                                                                       | 
  v33 = 0;                                                                                                       | 
  v46 = 0LL;                                                                                                     | 
  v47 = 0LL;                                                                                                     | 
  v37 = 1;                                                                                                       | 
  v39 = 0;                                                                                                       | 
  v45 = 0LL;                                                                                                     | 
  v54 = 0;                                                                                                       | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                             | 
  v34 = PreviousMode;                                                                                            | 
  if ( !SeSinglePrivilegeCheck(SeCreateTokenPrivilege, PreviousMode) )                                           | 
  {                                                                                                              | 
    return 3221225569LL;                                                                                         | 
  }                                                                                                              | 
  if ( PreviousMode )                                                                                            | 
  {                                                                                                              | 
    v19 = 0x7FFFFFFF0000LL;                                                                                      | 
    v20 = 0x7FFFFFFF0000LL;                                                                                      | 
    if ( (unsigned __int64)v66 < 0x7FFFFFFF0000LL )                                                              | 
    {                                                                                                            | 
      v20 = (__int64)v66;                                                                                        | 
    }                                                                                                            | 
    *(_QWORD *)v20 = *(_QWORD *)v20;                                                                             | 
    if ( ((unsigned __int8)v57 & 3) == 0                                                                         | 
      && ((unsigned __int8)v58 & 3) == 0                                                                         | 
      && ((unsigned __int8)v59 & 3) == 0                                                                         | 
      && (v61 & 3) == 0                                                                                          | 
      && (!a14 || (a14 & 3) == 0)                                                                                | 
      && ((unsigned __int8)v60 & 3) == 0                                                                         | 
      && (!a16 || ((unsigned __int8)a16 & 3) == 0)                                                               | 
      && ((unsigned __int8)v56 & 3) == 0                                                                         | 
      && (!a13 || ((unsigned __int8)a13 & 3) == 0)                                                               | 
      && (!a12 || ((unsigned __int8)a12 & 3) == 0)                                                               | 
      && (!a11 || (a11 & 3) == 0) )                                                                              | 
    {                                                                                                            | 
      if ( !a10 )                                                                                                | 
      {                                                                                                          | 
        goto LABEL_31;                                                                                           | 
      }                                                                                                          | 
      if ( (a10 & 3) == 0 )                                                                                      | 
      {                                                                                                          | 
        if ( a10 < 0x7FFFFFFF0000LL )                                                                            | 
        {                                                                                                        | 
          v19 = a10;                                                                                             | 
        }                                                                                                        | 
        goto LABEL_31;                                                                                           | 
      }                                                                                                          | 
    }                                                                                                            | 
    ExRaiseDatatypeMisalignment();                                                                               | 
  }                                                                                                              | 
LABEL_31:                                                                                                        | 
  v21 = v42;                                                                                                     | 
  if ( (unsigned int)(v42 - 1) > 1 )                                                                             | 
  {                                                                                                              | 
    return 3221225640LL;                                                                                         | 
  }                                                                                                              | 
  LOBYTE(v19) = PreviousMode;                                                                                    | 
  result = SeCaptureSecurityQos(*(_QWORD *)v64, v19, &v32, &v67);                                                | 
  if ( (int)result >= 0 )                                                                                        | 
  {                                                                                                              | 
    if ( v21 != 2 || v32 )                                                                                       | 
    {                                                                                                            | 
      v35 = 0;                                                                                                   | 
      v63 = *v56;                                                                                                | 
      v62 = *v57;                                                                                                | 
      v29 = 0;                                                                                                   | 
      LOBYTE(v23) = PreviousMode;                                                                                | 
      Token = SeCaptureSidAndAttributesArray(v69, 1LL, v23);                                                     | 
      v35 = Token;                                                                                               | 
      if ( Token >= 0 )                                                                                          | 
      {                                                                                                          | 
        v28 = *v58;                                                                                              | 
        v41 = v28;                                                                                               | 
        v29 = 0;                                                                                                 | 
        LOBYTE(v26) = PreviousMode;                                                                              | 
        Token = SeCaptureSidAndAttributesArray(v58 + 2, v28, v26);                                               | 
        v35 = Token;                                                                                             | 
        v36 = (v36 - 16 * v28 + 3) & 0xFFFFFFFC;                                                                 | 
        v17 = (void **)v70;                                                                                      | 
      }                                                                                                          | 
      if ( Token >= 0 )                                                                                          | 
      {                                                                                                          | 
        v40 = *v59;                                                                                              | 
        Token = SeCaptureLuidAndAttributesArray(v59 + 1, 0, v30, v31, (__int64)&v50, (__int64)&v53);             | 
        v35 = Token;                                                                                             | 
      }                                                                                                          | 
      if ( v17 && Token >= 0 )                                                                                   | 
      {                                                                                                          | 
        Token = SeCaptureSid(*v17, v29, 1, (__int64)&v51);                                                       | 
        v35 = Token;                                                                                             | 
      }                                                                                                          | 
      if ( Token >= 0 )                                                                                          | 
      {                                                                                                          | 
        Token = SeCaptureSid(*v60, v29, 1, (__int64)&P);                                                         | 
        v35 = Token;                                                                                             | 
      }                                                                                                          | 
      if ( a16 && Token >= 0 && *a16 )                                                                           | 
      {                                                                                                          | 
        Token = SeCaptureAcl(*a16, 512, v30, (__int64)&v44, (__int64)&v53 + 4);                                  | 
        v35 = Token;                                                                                             | 
      }                                                                                                          | 
      *(_OWORD *)v72 = *(_OWORD *)v61;                                                                           | 
      if ( a12 && Token >= 0 )                                                                                   | 
      {                                                                                                          | 
        v39 = *a12;                                                                                              | 
        LOBYTE(v26) = PreviousMode;                                                                              | 
        Token = SeCaptureSidAndAttributesArray(a12 + 2, (unsigned int)v39, v26);                                 | 
        v35 = Token;                                                                                             | 
      }                                                                                                          | 
      if ( a10 && Token >= 0 )                                                                                   | 
      {                                                                                                          | 
        LOBYTE(v27) = PreviousMode;                                                                              | 
        Token = SepCaptureTokenSecurityAttributesInformation(a10, (unsigned int)&v37, 1, v27, 0, (__int64)&v46); | 
        v35 = Token;                                                                                             | 
      }                                                                                                          | 
      if ( a11 && Token >= 0 )                                                                                   | 
      {                                                                                                          | 
        LOBYTE(v27) = PreviousMode;                                                                              | 
        Token = SepCaptureTokenSecurityAttributesInformation(a11, (unsigned int)&v37, 1, v27, 0, (__int64)&v47); | 
        v35 = Token;                                                                                             | 
      }                                                                                                          | 
      if ( v71 && Token >= 0 )                                                                                   | 
      {                                                                                                          | 
        v38 = *v71;                                                                                              | 
        v33 = 1;                                                                                                 | 
      }                                                                                                          | 
      if ( Token >= 0 )                                                                                          | 
      {                                                                                                          | 
        LOBYTE(v24) = PreviousMode;                                                                              | 
        Token = SepCreateTokenEx(                                                                                | 
                  (int)v65,                                                                                      | 
                  v24,                                                                                           | 
                  v55,                                                                                           | 
                  v64[0],                                                                                        | 
                  v42,                                                                                           | 
                  SHIDWORD(v67),                                                                                 | 
                  (__int64)&v63,                                                                                 | 
                  (__int64)&v62,                                                                                 | 
                  v48,                                                                                           | 
                  v41,                                                                                           | 
                  v49,                                                                                           | 
                  v36,                                                                                           | 
                  v40,                                                                                           | 
                  v50,                                                                                           | 
                  v51,                                                                                           | 
                  P,                                                                                             | 
                  v44,                                                                                           | 
                  (__int64)v72,                                                                                  | 
                  (__int64)v46,                                                                                  | 
                  (__int64)v47,                                                                                  | 
                  v39,                                                                                           | 
                  v45,                                                                                           | 
                  (unsigned __int64)&v38 & -(__int64)(v33 != 0),                                                 | 
                  0);                                                                                            | 
      }                                                                                                          | 
      if ( v48 )                                                                                                 | 
      {                                                                                                          | 
        LOBYTE(v24) = PreviousMode;                                                                              | 
        SeReleaseLuidAndAttributesArray(v48, v24);                                                               | 
      }                                                                                                          | 
      if ( v49 )                                                                                                 | 
      {                                                                                                          | 
        LOBYTE(v24) = PreviousMode;                                                                              | 
        SeReleaseLuidAndAttributesArray(v49, v24);                                                               | 
      }                                                                                                          | 
      if ( v50 )                                                                                                 | 
      {                                                                                                          | 
        LOBYTE(v24) = PreviousMode;                                                                              | 
        SeReleaseLuidAndAttributesArray(v50, v24);                                                               | 
      }                                                                                                          | 
      if ( v51 && (unsigned __int8)PreviousMode <= 1u )                                                          | 
      {                                                                                                          | 
        ExFreePoolWithTag(v51, 0);                                                                               | 
      }                                                                                                          | 
      if ( P && (unsigned __int8)PreviousMode <= 1u )                                                            | 
      {                                                                                                          | 
        ExFreePoolWithTag(P, 0);                                                                                 | 
      }                                                                                                          | 
      if ( v44 )                                                                                                 | 
      {                                                                                                          | 
        LOBYTE(v24) = PreviousMode;                                                                              | 
        SeReleaseAcl(v44, v24);                                                                                  | 
      }                                                                                                          | 
      if ( v45 )                                                                                                 | 
      {                                                                                                          | 
        LOBYTE(v24) = PreviousMode;                                                                              | 
        SeReleaseLuidAndAttributesArray(v45, v24);                                                               | 
      }                                                                                                          | 
      if ( v46 )                                                                                                 | 
      {                                                                                                          | 
        SepFreeCapturedTokenSecurityAttributesInformation(v46);                                                  | 
      }                                                                                                          | 
      if ( v47 )                                                                                                 | 
      {                                                                                                          | 
        SepFreeCapturedTokenSecurityAttributesInformation(v47);                                                  | 
      }                                                                                                          | 
      if ( Token >= 0 )                                                                                          | 
      {                                                                                                          | 
        *v66 = *(_QWORD *)v65;                                                                                   | 
      }                                                                                                          | 
      return (unsigned int)Token;                                                                                | 
    }                                                                                                            | 
    else                                                                                                         | 
    {                                                                                                            | 
      return 3221225637LL;                                                                                       | 
    }                                                                                                            | 
  }                                                                                                              | 
  return result;                                                                                                 | 
}                                                                                                                | 
262 row(s)
