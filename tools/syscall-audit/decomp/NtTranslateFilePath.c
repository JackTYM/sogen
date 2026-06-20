__int64 __fastcall NtTranslateFilePath(char *Src, unsigned int a2, volatile void *a3, unsigned int *a4) | 
{                                                                                                       | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                         | 
                                                                                                        | 
  v26 = 0;                                                                                              | 
  P = 0LL;                                                                                              | 
  v29 = 0LL;                                                                                            | 
  if ( dword_140EFED30 != 2 )                                                                           | 
  {                                                                                                     | 
    return 3221225474LL;                                                                                | 
  }                                                                                                     | 
  if ( a2 - 1 <= 4 )                                                                                    | 
  {                                                                                                     | 
    if ( !(unsigned __int8)PsIsCurrentThreadInServerSilo() )                                            | 
    {                                                                                                   | 
      CurrentThread = KeGetCurrentThread();                                                             | 
      v8 = CurrentThread->PreviousMode;                                                                 | 
      PreviousMode = v8;                                                                                | 
      v9 = (unsigned __int64)(Src + 4);                                                                 | 
      v10 = 0x7FFFFFFF0000LL;                                                                           | 
      if ( v8 )                                                                                         | 
      {                                                                                                 | 
        if ( v9 >= 0x7FFFFFFF0000LL )                                                                   | 
        {                                                                                               | 
          v9 = 0x7FFFFFFF0000LL;                                                                        | 
        }                                                                                               | 
        v11 = *(_DWORD *)v9;                                                                            | 
        v28 = v11;                                                                                      | 
      }                                                                                                 | 
      else                                                                                              | 
      {                                                                                                 | 
        v11 = *(_DWORD *)v9;                                                                            | 
        v28 = v11;                                                                                      | 
      }                                                                                                 | 
      if ( v11 < 0xC )                                                                                  | 
      {                                                                                                 | 
        return 3221225485LL;                                                                            | 
      }                                                                                                 | 
      if ( v8 )                                                                                         | 
      {                                                                                                 | 
        if ( ((unsigned __int8)Src & 3) != 0 )                                                          | 
        {                                                                                               | 
          ExRaiseDatatypeMisalignment();                                                                | 
        }                                                                                               | 
        if ( (unsigned __int64)a4 < 0x7FFFFFFF0000LL )                                                  | 
        {                                                                                               | 
          v10 = (__int64)a4;                                                                            | 
        }                                                                                               | 
        *(_DWORD *)v10 = *(_DWORD *)v10;                                                                | 
        v13 = *a4;                                                                                      | 
        v26 = v13;                                                                                      | 
        v17 = v13;                                                                                      | 
        if ( !a3 )                                                                                      | 
        {                                                                                               | 
          v13 = 0;                                                                                      | 
          v26 = 0;                                                                                      | 
          v17 = 0;                                                                                      | 
        }                                                                                               | 
        if ( v17 )                                                                                      | 
        {                                                                                               | 
          ProbeForWrite(a3, v17, 4u);                                                                   | 
        }                                                                                               | 
        if ( !SeSinglePrivilegeCheck(SeSystemEnvironmentPrivilege, PreviousMode) )                      | 
        {                                                                                               | 
          return 3221225569LL;                                                                          | 
        }                                                                                               | 
        v11 = v28;                                                                                      | 
      }                                                                                                 | 
      else                                                                                              | 
      {                                                                                                 | 
        v12 = *a4;                                                                                      | 
        v13 = a3 != 0LL ? *a4 : 0;                                                                      | 
        v26 = a3 != 0LL ? v12 : 0;                                                                      | 
      }                                                                                                 | 
      Pool2 = (_DWORD *)ExAllocatePool2(0x40uLL, v11, 0x72766E45u);                                     | 
      v15 = Pool2;                                                                                      | 
      P = Pool2;                                                                                        | 
      if ( !Pool2 )                                                                                     | 
      {                                                                                                 | 
        return 3221225626LL;                                                                            | 
      }                                                                                                 | 
      memmove(Pool2, Src, v11);                                                                         | 
      v15[1] = v11;                                                                                     | 
      if ( v13 )                                                                                        | 
      {                                                                                                 | 
        v16 = (void *)ExAllocatePool2(0x40uLL, v13, 0x72766E45u);                                       | 
        v29 = v16;                                                                                      | 
        if ( !v16 )                                                                                     | 
        {                                                                                               | 
          ExFreePoolWithTag(v15, 0);                                                                    | 
          P = 0LL;                                                                                      | 
          return 3221225626LL;                                                                          | 
        }                                                                                               | 
      }                                                                                                 | 
      else                                                                                              | 
      {                                                                                                 | 
        v16 = 0LL;                                                                                      | 
        v29 = 0LL;                                                                                      | 
      }                                                                                                 | 
      v19 = ExpVerifyFilePath(v15, (char *)v15 + (unsigned int)v15[1]);                                 | 
      if ( v19 >= 0 )                                                                                   | 
      {                                                                                                 | 
        v20 = v15[2];                                                                                   | 
        if ( a2 == v20 )                                                                                | 
        {                                                                                               | 
          if ( v13 < v11 )                                                                              | 
          {                                                                                             | 
            v19 = -1073741789;                                                                          | 
          }                                                                                             | 
          else                                                                                          | 
          {                                                                                             | 
            memmove(v16, v15, v11);                                                                     | 
          }                                                                                             | 
          v13 = v11;                                                                                    | 
          v26 = v11;                                                                                    | 
        }                                                                                               | 
        else                                                                                            | 
        {                                                                                               | 
          v21 = v20 - 1;                                                                                | 
          if ( v21 && (v22 = v21 - 1) != 0 )                                                            | 
          {                                                                                             | 
            v23 = v22 - 1;                                                                              | 
            if ( v23 )                                                                                  | 
            {                                                                                           | 
              if ( v23 != 1 )                                                                           | 
              {                                                                                         | 
                v19 = -1073741811;                                                                      | 
                goto LABEL_32;                                                                          | 
              }                                                                                         | 
              v24 = ExpTranslateEfiPath(v15, a2, v16, &v26);                                            | 
            }                                                                                           | 
            else                                                                                        | 
            {                                                                                           | 
              v24 = ExpTranslateNtPath(v15, a2, v16, &v26);                                             | 
            }                                                                                           | 
          }                                                                                             | 
          else                                                                                          | 
          {                                                                                             | 
            v24 = ExpTranslateArcPath(v15, a2, v16, &v26);                                              | 
          }                                                                                             | 
          v19 = v24;                                                                                    | 
          v13 = v26;                                                                                    | 
        }                                                                                               | 
      }                                                                                                 | 
LABEL_32:                                                                                               | 
      ExFreePoolWithTag(v15, 0);                                                                        | 
      if ( v19 >= 0 )                                                                                   | 
      {                                                                                                 | 
        if ( !v16 )                                                                                     | 
        {                                                                                               | 
LABEL_37:                                                                                               | 
          *a4 = v13;                                                                                    | 
          return (unsigned int)v19;                                                                     | 
        }                                                                                               | 
        memmove((void *)a3, v16, v13);                                                                  | 
      }                                                                                                 | 
      if ( v16 )                                                                                        | 
      {                                                                                                 | 
        ExFreePoolWithTag(v16, 0);                                                                      | 
        v29 = 0LL;                                                                                      | 
      }                                                                                                 | 
      goto LABEL_37;                                                                                    | 
    }                                                                                                   | 
    return 3221225474LL;                                                                                | 
  }                                                                                                     | 
  return 3221225485LL;                                                                                  | 
}                                                                                                       | 
167 row(s)
