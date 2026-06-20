__int64 __fastcall NtGdiExtTextOutW(                                                                             | 
        HDC a1,                                                                                                  | 
        unsigned int a2,                                                                                         | 
        unsigned int a3,                                                                                         | 
        int a4,                                                                                                  | 
        ULONG64 a5,                                                                                              | 
        __int64 a6,                                                                                              | 
        unsigned int a7,                                                                                         | 
        __int64 a8,                                                                                              | 
        int a9)                                                                                                  | 
{                                                                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                  | 
                                                                                                                 | 
  v36 = a4;                                                                                                      | 
  v48 = a1;                                                                                                      | 
  v35 = a7;                                                                                                      | 
  v49 = a1;                                                                                                      | 
  v34 = a2;                                                                                                      | 
  v42 = a3;                                                                                                      | 
  v40 = a4;                                                                                                      | 
  v10 = (__int128 *)a5;                                                                                          | 
  v39 = (__int128 *)a5;                                                                                          | 
  v11 = a6;                                                                                                      | 
  v45 = a6;                                                                                                      | 
  v51 = a6;                                                                                                      | 
  v37 = a7;                                                                                                      | 
  v43 = a8;                                                                                                      | 
  v41 = a9;                                                                                                      | 
  v55 = 0LL;                                                                                                     | 
  if ( a7 > 0xFFFF )                                                                                             | 
  {                                                                                                              | 
    goto LABEL_52;                                                                                               | 
  }                                                                                                              | 
  if ( a5 )                                                                                                      | 
  {                                                                                                              | 
    if ( (a4 & 6) != 0 )                                                                                         | 
    {                                                                                                            | 
      if ( a5 >= MmUserProbeAddress )                                                                            | 
      {                                                                                                          | 
        v10 = (__int128 *)MmUserProbeAddress;                                                                    | 
      }                                                                                                          | 
      RtlCopyVolatileMemory(&v55, v10, 0x10uLL);                                                                 | 
      v10 = &v55;                                                                                                | 
      v39 = &v55;                                                                                                | 
      LOBYTE(a4) = v36;                                                                                          | 
      v11 = v45;                                                                                                 | 
    }                                                                                                            | 
    else                                                                                                         | 
    {                                                                                                            | 
      v10 = 0LL;                                                                                                 | 
      v39 = 0LL;                                                                                                 | 
    }                                                                                                            | 
  }                                                                                                              | 
  if ( a7 )                                                                                                      | 
  {                                                                                                              | 
    if ( (a4 & 6) != 0 && !v10 || !v11 )                                                                         | 
    {                                                                                                            | 
      v15 = 0LL;                                                                                                 | 
      goto LABEL_20;                                                                                             | 
    }                                                                                                            | 
    v14 = *(_DWORD *)ReturnValueTracer<unsigned long>::ReturnValueTracer<unsigned long>(&v38, 1LL);              | 
    v18 = 0;                                                                                                     | 
    v19 = (30 * a7 + 7) & 0xFFFFFFF8;                                                                            | 
    v38 = v19;                                                                                                   | 
    if ( v17 )                                                                                                   | 
    {                                                                                                            | 
      v18 = 4 * a7;                                                                                              | 
      if ( (v16 & 0x2000) != 0 )                                                                                 | 
      {                                                                                                          | 
        v18 = 8 * a7;                                                                                            | 
      }                                                                                                          | 
    }                                                                                                            | 
    v50 = v18 + 7;                                                                                               | 
    LODWORD(v47) = v18 + 7;                                                                                      | 
    if ( (unsigned __int64)(v19 + 2 * a7 + (v50 & 0xFFFFFFF8)) <= 0xC0 )                                         | 
    {                                                                                                            | 
      v20 = 0LL;                                                                                                 | 
      v46 = 0LL;                                                                                                 | 
      v21 = v56;                                                                                                 | 
      v44 = v56;                                                                                                 | 
LABEL_28:                                                                                                        | 
      if ( v17 )                                                                                                 | 
      {                                                                                                          | 
        v22 = (_QWORD *)MmUserProbeAddress;                                                                      | 
        if ( v18 )                                                                                               | 
        {                                                                                                        | 
          v23 = (ULONG64)v17 + v18;                                                                              | 
          if ( v23 > MmUserProbeAddress || v23 < (unsigned __int64)v17 )                                         | 
          {                                                                                                      | 
            v22 = (_QWORD *)MmUserProbeAddress;                                                                  | 
            v10 = v39;                                                                                           | 
          }                                                                                                      | 
        }                                                                                                        | 
        memmove(v21, v17, v18);                                                                                  | 
        v24 = (char *)v45;                                                                                       | 
        v43 = (__int64)v21;                                                                                      | 
        v21 += (unsigned int)v50 & 0xFFFFFFF8;                                                                   | 
      }                                                                                                          | 
      else                                                                                                       | 
      {                                                                                                          | 
        v22 = (_QWORD *)MmUserProbeAddress;                                                                      | 
        v24 = (char *)v45;                                                                                       | 
      }                                                                                                          | 
      v25 = v21;                                                                                                 | 
      v47 = v21;                                                                                                 | 
      v26 = &v21[v19];                                                                                           | 
      v44 = v26;                                                                                                 | 
      v27 = 2LL * v35;                                                                                           | 
      if ( v27 )                                                                                                 | 
      {                                                                                                          | 
        if ( ((unsigned __int8)v24 & 1) != 0 )                                                                   | 
        {                                                                                                        | 
          ExRaiseDatatypeMisalignment();                                                                         | 
        }                                                                                                        | 
        if ( (unsigned __int64)&v24[v27] > *v22 || &v24[v27] < v24 )                                             | 
        {                                                                                                        | 
          v10 = v39;                                                                                             | 
        }                                                                                                        | 
      }                                                                                                          | 
      memmove(v26, v24, v27);                                                                                    | 
      v28 = v43;                                                                                                 | 
      if ( v14 )                                                                                                 | 
      {                                                                                                          | 
        APIDCOBJ::APIDCOBJ((APIDCOBJ *)v54, v48);                                                                | 
        if ( v54[0] )                                                                                            | 
        {                                                                                                        | 
          SessionState = W32GetSessionState();                                                                   | 
          EUDCCountRegion::EUDCCountRegion(                                                                      | 
            (EUDCCountRegion *)v53,                                                                              | 
            (struct Gre::Font::GLOBALS *)(*(_QWORD *)(SessionState + 96) + 4872LL));                             | 
          if ( EUDCCountRegion::IsReady((EUDCCountRegion *)v53) )                                                | 
          {                                                                                                      | 
            v30 = (unsigned int *)GrepExtTextOutW(&v42, v54, v34, v42, v36, v10, v26, v35, v28, v25, v41, 24);   | 
          }                                                                                                      | 
          else                                                                                                   | 
          {                                                                                                      | 
            v30 = (unsigned int *)ReturnValueTracer<unsigned long>::ReturnValueTracer<unsigned long>(&v41, 0LL); | 
          }                                                                                                      | 
          v14 = *v30;                                                                                            | 
          EUDCCountRegion::~EUDCCountRegion((EUDCCountRegion *)v53);                                             | 
        }                                                                                                        | 
        else                                                                                                     | 
        {                                                                                                        | 
          v14 = *(_DWORD *)ReturnValueTracer<unsigned long>::ReturnValueTracer<unsigned long>(&v34, 0LL);        | 
        }                                                                                                        | 
        APIDCOBJ::~APIDCOBJ((APIDCOBJ *)v54);                                                                    | 
      }                                                                                                          | 
      if ( v20 )                                                                                                 | 
      {                                                                                                          | 
        FreeTmpBuffer(v20);                                                                                      | 
      }                                                                                                          | 
      goto LABEL_51;                                                                                             | 
    }                                                                                                            | 
    v20 = AllocFreeTmpBuffer();                                                                                  | 
    v46 = v20;                                                                                                   | 
    v21 = (_BYTE *)v20;                                                                                          | 
    v44 = (_BYTE *)v20;                                                                                          | 
    if ( v20 )                                                                                                   | 
    {                                                                                                            | 
      v17 = (const void *)v43;                                                                                   | 
      goto LABEL_28;                                                                                             | 
    }                                                                                                            | 
LABEL_52:                                                                                                        | 
    v31 = 0LL;                                                                                                   | 
    v32 = &v34;                                                                                                  | 
    return *(unsigned int *)ReturnValueTracer<unsigned long>::ReturnValueTracer<unsigned long>(v32, v31);        | 
  }                                                                                                              | 
  if ( v10 && (a4 & 2) != 0 )                                                                                    | 
  {                                                                                                              | 
    v12 = W32GetSessionState();                                                                                  | 
    EUDCCountRegion::EUDCCountRegion(                                                                            | 
      (EUDCCountRegion *)v52,                                                                                    | 
      (struct Gre::Font::GLOBALS *)(*(_QWORD *)(v12 + 96) + 4872LL));                                            | 
    if ( EUDCCountRegion::IsReady((EUDCCountRegion *)v52) )                                                      | 
    {                                                                                                            | 
      v13 = (unsigned int *)GreExtTextOutRect(&v34, a1, v10);                                                    | 
    }                                                                                                            | 
    else                                                                                                         | 
    {                                                                                                            | 
      v13 = (unsigned int *)ReturnValueTracer<unsigned long>::ReturnValueTracer<unsigned long>(&v34, 0LL);       | 
    }                                                                                                            | 
    v14 = *v13;                                                                                                  | 
    EUDCCountRegion::~EUDCCountRegion((EUDCCountRegion *)v52);                                                   | 
    goto LABEL_51;                                                                                               | 
  }                                                                                                              | 
  v15 = 1LL;                                                                                                     | 
LABEL_20:                                                                                                        | 
  v14 = *(_DWORD *)ReturnValueTracer<unsigned long>::ReturnValueTracer<unsigned long>(&v34, v15);                | 
LABEL_51:                                                                                                        | 
  v31 = v14;                                                                                                     | 
  v32 = &v37;                                                                                                    | 
  return *(unsigned int *)ReturnValueTracer<unsigned long>::ReturnValueTracer<unsigned long>(v32, v31);          | 
}                                                                                                                | 
193 row(s)
