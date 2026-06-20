__int64 __fastcall NtCreateThread(                                                                | 
        _QWORD *a1,                                                                               | 
        unsigned int a2,                                                                          | 
        __int64 a3,                                                                               | 
        __int64 a4,                                                                               | 
        unsigned __int64 a5,                                                                      | 
        __int64 a6,                                                                               | 
        unsigned __int64 a7,                                                                      | 
        char a8)                                                                                  | 
{                                                                                                 | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                   | 
                                                                                                  | 
  v12 = (_OWORD *)a6;                                                                             | 
  memset_0(&v26, 0, 0x48uLL);                                                                     | 
  v34 = 0LL;                                                                                      | 
  v35 = 0LL;                                                                                      | 
  if ( !a6 )                                                                                      | 
  {                                                                                               | 
    return 3221225485LL;                                                                          | 
  }                                                                                               | 
  CurrentThread = KeGetCurrentThread();                                                           | 
  LOBYTE(v13) = CurrentThread->PreviousMode;                                                      | 
  v24 = v13;                                                                                      | 
  if ( (_BYTE)v13 )                                                                               | 
  {                                                                                               | 
    v13 = 0x7FFFFFFF0000LL;                                                                       | 
    v16 = 0x7FFFFFFF0000LL;                                                                       | 
    if ( (unsigned __int64)a1 < 0x7FFFFFFF0000LL )                                                | 
    {                                                                                             | 
      v16 = (__int64)a1;                                                                          | 
    }                                                                                             | 
    *(_QWORD *)v16 = *(_QWORD *)v16;                                                              | 
    if ( a5 )                                                                                     | 
    {                                                                                             | 
      if ( (a5 & 3) != 0 )                                                                        | 
      {                                                                                           | 
        goto LABEL_18;                                                                            | 
      }                                                                                           | 
      v17 = 0x7FFFFFFF0000LL;                                                                     | 
      if ( a5 < 0x7FFFFFFF0000LL )                                                                | 
      {                                                                                           | 
        v17 = a5;                                                                                 | 
      }                                                                                           | 
      *(_BYTE *)v17 = *(_BYTE *)v17;                                                              | 
      *(_BYTE *)(v17 + 15) = *(_BYTE *)(v17 + 15);                                                | 
    }                                                                                             | 
    if ( (a6 & 0xF) == 0 )                                                                        | 
    {                                                                                             | 
      v18 = v36;                                                                                  | 
      v19 = 9LL;                                                                                  | 
      v14 = 128LL;                                                                                | 
      do                                                                                          | 
      {                                                                                           | 
        *v18 = *v12;                                                                              | 
        v18[1] = v12[1];                                                                          | 
        v18[2] = v12[2];                                                                          | 
        v18[3] = v12[3];                                                                          | 
        v18[4] = v12[4];                                                                          | 
        v18[5] = v12[5];                                                                          | 
        v18[6] = v12[6];                                                                          | 
        v18 += 8;                                                                                 | 
        *(v18 - 1) = v12[7];                                                                      | 
        v12 += 8;                                                                                 | 
        --v19;                                                                                    | 
      }                                                                                           | 
      while ( v19 );                                                                              | 
      *v18 = *v12;                                                                                | 
      v18[1] = v12[1];                                                                            | 
      v18[2] = v12[2];                                                                            | 
      v18[3] = v12[3];                                                                            | 
      v18[4] = v12[4];                                                                            | 
      v25 = v36;                                                                                  | 
      v37 = (v37 & 0xFFFFFFFFFFFFFFF0uLL) - 40;                                                   | 
      if ( (a7 & 3) == 0 )                                                                        | 
      {                                                                                           | 
        if ( a7 < 0x7FFFFFFF0000LL )                                                              | 
        {                                                                                         | 
          v13 = a7;                                                                               | 
        }                                                                                         | 
        v12 = v36;                                                                                | 
        LOBYTE(v13) = v24;                                                                        | 
        goto LABEL_20;                                                                            | 
      }                                                                                           | 
    }                                                                                             | 
LABEL_18:                                                                                         | 
    ExRaiseDatatypeMisalignment();                                                                | 
  }                                                                                               | 
  v25 = (_BYTE *)a6;                                                                              | 
LABEL_20:                                                                                         | 
  result = RtlpSanitizeContextFlags(v12 + 3, v13, v14);                                           | 
  if ( (int)result >= 0 )                                                                         | 
  {                                                                                               | 
    *((_DWORD *)v12 + 12) &= 0x10001Fu;                                                           | 
    *a1 = 0LL;                                                                                    | 
    v20 = *(_OWORD *)a7;                                                                          | 
    v26 = *(_OWORD *)a7;                                                                          | 
    if ( v26 == 0 )                                                                               | 
    {                                                                                             | 
      v26 = v20;                                                                                  | 
      v27 = *(_OWORD *)(a7 + 16);                                                                 | 
      v28 = *(_QWORD *)(a7 + 32);                                                                 | 
      if ( v28 )                                                                                  | 
      {                                                                                           | 
        v32 = KeGetCurrentThread();                                                               | 
        Process = v32->ApcState.Process;                                                          | 
        if ( Process[1].ReadyTime )                                                               | 
        {                                                                                         | 
          v22 = WORD2(Process[3].PerProcessorCycleTimes);                                         | 
          if ( v22 == 332 || v22 == 452 )                                                         | 
          {                                                                                       | 
            v30 = v28;                                                                            | 
            v29 = v27;                                                                            | 
            v28 = 0LL;                                                                            | 
            v27 = 0LL;                                                                            | 
          }                                                                                       | 
        }                                                                                         | 
        v33 = 1;                                                                                  | 
        return PspCreateThread(a1, a2, a3, a4, 0LL, 0LL, a5, v25, &v26, a8 == 1, 0LL, 0LL, &v33); | 
      }                                                                                           | 
      else                                                                                        | 
      {                                                                                           | 
        return 3221225485LL;                                                                      | 
      }                                                                                           | 
    }                                                                                             | 
    else                                                                                          | 
    {                                                                                             | 
      return 3221225659LL;                                                                        | 
    }                                                                                             | 
  }                                                                                               | 
  return result;                                                                                  | 
}                                                                                                 | 
131 row(s)
