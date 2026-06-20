__int64 __fastcall NtGdiPolyPatBlt(HDC a1, unsigned int a2, struct _POLYPATBLT *a3, unsigned int a4, unsigned int a5) | 
{                                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                       | 
                                                                                                                      | 
  v8 = 1;                                                                                                             | 
  v9 = 0;                                                                                                             | 
  if ( a4 )                                                                                                           | 
  {                                                                                                                   | 
    if ( a3 )                                                                                                         | 
    {                                                                                                                 | 
      APIDCOBJ::APIDCOBJ((APIDCOBJ *)v21, a1);                                                                        | 
      if ( v21[0] )                                                                                                   | 
      {                                                                                                               | 
        if ( a4 > 0xAAAAAAA )                                                                                         | 
        {                                                                                                             | 
          v8 = 0;                                                                                                     | 
        }                                                                                                             | 
        else                                                                                                          | 
        {                                                                                                             | 
          GreProbeUntrustedVaRange(a3, 0x18uLL, a4, 4uLL);                                                            | 
        }                                                                                                             | 
        if ( v8 )                                                                                                     | 
        {                                                                                                             | 
          if ( ((gajRop3[(unsigned __int8)a2] | gajRop3[BYTE1(a2)]) & 0xB2) != 0 )                                    | 
          {                                                                                                           | 
            DWMSCREENREADMODIFYWRITEASSIST::DWMSCREENREADMODIFYWRITEASSIST(                                           | 
              (DWMSCREENREADMODIFYWRITEASSIST *)v22,                                                                  | 
              (struct XDCOBJ *)v21);                                                                                  | 
            v10 = *v23;                                                                                               | 
            if ( *v23 && *(_DWORD *)(v10 + 492) && (*(_DWORD *)(v10 + 36) & 0x4000) != 0 )                            | 
            {                                                                                                         | 
              for ( i = 0LL; (unsigned int)i < a4; i = (unsigned int)(v17 + 1) )                                      | 
              {                                                                                                       | 
                v14 = *((_DWORD *)a3 + 6 * i + 1);                                                                    | 
                v15 = v14 + *((_DWORD *)a3 + 6 * i + 3);                                                              | 
                v16 = *((_DWORD *)a3 + 6 * i + 2);                                                                    | 
                v19.left = *((_DWORD *)a3 + 6 * i);                                                                   | 
                v19.top = v14;                                                                                        | 
                v19.right = v19.left + v16;                                                                           | 
                v19.bottom = v15;                                                                                     | 
                ERECTL::vOrder((ERECTL *)&v19);                                                                       | 
                if ( v17 )                                                                                            | 
                {                                                                                                     | 
                  ERECTL::operator|=(&v20, &v19);                                                                     | 
                }                                                                                                     | 
                else                                                                                                  | 
                {                                                                                                     | 
                  v20 = v19;                                                                                          | 
                }                                                                                                     | 
              }                                                                                                       | 
              if ( !(unsigned int)ERECTL::bEmpty((ERECTL *)&v20) )                                                    | 
              {                                                                                                       | 
                v9 = DWMSCREENREADMODIFYWRITEASSIST::bReadBackRect((DWMSCREENREADMODIFYWRITEASSIST *)v22, &v20);      | 
              }                                                                                                       | 
            }                                                                                                         | 
          }                                                                                                           | 
          v8 = GrepPolyPatBlt(                                                                                        | 
                 (struct XDCOBJ *)v21,                                                                                | 
                 a2,                                                                                                  | 
                 a3,                                                                                                  | 
                 a4,                                                                                                  | 
                 a5,                                                                                                  | 
                 *(_DWORD *)(*(_QWORD *)(v21[0] + 976LL) + 184LL),                                                    | 
                 *(_DWORD *)(*(_QWORD *)(v21[0] + 976LL) + 176LL),                                                    | 
                 *(_DWORD *)(*(_QWORD *)(v21[0] + 976LL) + 188LL),                                                    | 
                 *(_DWORD *)(*(_QWORD *)(v21[0] + 976LL) + 180LL));                                                   | 
        }                                                                                                             | 
      }                                                                                                               | 
      else                                                                                                            | 
      {                                                                                                               | 
        v8 = 0;                                                                                                       | 
        EngSetLastError(6u);                                                                                          | 
      }                                                                                                               | 
      APIDCOBJ::~APIDCOBJ((APIDCOBJ *)v21);                                                                           | 
    }                                                                                                                 | 
    else                                                                                                              | 
    {                                                                                                                 | 
      v8 = 0;                                                                                                         | 
    }                                                                                                                 | 
  }                                                                                                                   | 
  if ( v9 )                                                                                                           | 
  {                                                                                                                   | 
    v18 = UserReferenceDwmApiPort(a1);                                                                                | 
    DwmSyncFlushAndWaitForBatch(v18);                                                                                 | 
  }                                                                                                                   | 
  return v8;                                                                                                          | 
}                                                                                                                     | 
87 row(s)
