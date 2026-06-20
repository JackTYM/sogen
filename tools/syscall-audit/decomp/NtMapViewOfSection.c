__int64 __fastcall NtMapViewOfSection(                                                                       | 
        __int64 a1,                                                                                          | 
        __int64 a2,                                                                                          | 
        _QWORD *a3,                                                                                          | 
        ULONGLONG a4,                                                                                        | 
        int a5,                                                                                              | 
        _QWORD *a6,                                                                                          | 
        _QWORD *a7,                                                                                          | 
        int a8,                                                                                              | 
        int a9,                                                                                              | 
        int a10)                                                                                             | 
{                                                                                                            | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                              | 
                                                                                                             | 
  v10 = a4;                                                                                                  | 
  v40 = 0LL;                                                                                                 | 
  v41 = 0LL;                                                                                                 | 
  *(_OWORD *)Object = 0LL;                                                                                   | 
  if ( a4 )                                                                                                  | 
  {                                                                                                          | 
    v10 = a4 < 0x20 ? a4 + 32 : 63 - RtlFindMostSignificantBit(a4);                                          | 
    if ( v10 > 0x35 )                                                                                        | 
    {                                                                                                        | 
      return 3221225485LL;                                                                                   | 
    }                                                                                                        | 
  }                                                                                                          | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                         | 
  result = MiMapViewOfSectionCommon(a2, a1, 0LL, a3, a7, a6, a10, v10, PreviousMode, &v40);                  | 
  if ( (int)result >= 0 )                                                                                    | 
  {                                                                                                          | 
    v15 = MiSectionControlArea(Object[0]);                                                                   | 
    v47 = 0LL;                                                                                               | 
    v48 = 0LL;                                                                                               | 
    v49 = 0LL;                                                                                               | 
    v50 = 0LL;                                                                                               | 
    v51 = 0LL;                                                                                               | 
    v52 = 0LL;                                                                                               | 
    v53 = 0LL;                                                                                               | 
    v54 = 0LL;                                                                                               | 
    *(_QWORD *)&v53 = KeGetCurrentThread()->ApcState.Process;                                                | 
    *((PVOID *)&v52 + 1) = Object[1];                                                                        | 
    BYTE9(v50) = KeGetCurrentThread()->PreviousMode;                                                         | 
    DWORD1(v50) = a9 & 0x7F;                                                                                 | 
    if ( DWORD1(v50) > (unsigned __int16)KeNumberNodes )                                                     | 
    {                                                                                                        | 
      v19 = -1073741811;                                                                                     | 
      v20 = (_QWORD)v40 == 0LL;                                                                              | 
    }                                                                                                        | 
    else                                                                                                     | 
    {                                                                                                        | 
      DWORD2(v49) = a9 & 0xFFFFFF80;                                                                         | 
      HIDWORD(v49) = a10;                                                                                    | 
      LODWORD(v51) = 0;                                                                                      | 
      v16 = BYTE12(v50);                                                                                     | 
      if ( (_QWORD)v40 )                                                                                     | 
      {                                                                                                      | 
        v16 = BYTE12(v50) | 1;                                                                               | 
        HIDWORD(v50) |= 1u;                                                                                  | 
      }                                                                                                      | 
      *((_QWORD *)&v48 + 1) = *((_QWORD *)&v40 + 1);                                                         | 
      if ( (*(_DWORD *)(v15 + 56) & 0x20) == 0 && (v16 & 1) != 0 )                                           | 
      {                                                                                                      | 
        v18 = *((_QWORD *)Object[1] + 109) - 1LL;                                                            | 
        if ( v18 >= 0x7FFFFFFEFFFFLL )                                                                       | 
        {                                                                                                    | 
          v18 = 0x7FFFFFFEFFFFLL;                                                                            | 
        }                                                                                                    | 
      }                                                                                                      | 
      else                                                                                                   | 
      {                                                                                                      | 
        v17 = 0x7FFFFFFEFFFFLL;                                                                              | 
        if ( v10 )                                                                                           | 
        {                                                                                                    | 
          v37 = 0xFFFFFFFFFFFFFFFFuLL >> v10;                                                                | 
          if ( 0xFFFFFFFFFFFFFFFFuLL >> v10 > 0x7FFFFFFEFFFFLL )                                             | 
          {                                                                                                  | 
            v37 = 0x7FFFFFFEFFFFLL;                                                                          | 
          }                                                                                                  | 
          v17 = v37;                                                                                         | 
        }                                                                                                    | 
        v18 = *((_QWORD *)Object[1] + 109) - 1LL;                                                            | 
        if ( v17 <= v18 )                                                                                    | 
        {                                                                                                    | 
          v18 = v17;                                                                                         | 
        }                                                                                                    | 
      }                                                                                                      | 
      *((_QWORD *)&v47 + 1) = v18;                                                                           | 
      *(_QWORD *)&v48 = 0x10000LL;                                                                           | 
      if ( (*(_DWORD *)(v15 + 56) & 0x420) == 0 )                                                            | 
      {                                                                                                      | 
        if ( (a9 & 0x20000000) != 0 )                                                                        | 
        {                                                                                                    | 
          *(_QWORD *)&v48 = 0x200000LL;                                                                      | 
        }                                                                                                    | 
        else if ( (a9 & 0x40000000) != 0 )                                                                   | 
        {                                                                                                    | 
          *(_QWORD *)&v48 = 4096LL;                                                                          | 
        }                                                                                                    | 
      }                                                                                                      | 
      v19 = MiMapViewOfSection(Object[0], (unsigned int)&v47, (unsigned int)&v40, a5, (__int64)&v41, a8, 0); | 
      v44[2] = v19;                                                                                          | 
      if ( v19 >= 0 )                                                                                        | 
      {                                                                                                      | 
        if ( (BYTE12(v50) & 4) != 0 )                                                                        | 
        {                                                                                                    | 
          DbgkMapViewOfSection(Object[1], UserData, 0, 0);                                                   | 
        }                                                                                                    | 
        if ( (*((_DWORD *)Object[0] + 14) & 0x20) == 0 && (BYTE8(v41) & 2) != 0 )                            | 
        {                                                                                                    | 
          v44[0] = a10;                                                                                      | 
          v43 = a9;                                                                                          | 
          v46 = *((_QWORD *)&v48 + 1);                                                                       | 
          v45 = v40;                                                                                         | 
          v21 = (_KPROCESS *)Object[1];                                                                      | 
          if ( EtwProviderEnabled(EtwThreatIntProvRegHandle, 0, 0xF00uLL) )                                  | 
          {                                                                                                  | 
            Process = KeGetCurrentThread()->ApcState.Process;                                                | 
            v23 = 0LL;                                                                                       | 
            if ( Process == v21 )                                                                            | 
            {                                                                                                | 
              v23 = 1LL;                                                                                     | 
            }                                                                                                | 
            v24 = 2LL;                                                                                       | 
            if ( PreviousMode )                                                                              | 
            {                                                                                                | 
              v24 = 0LL;                                                                                     | 
            }                                                                                                | 
            v25 = (const EVENT_DESCRIPTOR *)off_1400017A0[v24 + v23];                                        | 
            if ( EtwEventEnabled(EtwThreatIntProvRegHandle, v25) )                                           | 
            {                                                                                                | 
              v26 = EtwpTiFillProcessIdentity(v57, Process, v55);                                            | 
              v28 = EtwpTiFillThreadIdentity(&v57[v26], KeGetCurrentThread(), v27, v26);                     | 
              v30 = EtwpTiFillProcessIdentity(&v57[v28 + v29], v21, v56);                                    | 
              v32 = v30 + v31;                                                                               | 
              v33 = v32;                                                                                     | 
              v57[v33].Ptr = (ULONGLONG)&v45;                                                                | 
              *(_QWORD *)&v57[v33].Size = 8LL;                                                               | 
              v34 = ++v32;                                                                                   | 
              v57[v34].Ptr = (ULONGLONG)&v46;                                                                | 
              *(_QWORD *)&v57[v34].Size = 8LL;                                                               | 
              v35 = ++v32;                                                                                   | 
              v57[v35].Ptr = (ULONGLONG)&v43;                                                                | 
              *(_QWORD *)&v57[v35].Size = 4LL;                                                               | 
              v36 = ++v32;                                                                                   | 
              v57[v36].Ptr = (ULONGLONG)v44;                                                                 | 
              *(_QWORD *)&v57[v36].Size = 4LL;                                                               | 
              EtwWrite(EtwThreatIntProvRegHandle, v25, 0LL, v32 + 1, v57);                                   | 
            }                                                                                                | 
          }                                                                                                  | 
        }                                                                                                    | 
        *a7 = *((_QWORD *)&v48 + 1);                                                                         | 
        *a3 = v40;                                                                                           | 
        if ( a6 )                                                                                            | 
        {                                                                                                    | 
          *a6 = v41;                                                                                         | 
        }                                                                                                    | 
        goto LABEL_20;                                                                                       | 
      }                                                                                                      | 
      v20 = (_QWORD)v40 == 0LL;                                                                              | 
    }                                                                                                        | 
    if ( v20 )                                                                                               | 
    {                                                                                                        | 
      ++dword_140E301E8;                                                                                     | 
    }                                                                                                        | 
    else                                                                                                     | 
    {                                                                                                        | 
      ++dword_140E301EC;                                                                                     | 
    }                                                                                                        | 
LABEL_20:                                                                                                    | 
    ObfDereferenceObjectWithTag(Object[0], 0x77566D4Du);                                                     | 
    ObfDereferenceObjectWithTag(Object[1], 0x77566D4Du);                                                     | 
    return (unsigned int)v19;                                                                                | 
  }                                                                                                          | 
  ++dword_140E301E8;                                                                                         | 
  return result;                                                                                             | 
}                                                                                                            | 
