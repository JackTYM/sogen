__int64 __fastcall NtQueryDirectoryObject(                                                                                | 
        HANDLE Handle,                                                                                                    | 
        char *Address,                                                                                                    | 
        SIZE_T Length,                                                                                                    | 
        char a4,                                                                                                          | 
        char a5,                                                                                                          | 
        int *a6,                                                                                                          | 
        unsigned int *a7)                                                                                                 | 
{                                                                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                           | 
                                                                                                                          | 
  v60 = Length;                                                                                                           | 
  v7 = (unsigned int)Length;                                                                                              | 
  DestinationString = 0LL;                                                                                                | 
  v57 = 0LL;                                                                                                              | 
  v58 = 0LL;                                                                                                              | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                      | 
  if ( PreviousMode )                                                                                                     | 
  {                                                                                                                       | 
    ProbeForWrite(Address, (unsigned int)Length, 2u);                                                                     | 
    v10 = 0x7FFFFFFF0000LL;                                                                                               | 
    v11 = 0x7FFFFFFF0000LL;                                                                                               | 
    if ( (unsigned __int64)a6 < 0x7FFFFFFF0000LL )                                                                        | 
    {                                                                                                                     | 
      v11 = (__int64)a6;                                                                                                  | 
    }                                                                                                                     | 
    *(_DWORD *)v11 = *(_DWORD *)v11;                                                                                      | 
    if ( a7 )                                                                                                             | 
    {                                                                                                                     | 
      if ( (unsigned __int64)a7 < 0x7FFFFFFF0000LL )                                                                      | 
      {                                                                                                                   | 
        v10 = (__int64)a7;                                                                                                | 
      }                                                                                                                   | 
      *(_DWORD *)v10 = *(_DWORD *)v10;                                                                                    | 
    }                                                                                                                     | 
    if ( a5 )                                                                                                             | 
    {                                                                                                                     | 
      v46 = 0;                                                                                                            | 
    }                                                                                                                     | 
    else                                                                                                                  | 
    {                                                                                                                     | 
      v46 = *a6;                                                                                                          | 
    }                                                                                                                     | 
  }                                                                                                                       | 
  else if ( a5 )                                                                                                          | 
  {                                                                                                                       | 
    v46 = 0;                                                                                                              | 
  }                                                                                                                       | 
  else                                                                                                                    | 
  {                                                                                                                       | 
    v46 = *a6;                                                                                                            | 
  }                                                                                                                       | 
  if ( (unsigned int)v7 >= (int)v7 + 32 )                                                                                 | 
  {                                                                                                                       | 
    return 3221225485LL;                                                                                                  | 
  }                                                                                                                       | 
  Pool2 = (void *)ExAllocatePool2(0x101uLL, v7 + 32, 0x6D4E624Fu);                                                        | 
  Src = Pool2;                                                                                                            | 
  if ( !Pool2 )                                                                                                           | 
  {                                                                                                                       | 
    return 3221225626LL;                                                                                                  | 
  }                                                                                                                       | 
  Object = 0LL;                                                                                                           | 
  v14 = ObReferenceObjectByHandle(Handle, 1u, ObpDirectoryObjectType, PreviousMode, &Object, 0LL);                        | 
  if ( v14 < 0 )                                                                                                          | 
  {                                                                                                                       | 
    ExFreePoolWithTag(Pool2, 0);                                                                                          | 
    return (unsigned int)v14;                                                                                             | 
  }                                                                                                                       | 
  v54 = (unsigned __int16 *)Pool2;                                                                                        | 
  v15 = 32;                                                                                                               | 
  v49 = 32;                                                                                                               | 
  v16 = 0;                                                                                                                | 
  v47 = 0;                                                                                                                | 
  v48 = 0;                                                                                                                | 
  v17 = -2147483622;                                                                                                      | 
  CurrentThread = KeGetCurrentThread();                                                                                   | 
  --CurrentThread->KernelApcDisable;                                                                                      | 
  v19 = (volatile signed __int64 *)((char *)Object + 296);                                                                | 
  v20 = KeAbPreAcquire((char *)Object + 296, 0LL, 0LL);                                                                   | 
  if ( _InterlockedCompareExchange64(v19, 17LL, 0LL) )                                                                    | 
  {                                                                                                                       | 
    ExfAcquirePushLockSharedEx(v19, 0LL, v20, v19);                                                                       | 
  }                                                                                                                       | 
  if ( v20 )                                                                                                              | 
  {                                                                                                                       | 
    *(_BYTE *)(v20 + 10) = 1;                                                                                             | 
  }                                                                                                                       | 
  v21 = 0;                                                                                                                | 
  v51 = 0;                                                                                                                | 
  v22 = Object;                                                                                                           | 
  v55 = Object;                                                                                                           | 
  v23 = v46;                                                                                                              | 
  while ( 1 )                                                                                                             | 
  {                                                                                                                       | 
    v24 = (_QWORD *)*v22;                                                                                                 | 
    if ( *v22 )                                                                                                           | 
    {                                                                                                                     | 
      break;                                                                                                              | 
    }                                                                                                                     | 
LABEL_39:                                                                                                                 | 
    v51 = ++v21;                                                                                                          | 
    v55 = ++v22;                                                                                                          | 
    if ( v21 >= 0x25 )                                                                                                    | 
    {                                                                                                                     | 
      goto LABEL_47;                                                                                                      | 
    }                                                                                                                     | 
  }                                                                                                                       | 
  while ( 1 )                                                                                                             | 
  {                                                                                                                       | 
    v25 = v16;                                                                                                            | 
    v53 = v16;                                                                                                            | 
    v26 = v16++;                                                                                                          | 
    v47 = v16;                                                                                                            | 
    if ( v23 == v26 )                                                                                                     | 
    {                                                                                                                     | 
      break;                                                                                                              | 
    }                                                                                                                     | 
LABEL_37:                                                                                                                 | 
    v24 = (_QWORD *)*v24;                                                                                                 | 
    if ( !v24 )                                                                                                           | 
    {                                                                                                                     | 
      v22 = v55;                                                                                                          | 
      v21 = v51;                                                                                                          | 
      goto LABEL_39;                                                                                                      | 
    }                                                                                                                     | 
  }                                                                                                                       | 
  v27 = v24[1];                                                                                                           | 
  if ( (*(_BYTE *)(v27 - 22) & 2) != 0 )                                                                                  | 
  {                                                                                                                       | 
    v28 = v27 - 48 - ObpInfoMaskToOffset[*(_BYTE *)(v27 - 22) & 3];                                                       | 
  }                                                                                                                       | 
  else                                                                                                                    | 
  {                                                                                                                       | 
    v28 = 0LL;                                                                                                            | 
  }                                                                                                                       | 
  if ( v28 )                                                                                                              | 
  {                                                                                                                       | 
    DestinationString = *(UNICODE_STRING *)(v28 + 8);                                                                     | 
  }                                                                                                                       | 
  else                                                                                                                    | 
  {                                                                                                                       | 
    RtlInitUnicodeString(&DestinationString, 0LL);                                                                        | 
    v25 = v53;                                                                                                            | 
  }                                                                                                                       | 
  v29 = (unsigned __int8)((unsigned __int16)(v27 - 48) >> 8);                                                             | 
  v30 = (unsigned __int8)ObHeaderCookie;                                                                                  | 
  v31 = DestinationString.Length;                                                                                         | 
  v32 = DestinationString.Length                                                                                          | 
      + 36                                                                                                                | 
      + v15                                                                                                               | 
      + *(unsigned __int16 *)(ObTypeIndexTable[*(unsigned __int8 *)(v27 - 24) ^ v29 ^ (unsigned __int8)ObHeaderCookie]    | 
                            + 16);                                                                                        | 
  if ( v15 > v32 )                                                                                                        | 
  {                                                                                                                       | 
    v17 = 261;                                                                                                            | 
    v16 = v25;                                                                                                            | 
    v47 = v25;                                                                                                            | 
    goto LABEL_47;                                                                                                        | 
  }                                                                                                                       | 
  if ( v32 <= v60 )                                                                                                       | 
  {                                                                                                                       | 
    v33 = v54;                                                                                                            | 
    *v54 = DestinationString.Length;                                                                                      | 
    v33[1] = v31 + 2;                                                                                                     | 
    *((_QWORD *)v33 + 1) = DestinationString.Buffer;                                                                      | 
    v33[8] = *(_WORD *)(ObTypeIndexTable[v29 ^ v30 ^ *(unsigned __int8 *)(v27 - 24)] + 16);                               | 
    v33[9] = *(_WORD *)(ObTypeIndexTable[v29 ^ v30 ^ *(unsigned __int8 *)(v27 - 24)] + 16) + 2;                           | 
    *((_QWORD *)v33 + 3) = *(_QWORD *)(ObTypeIndexTable[v29 ^ v30 ^ *(unsigned __int8 *)(v27 - 24)] + 24);                | 
    v17 = 0;                                                                                                              | 
    v15 = v32;                                                                                                            | 
    v49 = v32;                                                                                                            | 
    v54 = v33 + 16;                                                                                                       | 
    ++v48;                                                                                                                | 
    if ( a4 )                                                                                                             | 
    {                                                                                                                     | 
      goto LABEL_47;                                                                                                      | 
    }                                                                                                                     | 
    ++v23;                                                                                                                | 
    goto LABEL_37;                                                                                                        | 
  }                                                                                                                       | 
  if ( a4 )                                                                                                               | 
  {                                                                                                                       | 
    v15 += DestinationString.Length                                                                                       | 
         + 36                                                                                                             | 
         + *(unsigned __int16 *)(ObTypeIndexTable[*(unsigned __int8 *)(v27 - 24) ^ v29 ^ (unsigned __int8)ObHeaderCookie] | 
                               + 16);                                                                                     | 
  }                                                                                                                       | 
  v49 = v15;                                                                                                              | 
  v47 = --v16;                                                                                                            | 
  v17 = -1073741789;                                                                                                      | 
  if ( !a4 )                                                                                                              | 
  {                                                                                                                       | 
    v17 = 261;                                                                                                            | 
  }                                                                                                                       | 
LABEL_47:                                                                                                                 | 
  if ( v17 >= 0 )                                                                                                         | 
  {                                                                                                                       | 
    v34 = v54;                                                                                                            | 
    *(_OWORD *)v54 = 0LL;                                                                                                 | 
    *((_OWORD *)v34 + 1) = 0LL;                                                                                           | 
    v35 = (char *)(v34 + 16);                                                                                             | 
    v36 = v48;                                                                                                            | 
    if ( v48 )                                                                                                            | 
    {                                                                                                                     | 
      v37 = (const void **)((char *)Src + 24);                                                                            | 
      v38 = Src;                                                                                                          | 
      do                                                                                                                  | 
      {                                                                                                                   | 
        memmove(v35, *(v37 - 2), *((unsigned __int16 *)v37 - 12));                                                        | 
        *(v37 - 2) = &Address[v35 - v38];                                                                                 | 
        v39 = &v35[*((unsigned __int16 *)v37 - 12)];                                                                      | 
        *(_WORD *)v39 = 0;                                                                                                | 
        v39 += 2;                                                                                                         | 
        memmove(v39, *v37, *((unsigned __int16 *)v37 - 4));                                                               | 
        *v37 = &Address[v39 - v38];                                                                                       | 
        v40 = &v39[*((unsigned __int16 *)v37 - 4)];                                                                       | 
        *(_WORD *)v40 = 0;                                                                                                | 
        v35 = v40 + 2;                                                                                                    | 
        v37 += 4;                                                                                                         | 
        --v36;                                                                                                            | 
      }                                                                                                                   | 
      while ( v36 );                                                                                                      | 
      v16 = v47;                                                                                                          | 
      v15 = v49;                                                                                                          | 
    }                                                                                                                     | 
  }                                                                                                                       | 
  v41 = (volatile signed __int64 *)((char *)Object + 296);                                                                | 
  _m_prefetchw((char *)Object + 296);                                                                                     | 
  v42 = *v41;                                                                                                             | 
  v43 = *v41 - 16;                                                                                                        | 
  if ( (*v41 & 0xFFFFFFFFFFFFFFF0uLL) <= 0x10 )                                                                           | 
  {                                                                                                                       | 
    v43 = 0LL;                                                                                                            | 
  }                                                                                                                       | 
  if ( (v42 & 2) != 0 || (v44 = *v41, v44 != _InterlockedCompareExchange64(v41, v43, v42)) )                              | 
  {                                                                                                                       | 
    ExfReleasePushLock(v41, v43);                                                                                         | 
  }                                                                                                                       | 
  KeAbPostRelease((ULONG_PTR)v41);                                                                                        | 
  if ( BYTE6(v58) )                                                                                                       | 
  {                                                                                                                       | 
    ObDereferenceObjectDeferDeleteWithTag(Object, 0x554C624Fu);                                                           | 
  }                                                                                                                       | 
  KiLeaveCriticalRegionUnsafe(KeGetCurrentThread());                                                                      | 
  v45 = v60;                                                                                                              | 
  if ( v15 <= v60 )                                                                                                       | 
  {                                                                                                                       | 
    v45 = v15;                                                                                                            | 
  }                                                                                                                       | 
  memmove(Address, Src, v45);                                                                                             | 
  if ( a7 )                                                                                                               | 
  {                                                                                                                       | 
    *a7 = v15;                                                                                                            | 
  }                                                                                                                       | 
  if ( v17 >= 0 )                                                                                                         | 
  {                                                                                                                       | 
    *a6 = v16;                                                                                                            | 
  }                                                                                                                       | 
  ObfDereferenceObject(Object);                                                                                           | 
  ExFreePoolWithTag(Src, 0);                                                                                              | 
  return (unsigned int)v17;                                                                                               | 
}                                                                                                                         | 
263 row(s)
