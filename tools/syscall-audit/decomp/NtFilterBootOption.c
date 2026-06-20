__int64 __fastcall NtFilterBootOption(int a1, int a2, unsigned int a3, _QWORD *a4, size_t Size)       | 
{                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                       | 
                                                                                                      | 
  v7 = a1;                                                                                            | 
  P = 0LL;                                                                                            | 
  v21[0] = 0;                                                                                         | 
  v24[0] = 0LL;                                                                                       | 
  v20 = SeSinglePrivilegeCheck(SeTcbPrivilege, KeGetCurrentThread()->PreviousMode);                   | 
  if ( !v20 && ((int)RtlCheckTokenMembership(0LL, SeAliasAdminsSid, &v20) < 0 || !v20) )              | 
  {                                                                                                   | 
    v8 = -1073741790;                                                                                 | 
    goto LABEL_64;                                                                                    | 
  }                                                                                                   | 
  v22 = 0;                                                                                            | 
  v8 = 0;                                                                                             | 
  if ( !qword_140E67C08 )                                                                             | 
  {                                                                                                   | 
    v8 = -2143092730;                                                                                 | 
    goto LABEL_64;                                                                                    | 
  }                                                                                                   | 
  if ( !v7 )                                                                                          | 
  {                                                                                                   | 
    if ( !a2 && !a3 && !a4 && !(_DWORD)Size )                                                         | 
    {                                                                                                 | 
      if ( !_InterlockedCompareExchange(&dword_140E67BD8, 0, 0) )                                     | 
      {                                                                                               | 
        v8 = SepSecureBootCorrectBcd(v21);                                                            | 
        v22 = v8;                                                                                     | 
        if ( v8 < 0 || !byte_140E67BDC && v21[0] )                                                    | 
        {                                                                                             | 
          goto LABEL_64;                                                                              | 
        }                                                                                             | 
        _InterlockedExchange(&dword_140E67BD8, 1);                                                    | 
      }                                                                                               | 
      v8 = v22;                                                                                       | 
      goto LABEL_64;                                                                                  | 
    }                                                                                                 | 
    goto LABEL_52;                                                                                    | 
  }                                                                                                   | 
  if ( v7 == 1 )                                                                                      | 
  {                                                                                                   | 
    if ( a2 )                                                                                         | 
    {                                                                                                 | 
      if ( a3 )                                                                                       | 
      {                                                                                               | 
        if ( a4 )                                                                                     | 
        {                                                                                             | 
          v9 = Size;                                                                                  | 
          if ( (_DWORD)Size )                                                                         | 
          {                                                                                           | 
            v11 = dword_140E67BE8;                                                                    | 
            if ( !_bittest(&v11, HIBYTE(a3) & 0xF) )                                                  | 
            {                                                                                         | 
LABEL_21:                                                                                             | 
              v8 = 0;                                                                                 | 
              goto LABEL_64;                                                                          | 
            }                                                                                         | 
            if ( KeGetCurrentThread()->PreviousMode )                                                 | 
            {                                                                                         | 
              v12 = a4;                                                                               | 
              if ( (unsigned __int64)a4 + (unsigned int)Size > 0x7FFFFFFF0000LL                       | 
                || (_QWORD *)((char *)a4 + (unsigned int)Size) < a4 )                                 | 
              {                                                                                       | 
                v12 = a4;                                                                             | 
              }                                                                                       | 
              if ( (unsigned int)Size > 8 )                                                           | 
              {                                                                                       | 
                P = (PVOID)ExAllocatePool2(0x100uLL, (unsigned int)Size, 0x62536553u);                | 
                if ( !P )                                                                             | 
                {                                                                                     | 
                  v8 = -1073741801;                                                                   | 
                  v22 = -1073741801;                                                                  | 
                  goto LABEL_64;                                                                      | 
                }                                                                                     | 
                v13 = P;                                                                              | 
                memmove(P, v12, (unsigned int)Size);                                                  | 
                a4 = v13;                                                                             | 
              }                                                                                       | 
              else                                                                                    | 
              {                                                                                       | 
                memmove(v24, v12, (unsigned int)Size);                                                | 
                a4 = v24;                                                                             | 
              }                                                                                       | 
              v8 = v22;                                                                               | 
              v7 = a1;                                                                                | 
            }                                                                                         | 
            else                                                                                      | 
            {                                                                                         | 
              v8 = v22;                                                                               | 
            }                                                                                         | 
            goto LABEL_33;                                                                            | 
          }                                                                                           | 
        }                                                                                             | 
      }                                                                                               | 
    }                                                                                                 | 
LABEL_52:                                                                                             | 
    v8 = -1073741811;                                                                                 | 
    goto LABEL_64;                                                                                    | 
  }                                                                                                   | 
  if ( v7 != 2 )                                                                                      | 
  {                                                                                                   | 
    v8 = -1073741585;                                                                                 | 
    goto LABEL_64;                                                                                    | 
  }                                                                                                   | 
  if ( !a2 )                                                                                          | 
  {                                                                                                   | 
    goto LABEL_52;                                                                                    | 
  }                                                                                                   | 
  if ( !a3 )                                                                                          | 
  {                                                                                                   | 
    goto LABEL_52;                                                                                    | 
  }                                                                                                   | 
  if ( a4 )                                                                                           | 
  {                                                                                                   | 
    goto LABEL_52;                                                                                    | 
  }                                                                                                   | 
  v9 = Size;                                                                                          | 
  if ( (_DWORD)Size )                                                                                 | 
  {                                                                                                   | 
    goto LABEL_52;                                                                                    | 
  }                                                                                                   | 
  v10 = dword_140E67BE8;                                                                              | 
  if ( !_bittest(&v10, HIBYTE(a3) & 0xF) )                                                            | 
  {                                                                                                   | 
    goto LABEL_21;                                                                                    | 
  }                                                                                                   | 
LABEL_33:                                                                                             | 
  v14 = 0;                                                                                            | 
  if ( *((_WORD *)qword_140E67C08 + 18) )                                                             | 
  {                                                                                                   | 
    v15 = (unsigned int *)(qword_140E67BE0 + 8);                                                      | 
    while ( 1 )                                                                                       | 
    {                                                                                                 | 
      if ( *(v15 - 1) == a3 )                                                                         | 
      {                                                                                               | 
        v16 = *(v15 - 2);                                                                             | 
        if ( !v16 || v16 == a2 )                                                                      | 
        {                                                                                             | 
          v17 = *(unsigned __int16 *)(*v15 + qword_140E67BF8);                                        | 
          if ( (v17 & 0xFFFFF000) == 0                                                                | 
            && ((v17 & 0x20) == 0 || (dword_140E67C14 & 4) != 0)                                      | 
            && ((v17 & 0x40) == 0 || (dword_140E67C14 & 0x10) != 0)                                   | 
            && ((v17 & 0x800) == 0 || (dword_140E67C14 & 0x40) != 0) )                                | 
          {                                                                                           | 
            break;                                                                                    | 
          }                                                                                           | 
        }                                                                                             | 
      }                                                                                               | 
      ++v14;                                                                                          | 
      v15 += 3;                                                                                       | 
      if ( v14 >= *((unsigned __int16 *)qword_140E67C08 + 18) )                                       | 
      {                                                                                               | 
        goto LABEL_64;                                                                                | 
      }                                                                                               | 
    }                                                                                                 | 
    if ( v7 == 1 )                                                                                    | 
    {                                                                                                 | 
      v8 = SepSecureBootValidateBcdDataAgainstBcdRule(qword_140E67BE0 + 12LL * v14, a4, v9);          | 
    }                                                                                                 | 
    else                                                                                              | 
    {                                                                                                 | 
      v8 = 0;                                                                                         | 
      v18 = *(unsigned int *)(qword_140E67BE0 + 12LL * v14 + 8);                                      | 
      if ( (*(_BYTE *)(v18 + qword_140E67BF8) & 0x1F) != 8 || *(_WORD *)(v18 + qword_140E67BF8 + 2) ) | 
      {                                                                                               | 
        v8 = -1069350910;                                                                             | 
      }                                                                                               | 
    }                                                                                                 | 
  }                                                                                                   | 
LABEL_64:                                                                                             | 
  if ( P )                                                                                            | 
  {                                                                                                   | 
    ExFreePoolWithTag(P, 0x62536553u);                                                                | 
  }                                                                                                   | 
  return (unsigned int)v8;                                                                            | 
}                                                                                                     | 
177 row(s)
