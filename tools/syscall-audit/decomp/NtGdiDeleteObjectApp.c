__int64 __fastcall NtGdiDeleteObjectApp(HDC a1)                                                                  | 
{                                                                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                  | 
                                                                                                                 | 
  if ( ((unsigned int)a1 & 0x800000) != 0 )                                                                      | 
  {                                                                                                              | 
    return 1LL;                                                                                                  | 
  }                                                                                                              | 
  v2 = (unsigned int)a1 >> 16;                                                                                   | 
  v3 = BYTE2(a1) & 0x1F;                                                                                         | 
  SessionState = W32GetSessionState();                                                                           | 
  v5 = 0;                                                                                                        | 
  v6 = 1;                                                                                                        | 
  v7 = (unsigned __int16)a1 | ((unsigned int)a1 >> 8) & 0xFF0000;                                                | 
  v8 = *(_QWORD *)(*(_QWORD *)(SessionState + 88) + 5672LL);                                                     | 
  if ( (unsigned int)v7 >= 0x10000 )                                                                             | 
  {                                                                                                              | 
    if ( *(_DWORD *)v8 <= 0x10000u )                                                                             | 
    {                                                                                                            | 
      v7 = (unsigned __int16)a1;                                                                                 | 
    }                                                                                                            | 
    else                                                                                                         | 
    {                                                                                                            | 
      v9 = *(_QWORD *)(v8 + 16);                                                                                 | 
      v10 = *(_DWORD *)(v9 + 2056);                                                                              | 
      if ( (unsigned __int16)a1 >= v10 + ((*(unsigned __int16 *)(v9 + 2) + 0xFFFF) << 16) )                      | 
      {                                                                                                          | 
        v14 = 0LL;                                                                                               | 
      }                                                                                                          | 
      else                                                                                                       | 
      {                                                                                                          | 
        if ( (unsigned __int16)a1 >= v10 )                                                                       | 
        {                                                                                                        | 
          v11 = (((unsigned __int16)a1 - v10) >> 16) + 1;                                                        | 
        }                                                                                                        | 
        else                                                                                                     | 
        {                                                                                                        | 
          v11 = 0;                                                                                               | 
        }                                                                                                        | 
        v12 = *(_QWORD *)(v9 + 8LL * v11 + 8);                                                                   | 
        if ( v11 )                                                                                               | 
        {                                                                                                        | 
          v13 = (unsigned __int16)a1 + ((1 - v11) << 16) - v10;                                                  | 
        }                                                                                                        | 
        else                                                                                                     | 
        {                                                                                                        | 
          v13 = (unsigned __int16)a1;                                                                            | 
        }                                                                                                        | 
        v14 = 0LL;                                                                                               | 
        if ( (unsigned int)v13 < *(_DWORD *)(v12 + 20) )                                                         | 
        {                                                                                                        | 
          v14 = *(_QWORD *)v12 + 24 * v13;                                                                       | 
        }                                                                                                        | 
      }                                                                                                          | 
      if ( *(unsigned __int8 *)(v14 + 13) == WORD1(v7) )                                                         | 
      {                                                                                                          | 
        v7 = (unsigned __int16)a1;                                                                               | 
      }                                                                                                          | 
    }                                                                                                            | 
  }                                                                                                              | 
  v16 = *(_QWORD *)(v8 + 16);                                                                                    | 
  v17 = *(_DWORD *)(v16 + 2056);                                                                                 | 
  if ( (unsigned int)v7 < v17 + ((*(unsigned __int16 *)(v16 + 2) + 0xFFFF) << 16) )                              | 
  {                                                                                                              | 
    if ( (unsigned int)v7 >= v17 )                                                                               | 
    {                                                                                                            | 
      v18 = (((unsigned int)v7 - v17) >> 16) + 1;                                                                | 
    }                                                                                                            | 
    else                                                                                                         | 
    {                                                                                                            | 
      v18 = 0;                                                                                                   | 
    }                                                                                                            | 
    v19 = *(_QWORD *)(v16 + 8LL * v18 + 8);                                                                      | 
    if ( v18 )                                                                                                   | 
    {                                                                                                            | 
      v7 = ((1 - v18) << 16) - v17 + (unsigned int)v7;                                                           | 
    }                                                                                                            | 
    v20 = 0LL;                                                                                                   | 
    if ( (unsigned int)v7 < *(_DWORD *)(v19 + 20)                                                                | 
      && *(_QWORD *)(*(_QWORD *)(**(_QWORD **)(v19 + 24) + 8 * (v7 >> 8)) + 16LL * (unsigned __int8)v7 + 8) )    | 
    {                                                                                                            | 
      v20 = *(_QWORD *)v19 + 24LL * (unsigned int)v7;                                                            | 
    }                                                                                                            | 
    if ( v20                                                                                                     | 
      && *(unsigned __int8 *)(v20 + 14) == v3                                                                    | 
      && *(_WORD *)(v20 + 12) == (_WORD)v2                                                                       | 
      && (*(_DWORD *)(v20 + 8) & 0xFFFFFFFE) == 0 )                                                              | 
    {                                                                                                            | 
      LOBYTE(v5) = a1 != 0LL;                                                                                    | 
      return v5;                                                                                                 | 
    }                                                                                                            | 
  }                                                                                                              | 
  v21 = v3 - 1;                                                                                                  | 
  if ( !v21 )                                                                                                    | 
  {                                                                                                              | 
    return GrepDeleteDC(a1);                                                                                     | 
  }                                                                                                              | 
  v22 = v21 - 3;                                                                                                 | 
  if ( !v22 )                                                                                                    | 
  {                                                                                                              | 
    RGNOBJAPI::RGNOBJAPI((RGNOBJAPI *)v37, (HRGN)a1, 0, 0);                                                      | 
    if ( !v37[0] || *(_DWORD *)(v37[0] + 76LL) || !(unsigned int)RGNOBJAPI::bDeleteRGNOBJAPI((RGNOBJAPI *)v37) ) | 
    {                                                                                                            | 
      v6 = 0;                                                                                                    | 
    }                                                                                                            | 
    RGNOBJAPI::~RGNOBJAPI((RGNOBJAPI *)v37);                                                                     | 
    return v6;                                                                                                   | 
  }                                                                                                              | 
  v23 = v22 - 1;                                                                                                 | 
  if ( !v23 )                                                                                                    | 
  {                                                                                                              | 
    SEMOBJSHARED<1>::SEMOBJSHARED<1>(&v38);                                                                      | 
    v26 = bDeleteSurface(a1);                                                                                    | 
    EtwTraceGreLockReleaseSemaphore(L"DynamicModeChange", v38);                                                  | 
    GrepReleaseLockValidate<1>();                                                                                | 
    GreReleaseSemaphoreSharedInternal(v38);                                                                      | 
    return v26;                                                                                                  | 
  }                                                                                                              | 
  v24 = v23 - 3;                                                                                                 | 
  if ( !v24 )                                                                                                    | 
  {                                                                                                              | 
    v28 = W32GetSessionState();                                                                                  | 
    SEMOBJ<13>::SEMOBJ<13>(&v39, *(_QWORD *)(v28 + 88));                                                         | 
    LOBYTE(v29) = 8;                                                                                             | 
    v30 = (HSEMAPHORE)HmgShareLockCheck(a1, v29);                                                                | 
    v38 = v30;                                                                                                   | 
    if ( v30 )                                                                                                   | 
    {                                                                                                            | 
      if ( ((_DWORD)v30[6] & 0x1000000) == 0 )                                                                   | 
      {                                                                                                          | 
        v31 = XEPALOBJ::bDeletePalette(&v38, 0LL);                                                               | 
        if ( v38 )                                                                                               | 
        {                                                                                                        | 
          HmgDecrementShareReferenceCountEx((struct _BASEOBJECT *)v38, 0LL);                                     | 
        }                                                                                                        | 
        v32 = v39;                                                                                               | 
        if ( v39 )                                                                                               | 
        {                                                                                                        | 
          EtwTraceGreLockReleaseSemaphore(L"Palette", v39);                                                      | 
          v33 = GreGetCurrentThreadCrossSessionCheck();                                                          | 
          if ( v33 )                                                                                             | 
          {                                                                                                      | 
            v34 = (*((_BYTE *)v33 + 21))-- == 1;                                                                 | 
            if ( v34 )                                                                                           | 
            {                                                                                                    | 
              *(_QWORD *)v33 &= ~0x2000uLL;                                                                      | 
            }                                                                                                    | 
          }                                                                                                      | 
          GreReleaseSemaphoreSharedInternal(v32);                                                                | 
        }                                                                                                        | 
        return v31;                                                                                              | 
      }                                                                                                          | 
      HmgDecrementShareReferenceCountEx((struct _BASEOBJECT *)v30, 0LL);                                         | 
    }                                                                                                            | 
    v35 = v39;                                                                                                   | 
    if ( v39 )                                                                                                   | 
    {                                                                                                            | 
      EtwTraceGreLockReleaseSemaphore(L"Palette", v39);                                                          | 
      v36 = GreGetCurrentThreadCrossSessionCheck();                                                              | 
      if ( v36 )                                                                                                 | 
      {                                                                                                          | 
        v34 = (*((_BYTE *)v36 + 21))-- == 1;                                                                     | 
        if ( v34 )                                                                                               | 
        {                                                                                                        | 
          *(_QWORD *)v36 &= ~0x2000uLL;                                                                          | 
        }                                                                                                        | 
      }                                                                                                          | 
      GreReleaseSemaphoreSharedInternal(v35);                                                                    | 
    }                                                                                                            | 
    return 0LL;                                                                                                  | 
  }                                                                                                              | 
  v25 = v24 - 2;                                                                                                 | 
  if ( !v25 )                                                                                                    | 
  {                                                                                                              | 
    v27 = *(unsigned int (__fastcall **)(HDC, _QWORD))(*(_QWORD *)(W32GetWin32kBaseApiSetTable() + 24) + 592LL); | 
    if ( v27 )                                                                                                   | 
    {                                                                                                            | 
      return v27(a1, 0LL);                                                                                       | 
    }                                                                                                            | 
    return v5;                                                                                                   | 
  }                                                                                                              | 
  if ( v25 != 6 )                                                                                                | 
  {                                                                                                              | 
    return 0LL;                                                                                                  | 
  }                                                                                                              | 
  return bDeleteBrush((HBRUSH)a1);                                                                               | 
}                                                                                                                | 
187 row(s)
