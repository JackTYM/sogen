__int64 __fastcall NtFreeUserPhysicalPages(HANDLE Handle, unsigned __int64 *a2, void *a3)   | 
{                                                                                           | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                             | 
                                                                                            | 
  Src = a3;                                                                                 | 
  v3 = a2;                                                                                  | 
  v40 = a2;                                                                                 | 
  memset(v48, 0, sizeof(v48));                                                              | 
  memset_0(&Mdl, 0, 0x1030uLL);                                                             | 
  v36 = 0LL;                                                                                | 
  v46 = 0LL;                                                                                | 
  v47 = 0LL;                                                                                | 
  v5 = 0;                                                                                   | 
  CurrentThread = KeGetCurrentThread();                                                     | 
  Process = CurrentThread->ApcState.Process;                                                | 
  PreviousMode = CurrentThread->PreviousMode;                                               | 
  if ( PreviousMode )                                                                       | 
  {                                                                                         | 
    v7 = 0x7FFFFFFF0000LL;                                                                  | 
    if ( (unsigned __int64)v3 < 0x7FFFFFFF0000LL )                                          | 
    {                                                                                       | 
      v7 = (__int64)v3;                                                                     | 
    }                                                                                       | 
    *(_QWORD *)v7 = *(_QWORD *)v7;                                                          | 
    v6 = *v3;                                                                               | 
    v37 = *v3;                                                                              | 
    *v3 = 0LL;                                                                              | 
  }                                                                                         | 
  else                                                                                      | 
  {                                                                                         | 
    v6 = *v3;                                                                               | 
    v37 = *v3;                                                                              | 
  }                                                                                         | 
  if ( !v6 )                                                                                | 
  {                                                                                         | 
    return 3221225712LL;                                                                    | 
  }                                                                                         | 
  v9 = 0LL;                                                                                 | 
  v35 = 0LL;                                                                                | 
  p_Mdl = &Mdl;                                                                             | 
  result = MiReferenceAweHandle(Handle, (__int64)&v36);                                     | 
  Irp = 0LL;                                                                                | 
  if ( (int)result >= 0 )                                                                   | 
  {                                                                                         | 
    v13 = v36;                                                                              | 
    if ( v36 )                                                                              | 
    {                                                                                       | 
      v9 = *(_QWORD *)(MiSectionControlArea(v36, v11, 0LL) + 8);                            | 
      v35 = v9;                                                                             | 
    }                                                                                       | 
    else                                                                                    | 
    {                                                                                       | 
      v5 = 4;                                                                               | 
    }                                                                                       | 
    v14 = (ULONG_PTR)Process;                                                               | 
    Object = (volatile signed __int64 *)&Process->Header.Lock;                              | 
    v42 = Irp;                                                                              | 
    if ( v9 || (v9 = Process[2].ActiveProcessors[4].StaticBitmap[4], (v35 = v9) != 0) )     | 
    {                                                                                       | 
      v16 = MiPageChainCount(v9);                                                           | 
      v43 = v16;                                                                            | 
      p_Mdl = (struct _MDL *)Irp;                                                           | 
      v17 = 512LL;                                                                          | 
      if ( v6 > 0x200 )                                                                     | 
      {                                                                                     | 
        v18 = 786432LL;                                                                     | 
        if ( v16 != 1 )                                                                     | 
        {                                                                                   | 
          v18 = 0xFFFFFLL;                                                                  | 
        }                                                                                   | 
        v17 = v6;                                                                           | 
        if ( v6 > v18 )                                                                     | 
        {                                                                                   | 
          v17 = v18;                                                                        | 
        }                                                                                   | 
        while ( v17 > 0x200 )                                                               | 
        {                                                                                   | 
          p_Mdl = IoAllocateMdl(0LL, (_DWORD)v17 << 12, 0, 0, Irp);                         | 
          Irp = 0LL;                                                                        | 
          if ( p_Mdl )                                                                      | 
          {                                                                                 | 
            v5 |= 8u;                                                                       | 
            break;                                                                          | 
          }                                                                                 | 
          v17 >>= 1;                                                                        | 
        }                                                                                   | 
      }                                                                                     | 
      if ( !p_Mdl )                                                                         | 
      {                                                                                     | 
        p_Mdl = &Mdl;                                                                       | 
        v17 = 512LL;                                                                        | 
      }                                                                                     | 
      while ( 1 )                                                                           | 
      {                                                                                     | 
        if ( v17 > v6 )                                                                     | 
        {                                                                                   | 
          v17 = v6;                                                                         | 
        }                                                                                   | 
        p_Mdl->Next = (struct _MDL *)Irp;                                                   | 
        p_Mdl->Size = 8 * ((((v17 << 12) + 4095) >> 12) + 6);                               | 
        p_Mdl->MdlFlags = (__int16)Irp;                                                     | 
        p_Mdl->StartVa = Irp;                                                               | 
        p_Mdl->ByteOffset = (unsigned int)Irp;                                              | 
        p_Mdl->ByteCount = (_DWORD)v17 << 12;                                               | 
        v5 &= ~1u;                                                                          | 
        v19 = p_Mdl + 1;                                                                    | 
        if ( PreviousMode == (_BYTE)Irp )                                                   | 
        {                                                                                   | 
          memmove(v19, Src, 8 * v17);                                                       | 
        }                                                                                   | 
        else                                                                                | 
        {                                                                                   | 
          v15 = MiCaptureUlongPtrArray(v19);                                                | 
          if ( v15 < 0 )                                                                    | 
          {                                                                                 | 
            goto LABEL_62;                                                                  | 
          }                                                                                 | 
          v9 = v35;                                                                         | 
        }                                                                                   | 
        v20 = CurrentThread;                                                                | 
        if ( CurrentThread->ApcState.Process != (_KPROCESS *)v14 )                          | 
        {                                                                                   | 
          KiStackAttachProcess(v14);                                                        | 
          v5 |= 1u;                                                                         | 
          v20 = CurrentThread;                                                              | 
        }                                                                                   | 
        if ( !v13 )                                                                         | 
        {                                                                                   | 
          LOCK_ADDRESS_SPACE(v20, v14);                                                     | 
          if ( (*(_DWORD *)(v14 + 500) & 0x20) != 0 )                                       | 
          {                                                                                 | 
            break;                                                                          | 
          }                                                                                 | 
        }                                                                                   | 
        v15 = MiBuildPhysicalPageFreeChain(v9, p_Mdl, &v46);                                | 
        ByteOffset = p_Mdl->ByteOffset;                                                     | 
        v45 = ByteOffset;                                                                   | 
        Irp = 0LL;                                                                          | 
        if ( ByteOffset )                                                                   | 
        {                                                                                   | 
          if ( !v13 )                                                                       | 
          {                                                                                 | 
            MiReturnProcessCommitment(v14, v43 * (unsigned int)ByteOffset, 0LL);            | 
          }                                                                                 | 
          v42 = (IRP *)((char *)v42 + v17);                                                 | 
        }                                                                                   | 
        if ( !v13 )                                                                         | 
        {                                                                                   | 
          UNLOCK_ADDRESS_SPACE_UNORDERED(CurrentThread, v14);                               | 
          Irp = 0LL;                                                                        | 
        }                                                                                   | 
        ByteCount = p_Mdl->ByteCount;                                                       | 
        v44 = ByteCount;                                                                    | 
        if ( ByteCount )                                                                    | 
        {                                                                                   | 
          if ( v43 == 1 )                                                                   | 
          {                                                                                 | 
            p_Mdl->MdlFlags |= 2u;                                                          | 
            p_Mdl->ByteCount = (_DWORD)ByteCount << 12;                                     | 
            MiPreparePhysicalPagesMdlForFree(p_Mdl, 1LL, Irp);                              | 
            MiFreePagesFromMdl((ULONG_PTR)p_Mdl);                                           | 
            AweInfoPartition = MiGetAweInfoPartition(v35);                                  | 
            v25 = -v44;                                                                     | 
            _InterlockedAdd64((volatile signed __int64 *)(AweInfoPartition + 19624), -v44); | 
            v13 = v36;                                                                      | 
            if ( !v36 )                                                                     | 
            {                                                                               | 
              _InterlockedAdd64(Object + 174, v25);                                         | 
              v13 = v36;                                                                    | 
            }                                                                               | 
            ProcessPartition = MiGetProcessPartition(Process, v23, 0LL);                    | 
            if ( v27 != ProcessPartition )                                                  | 
            {                                                                               | 
              MiReturnCrossPartitionCharges(v27, 4LL, 1LL);                                 | 
              Irp = 0LL;                                                                    | 
            }                                                                               | 
            v14 = (ULONG_PTR)Object;                                                        | 
            v6 = v37;                                                                       | 
          }                                                                                 | 
          else                                                                              | 
          {                                                                                 | 
            MiFreePhysicalPageChain(v35, &v46, Irp);                                        | 
            Irp = 0LL;                                                                      | 
          }                                                                                 | 
        }                                                                                   | 
        v6 -= v45;                                                                          | 
        v37 = v6;                                                                           | 
        if ( v15 || !v6 )                                                                   | 
        {                                                                                   | 
          goto LABEL_62;                                                                    | 
        }                                                                                   | 
        if ( (v5 & 1) != 0 )                                                                | 
        {                                                                                   | 
          KiUnstackDetachProcess(v48, 0LL, Irp);                                            | 
          v5 &= ~1u;                                                                        | 
          Irp = 0LL;                                                                        | 
        }                                                                                   | 
        if ( *(IRP **)(v14 + 784) == Irp )                                                  | 
        {                                                                                   | 
          goto LABEL_59;                                                                    | 
        }                                                                                   | 
        v28 = *(_WORD *)(v14 + 1772);                                                       | 
        if ( v28 == 332 || (v29 = v28 == 452, v30 = (char)Irp, v29) )                       | 
        {                                                                                   | 
          v30 = 1;                                                                          | 
        }                                                                                   | 
        v29 = v30 == 0;                                                                     | 
        v31 = 4LL;                                                                          | 
        if ( v29 )                                                                          | 
        {                                                                                   | 
LABEL_59:                                                                                   | 
          v31 = 8LL;                                                                        | 
        }                                                                                   | 
        Src = (char *)Src + v17 * v31;                                                      | 
        v9 = v35;                                                                           | 
      }                                                                                     | 
      UNLOCK_ADDRESS_SPACE_UNORDERED(CurrentThread, v14);                                   | 
      v15 = -1073741558;                                                                    | 
LABEL_62:                                                                                   | 
      v3 = v40;                                                                             | 
    }                                                                                       | 
    else                                                                                    | 
    {                                                                                       | 
      v15 = -1073741585;                                                                    | 
    }                                                                                       | 
    if ( (v5 & 8) != 0 )                                                                    | 
    {                                                                                       | 
      p_Mdl->MdlFlags &= ~2u;                                                               | 
      IoFreeMdl(p_Mdl);                                                                     | 
    }                                                                                       | 
    if ( (v5 & 1) != 0 )                                                                    | 
    {                                                                                       | 
      KiUnstackDetachProcess(v48, 0LL, Irp);                                                | 
    }                                                                                       | 
    if ( (v5 & 4) == 0 )                                                                    | 
    {                                                                                       | 
      v32 = (void *)v13;                                                                    | 
      if ( !v13 )                                                                           | 
      {                                                                                     | 
        v32 = (void *)v14;                                                                  | 
      }                                                                                     | 
      ObfDereferenceObjectWithTag(v32, 0x68506D4Du);                                        | 
    }                                                                                       | 
    *v3 = (unsigned __int64)v42;                                                            | 
    return (unsigned int)v15;                                                               | 
  }                                                                                         | 
  return result;                                                                            | 
}                                                                                           | 
248 row(s)
