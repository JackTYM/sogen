__int64 __fastcall NtAreMappedFilesTheSame(unsigned __int64 a1, unsigned __int64 a2)             | 
{                                                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                  | 
                                                                                                 | 
  CurrentThread = KeGetCurrentThread();                                                          | 
  v5 = MiObtainReferencedVadEx(a1, 2LL, &v20);                                                   | 
  v6 = v5;                                                                                       | 
  if ( !v5 )                                                                                     | 
  {                                                                                              | 
    return 3221225793LL;                                                                         | 
  }                                                                                              | 
  MiUnlockVadShared(CurrentThread, v5);                                                          | 
  v7 = MiObtainReferencedVadEx(a2, 2LL, &v20);                                                   | 
  v8 = v7;                                                                                       | 
  if ( !v7 )                                                                                     | 
  {                                                                                              | 
    MiLockVadShared(CurrentThread, v6);                                                          | 
    MiUnlockAndDereferenceVadShared(v6);                                                         | 
    return 3221225793LL;                                                                         | 
  }                                                                                              | 
  if ( v6 == v7 )                                                                                | 
  {                                                                                              | 
    MiDereferenceVad(v6);                                                                        | 
    MiUnlockAndDereferenceVadShared(v8);                                                         | 
    return 0LL;                                                                                  | 
  }                                                                                              | 
  else if ( (unsigned int)MiVadIsMetadataBitmap(v6) || (unsigned int)MiVadIsMetadataBitmap(v8) ) | 
  {                                                                                              | 
    MiUnlockAndDereferenceVadShared(v8);                                                         | 
    MiLockVadShared(CurrentThread, v6);                                                          | 
    MiUnlockAndDereferenceVadShared(v6);                                                         | 
    return 3221225496LL;                                                                         | 
  }                                                                                              | 
  else                                                                                           | 
  {                                                                                              | 
    if ( a1 <= a2 )                                                                              | 
    {                                                                                            | 
      MiUnlockVadShared(CurrentThread, v8);                                                      | 
      MiLockVadShared(CurrentThread, v6);                                                        | 
      v10 = (volatile signed __int64 *)(v8 + 40);                                                | 
      v18 = KeAbPreAcquire(v8 + 40, 0LL, 0LL);                                                   | 
      if ( _InterlockedCompareExchange64((volatile signed __int64 *)(v8 + 40), 17LL, 0LL) )      | 
      {                                                                                          | 
        ExfAcquirePushLockSharedEx(v8 + 40, 0LL, v18, v8 + 40);                                  | 
      }                                                                                          | 
      if ( v18 )                                                                                 | 
      {                                                                                          | 
        *(_BYTE *)(v18 + 10) = 1;                                                                | 
      }                                                                                          | 
    }                                                                                            | 
    else                                                                                         | 
    {                                                                                            | 
      v9 = KeAbPreAcquire(v6 + 40, 0LL, 0LL);                                                    | 
      if ( _InterlockedCompareExchange64((volatile signed __int64 *)(v6 + 40), 17LL, 0LL) )      | 
      {                                                                                          | 
        ExfAcquirePushLockSharedEx(v6 + 40, 0LL, v9, v6 + 40);                                   | 
      }                                                                                          | 
      if ( v9 )                                                                                  | 
      {                                                                                          | 
        *(_BYTE *)(v9 + 10) = 1;                                                                 | 
      }                                                                                          | 
      v10 = (volatile signed __int64 *)(v8 + 40);                                                | 
    }                                                                                            | 
    if ( (unsigned int)MiVadDeleted(v6) || (unsigned int)MiVadDeleted(v8) )                      | 
    {                                                                                            | 
      v16 = -1073741503;                                                                         | 
      v15 = -1073741503;                                                                         | 
    }                                                                                            | 
    else if ( !_bittest((const signed __int32 *)(v6 + 48), 0x15u)                                | 
           && !_bittest((const signed __int32 *)(v8 + 48), 0x15u)                                | 
           && (v11 = *(__int64 **)(v6 + 72)) != 0LL                                              | 
           && (v12 = *(__int64 **)(v8 + 72)) != 0LL                                              | 
           && (v13 = *v11) != 0                                                                  | 
           && (v14 = *v12) != 0                                                                  | 
           && *(_QWORD *)(v13 + 64)                                                              | 
           && *(_QWORD *)(v14 + 64) )                                                            | 
    {                                                                                            | 
      v17 = MiReferenceControlAreaFile(v14);                                                     | 
      v16 = -1073741612;                                                                         | 
      v15 = -1073741612;                                                                         | 
      if ( v13 == *(_QWORD *)(*(_QWORD *)(v17 + 40) + 16LL) )                                    | 
      {                                                                                          | 
        v16 = 0;                                                                                 | 
        v15 = 0;                                                                                 | 
      }                                                                                          | 
      MiDereferenceControlAreaFile(v14, v17);                                                    | 
    }                                                                                            | 
    else                                                                                         | 
    {                                                                                            | 
      v15 = -1073741800;                                                                         | 
      v16 = -1073741800;                                                                         | 
    }                                                                                            | 
    if ( _InterlockedCompareExchange64(v10, 0LL, 17LL) != 17 )                                   | 
    {                                                                                            | 
      ExfReleasePushLockShared(v10);                                                             | 
      v16 = v15;                                                                                 | 
    }                                                                                            | 
    KeAbPostRelease((ULONG_PTR)v10);                                                             | 
    MiUnlockAndDereferenceVadShared(v6);                                                         | 
    MiLockVadShared(CurrentThread, v8);                                                          | 
    MiUnlockAndDereferenceVadShared(v8);                                                         | 
    return v16;                                                                                  | 
  }                                                                                              | 
}                                                                                                | 
