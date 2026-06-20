__int64 __fastcall NtCreateIoRing(unsigned __int64 a1, unsigned int a2, __int128 *a3, unsigned int a4, _OWORD *Address) | 
{                                                                                                                       | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                         | 
                                                                                                                        | 
  v40 = (_QWORD *)a1;                                                                                                   | 
  v25 = 0;                                                                                                              | 
  v7 = KeGetCurrentThread()->PreviousMode;                                                                              | 
  PreviousMode = v7;                                                                                                    | 
  v26 = 0LL;                                                                                                            | 
  v27 = 0;                                                                                                              | 
  Object = 0LL;                                                                                                         | 
  if ( a2 < 0x14 )                                                                                                      | 
  {                                                                                                                     | 
    inserted = -1073741584;                                                                                             | 
    goto LABEL_51;                                                                                                      | 
  }                                                                                                                     | 
  if ( a4 < 0x30 )                                                                                                      | 
  {                                                                                                                     | 
    inserted = -1073741789;                                                                                             | 
    goto LABEL_51;                                                                                                      | 
  }                                                                                                                     | 
  if ( v7 )                                                                                                             | 
  {                                                                                                                     | 
    v9 = 0x7FFFFFFF0000LL;                                                                                              | 
    if ( a1 < 0x7FFFFFFF0000LL )                                                                                        | 
    {                                                                                                                   | 
      v9 = a1;                                                                                                          | 
    }                                                                                                                   | 
    *(_QWORD *)v9 = *(_QWORD *)v9;                                                                                      | 
    ProbeForWrite(Address, a4, 1u);                                                                                     | 
  }                                                                                                                     | 
  v26 = *a3;                                                                                                            | 
  v27 = *((_DWORD *)a3 + 4);                                                                                            | 
  v10 = v26;                                                                                                            | 
  if ( (unsigned int)(v26 - 1) > 0x18F )                                                                                | 
  {                                                                                                                     | 
    inserted = -1069154301;                                                                                             | 
    goto LABEL_51;                                                                                                      | 
  }                                                                                                                     | 
  if ( !DWORD1(v26) )                                                                                                   | 
  {                                                                                                                     | 
    v11 = 32;                                                                                                           | 
LABEL_12:                                                                                                               | 
    v12 = -1;                                                                                                           | 
    goto LABEL_13;                                                                                                      | 
  }                                                                                                                     | 
  v11 = DWORD1(v26);                                                                                                    | 
  if ( DWORD1(v26) < 8 )                                                                                                | 
  {                                                                                                                     | 
    v11 = 8;                                                                                                            | 
    goto LABEL_12;                                                                                                      | 
  }                                                                                                                     | 
  if ( DWORD1(v26) > 0x10000 )                                                                                          | 
  {                                                                                                                     | 
    inserted = -1069154300;                                                                                             | 
    goto LABEL_51;                                                                                                      | 
  }                                                                                                                     | 
  if ( ((DWORD1(v26) - 1) & DWORD1(v26)) == 0 )                                                                         | 
  {                                                                                                                     | 
    goto LABEL_12;                                                                                                      | 
  }                                                                                                                     | 
  v12 = -1;                                                                                                             | 
  v14 = -1;                                                                                                             | 
  do                                                                                                                    | 
  {                                                                                                                     | 
    ++v14;                                                                                                              | 
    v11 >>= 1;                                                                                                          | 
  }                                                                                                                     | 
  while ( v11 );                                                                                                        | 
  v11 = 1 << (v14 + 1);                                                                                                 | 
LABEL_13:                                                                                                               | 
  DWORD1(v26) = v11;                                                                                                    | 
  v13 = DWORD2(v26);                                                                                                    | 
  if ( DWORD2(v26) > 0x20000 )                                                                                          | 
  {                                                                                                                     | 
    inserted = -1069154299;                                                                                             | 
    goto LABEL_51;                                                                                                      | 
  }                                                                                                                     | 
  if ( DWORD2(v26) <= 2 * v11 )                                                                                         | 
  {                                                                                                                     | 
    v13 = 2 * v11;                                                                                                      | 
    DWORD2(v26) = 2 * v11;                                                                                              | 
  }                                                                                                                     | 
  else                                                                                                                  | 
  {                                                                                                                     | 
    if ( ((DWORD2(v26) - 1) & DWORD2(v26)) != 0 )                                                                       | 
    {                                                                                                                   | 
      if ( DWORD2(v26) )                                                                                                | 
      {                                                                                                                 | 
        do                                                                                                              | 
        {                                                                                                               | 
          ++v12;                                                                                                        | 
          v13 >>= 1;                                                                                                    | 
        }                                                                                                               | 
        while ( v13 );                                                                                                  | 
      }                                                                                                                 | 
      v13 = 1 << (v12 + 1);                                                                                             | 
    }                                                                                                                   | 
    DWORD2(v26) = v13;                                                                                                  | 
  }                                                                                                                     | 
  if ( HIDWORD(v26) )                                                                                                   | 
  {                                                                                                                     | 
    inserted = -1069154303;                                                                                             | 
    goto LABEL_51;                                                                                                      | 
  }                                                                                                                     | 
  v43[0] = 48LL;                                                                                                        | 
  memset(&v43[1], 0, 24);                                                                                               | 
  v44 = 0LL;                                                                                                            | 
  LOBYTE(a4) = v7;                                                                                                      | 
  inserted = ObCreateObjectEx(0, (_DWORD)IoRingObjectType, (unsigned int)v43, a4);                                      | 
  v25 = inserted;                                                                                                       | 
  if ( inserted < 0 )                                                                                                   | 
  {                                                                                                                     | 
    goto LABEL_43;                                                                                                      | 
  }                                                                                                                     | 
  memset_0(Object, 0, 0xD0uLL);                                                                                         | 
  *(_WORD *)Object = 14;                                                                                                | 
  *((_WORD *)Object + 1) = 208;                                                                                         | 
  *((_DWORD *)Object + 2) = v10;                                                                                        | 
  *((_DWORD *)Object + 5) = v11;                                                                                        | 
  *((_DWORD *)Object + 7) = v13;                                                                                        | 
  *((_DWORD *)Object + 3) = 0;                                                                                          | 
  *((_DWORD *)Object + 4) = 0;                                                                                          | 
  *((_DWORD *)Object + 6) = v11 - 1;                                                                                    | 
  *((_DWORD *)Object + 8) = v13 - 1;                                                                                    | 
  *((_QWORD *)Object + 13) = 0LL;                                                                                       | 
  KeInitializeEvent((PRKEVENT)((char *)Object + 136), SynchronizationEvent, 0);                                         | 
  v15 = (((unsigned __int64)v11 << 6) + 87) & 0xFFFFFFFFFFFFFFF8uLL;                                                    | 
  v28 = v15 + 8 * (v13 + 2 * (v13 + 2LL));                                                                              | 
  inserted = MmCreateSection((char *)Object + 56, 6LL, 0LL, &v28, 4, 0x8000000, 0LL, 0LL);                              | 
  v25 = inserted;                                                                                                       | 
  if ( inserted < 0 )                                                                                                   | 
  {                                                                                                                     | 
    goto LABEL_43;                                                                                                      | 
  }                                                                                                                     | 
  inserted = MmMapViewInSessionSpace(*((PVOID *)Object + 7), (PVOID *)Object + 8, (PSIZE_T)Object + 11);                | 
  v25 = inserted;                                                                                                       | 
  if ( inserted < 0 )                                                                                                   | 
  {                                                                                                                     | 
    goto LABEL_43;                                                                                                      | 
  }                                                                                                                     | 
  inserted = IopAllocateAndLockMdl(v15 + *((_QWORD *)Object + 8), (unsigned int)(*((_DWORD *)Object + 22) - v15), 0LL); | 
  v25 = inserted;                                                                                                       | 
  if ( inserted < 0 )                                                                                                   | 
  {                                                                                                                     | 
    goto LABEL_43;                                                                                                      | 
  }                                                                                                                     | 
  if ( v7 )                                                                                                             | 
  {                                                                                                                     | 
    v37 = 0LL;                                                                                                          | 
    v38 = 0LL;                                                                                                          | 
    v28 = 0LL;                                                                                                          | 
    inserted = MmMapViewOfSection(                                                                                      | 
                 *((_QWORD *)Object + 7),                                                                               | 
                 KeGetCurrentThread()->ApcState.Process,                                                                | 
                 (unsigned int)&v28,                                                                                    | 
                 0,                                                                                                     | 
                 0LL,                                                                                                   | 
                 (__int64)&v37,                                                                                         | 
                 (__int64)&v38,                                                                                         | 
                 1,                                                                                                     | 
                 0,                                                                                                     | 
                 4);                                                                                                    | 
    v25 = inserted;                                                                                                     | 
    if ( inserted < 0 )                                                                                                 | 
    {                                                                                                                   | 
      goto LABEL_43;                                                                                                    | 
    }                                                                                                                   | 
    v16 = v28 + v15;                                                                                                    | 
    *((_QWORD *)Object + 5) = v28;                                                                                      | 
    *((_QWORD *)Object + 6) = v16;                                                                                      | 
  }                                                                                                                     | 
  Handle = 0LL;                                                                                                         | 
  inserted = ObInsertObjectEx(Object, 0, (__int64)&Object, (__int64)&Handle);                                           | 
  v25 = inserted;                                                                                                       | 
  if ( inserted >= 0 )                                                                                                  | 
  {                                                                                                                     | 
    *v40 = Handle;                                                                                                      | 
    v17 = (char *)Object;                                                                                               | 
    *Address = *(_OWORD *)((char *)Object + 8);                                                                         | 
    Address[1] = *(_OWORD *)(v17 + 24);                                                                                 | 
    Address[2] = *(_OWORD *)(v17 + 40);                                                                                 | 
  }                                                                                                                     | 
  else                                                                                                                  | 
  {                                                                                                                     | 
    Object = 0LL;                                                                                                       | 
  }                                                                                                                     | 
LABEL_43:                                                                                                               | 
  if ( inserted >= 0 && Object )                                                                                        | 
  {                                                                                                                     | 
    ProcessImageFileName = PsGetProcessImageFileName(KeGetCurrentThread()->ApcState.Process);                           | 
    if ( (unsigned int)dword_140E06E48 > 5                                                                              | 
      && (unsigned __int8)tlgKeywordOn(&dword_140E06E48, 0x400000000000LL, Object, ProcessImageFileName) )              | 
    {                                                                                                                   | 
      v41 = 1LL;                                                                                                        | 
      v46 = &v41;                                                                                                       | 
      v47 = 8LL;                                                                                                        | 
      v19 = "UserMode";                                                                                                 | 
      if ( !v7 )                                                                                                        | 
      {                                                                                                                 | 
        v19 = "KernelMode";                                                                                             | 
      }                                                                                                                 | 
      tlgCreate1Sz_char(v48, v19);                                                                                      | 
      tlgCreate1Sz_char(v49, v20);                                                                                      | 
      v29 = v26;                                                                                                        | 
      v50 = &v29;                                                                                                       | 
      v51 = 4LL;                                                                                                        | 
      v30 = HIDWORD(v26);                                                                                               | 
      v52 = &v30;                                                                                                       | 
      v53 = 4LL;                                                                                                        | 
      v31 = v27;                                                                                                        | 
      v54 = &v31;                                                                                                       | 
      v55 = 4LL;                                                                                                        | 
      v32 = *(_QWORD *)((char *)&v26 + 4);                                                                              | 
      v56 = &v32;                                                                                                       | 
      v57 = 4LL;                                                                                                        | 
      v58 = (char *)&v32 + 4;                                                                                           | 
      v59 = 4LL;                                                                                                        | 
      v33 = v21[2];                                                                                                     | 
      v60 = &v33;                                                                                                       | 
      v61 = 4LL;                                                                                                        | 
      v34 = v21[3];                                                                                                     | 
      v62 = &v34;                                                                                                       | 
      v63 = 4LL;                                                                                                        | 
      v35 = v21[4];                                                                                                     | 
      v64 = &v35;                                                                                                       | 
      v65 = 4LL;                                                                                                        | 
      v36 = v21[5];                                                                                                     | 
      v66 = &v36;                                                                                                       | 
      v67 = 4LL;                                                                                                        | 
      LODWORD(v37) = v21[6];                                                                                            | 
      v68 = &v37;                                                                                                       | 
      v69 = 4LL;                                                                                                        | 
      LODWORD(v38) = v21[7];                                                                                            | 
      v70 = &v38;                                                                                                       | 
      v71 = 4LL;                                                                                                        | 
      LODWORD(v28) = v21[8];                                                                                            | 
      v72 = &v28;                                                                                                       | 
      v73 = 4LL;                                                                                                        | 
      v42 = 0x1000000LL;                                                                                                | 
      v74 = &v42;                                                                                                       | 
      v75 = 8LL;                                                                                                        | 
      tlgWriteAgg((unsigned int)&dword_140E06E48, (unsigned int)&byte_1400455DF, (_DWORD)v21, 18, (__int64)v45);        | 
    }                                                                                                                   | 
    inserted = v25;                                                                                                     | 
  }                                                                                                                     | 
LABEL_51:                                                                                                               | 
  if ( Object )                                                                                                         | 
  {                                                                                                                     | 
    ObfDereferenceObject(Object);                                                                                       | 
  }                                                                                                                     | 
  return (unsigned int)inserted;                                                                                        | 
}                                                                                                                       | 
253 row(s)
