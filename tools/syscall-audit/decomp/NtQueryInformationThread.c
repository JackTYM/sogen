__int64 __fastcall NtQueryInformationThread(                                                                            | 
        ULONG_PTR BugCheckParameter1,                                                                                   | 
        int a2,                                                                                                         | 
        volatile void **a3,                                                                                             | 
        unsigned int a4,                                                                                                | 
        unsigned int *a5)                                                                                               | 
{                                                                                                                       | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                         | 
                                                                                                                        | 
  v91[5] = BugCheckParameter1;                                                                                          | 
  v84 = a5;                                                                                                             | 
  DWORD1(v88[0]) = 0;                                                                                                   | 
  Flink = 0LL;                                                                                                          | 
  v91[0] = 0LL;                                                                                                         | 
  v93 = 0LL;                                                                                                            | 
  v94 = 0;                                                                                                              | 
  v85 = 0LL;                                                                                                            | 
  *(_OWORD *)Address = 0LL;                                                                                             | 
  memset(v77, 0, 12);                                                                                                   | 
  v95 = 0LL;                                                                                                            | 
  v96 = 0LL;                                                                                                            | 
  Thread = 0LL;                                                                                                         | 
  memset_0(v98, 0, 0x100uLL);                                                                                           | 
  CycleTimeStamp = 0LL;                                                                                                 | 
  memset(v92, 0, sizeof(v92));                                                                                          | 
  CurrentThread = KeGetCurrentThread();                                                                                 | 
  v81 = CurrentThread;                                                                                                  | 
  PreviousMode = CurrentThread->PreviousMode;                                                                           | 
  v71 = PreviousMode;                                                                                                   | 
  if ( PreviousMode )                                                                                                   | 
  {                                                                                                                     | 
    if ( a2 == 38 )                                                                                                     | 
    {                                                                                                                   | 
      v12 = 8;                                                                                                          | 
    }                                                                                                                   | 
    else if ( a2 == 49 )                                                                                                | 
    {                                                                                                                   | 
      v12 = 4;                                                                                                          | 
    }                                                                                                                   | 
    else                                                                                                                | 
    {                                                                                                                   | 
      v12 = 1;                                                                                                          | 
      if ( a4 >= 4 )                                                                                                    | 
      {                                                                                                                 | 
        v12 = 4;                                                                                                        | 
      }                                                                                                                 | 
    }                                                                                                                   | 
    v78 = v12;                                                                                                          | 
    if ( a4 )                                                                                                           | 
    {                                                                                                                   | 
      if ( ((v12 - 1) & (unsigned int)a3) != 0 )                                                                        | 
      {                                                                                                                 | 
        ExRaiseDatatypeMisalignment();                                                                                  | 
      }                                                                                                                 | 
      v13 = 0x7FFFFFFF0000LL;                                                                                           | 
    }                                                                                                                   | 
    else                                                                                                                | 
    {                                                                                                                   | 
      v13 = 0x7FFFFFFF0000LL;                                                                                           | 
    }                                                                                                                   | 
    if ( a5 )                                                                                                           | 
    {                                                                                                                   | 
      if ( (unsigned __int64)a5 < 0x7FFFFFFF0000LL )                                                                    | 
      {                                                                                                                 | 
        v13 = (__int64)a5;                                                                                              | 
      }                                                                                                                 | 
      *(_DWORD *)v13 = *(_DWORD *)v13;                                                                                  | 
    }                                                                                                                   | 
    PreviousMode = v71;                                                                                                 | 
  }                                                                                                                     | 
  DWORD1(v88[0]) = 0;                                                                                                   | 
  v88[1] = 0LL;                                                                                                         | 
  v89 = 0uLL;                                                                                                           | 
  if ( a2 != 23 )                                                                                                       | 
  {                                                                                                                     | 
    if ( a2 == 25 )                                                                                                     | 
    {                                                                                                                   | 
      if ( a4 != 4 )                                                                                                    | 
      {                                                                                                                 | 
        return 3221225476LL;                                                                                            | 
      }                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);          | 
      if ( (int)result < 0 )                                                                                            | 
      {                                                                                                                 | 
        return result;                                                                                                  | 
      }                                                                                                                 | 
      BasePriority = Thread->BasePriority;                                                                              | 
      ObfDereferenceObjectWithTag(Thread, 0x79517350u);                                                                 | 
      *(_DWORD *)a3 = BasePriority;                                                                                     | 
      if ( a5 )                                                                                                         | 
      {                                                                                                                 | 
        *a5 = 4;                                                                                                        | 
      }                                                                                                                 | 
    }                                                                                                                   | 
    else                                                                                                                | 
    {                                                                                                                   | 
      if ( a2 )                                                                                                         | 
      {                                                                                                                 | 
        switch ( a2 )                                                                                                   | 
        {                                                                                                               | 
          case 1:                                                                                                       | 
            if ( a4 != 32 )                                                                                             | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);    | 
            if ( (int)result < 0 )                                                                                      | 
            {                                                                                                           | 
              return result;                                                                                            | 
            }                                                                                                           | 
            v22 = (unsigned int)KeMaximumIncrement * (unsigned __int64)Thread->SchedulerApc.SpareLong0;                 | 
            v23 = (unsigned int)KeMaximumIncrement * (unsigned __int64)Thread->UserTime;                                | 
            v24 = *(volatile void **)&Thread[1].Header.Lock;                                                            | 
            if ( LOBYTE(Thread->Header.SignalState) )                                                                   | 
            {                                                                                                           | 
              Flink = Thread[1].Header.WaitListHead.Flink;                                                              | 
            }                                                                                                           | 
            ObfDereferenceObjectWithTag(Thread, 0x79517350u);                                                           | 
            *a3 = v24;                                                                                                  | 
            a3[1] = Flink;                                                                                              | 
            a3[2] = (volatile void *)v22;                                                                               | 
            a3[3] = (volatile void *)v23;                                                                               | 
            if ( a5 )                                                                                                   | 
            {                                                                                                           | 
              *a5 = 32;                                                                                                 | 
            }                                                                                                           | 
            return 0LL;                                                                                                 | 
          case 6:                                                                                                       | 
            result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);    | 
            if ( (int)result < 0 )                                                                                      | 
            {                                                                                                           | 
              return result;                                                                                            | 
            }                                                                                                           | 
            LODWORD(Flink) = xKdEnumerateDebuggingDevices();                                                            | 
            goto LABEL_192;                                                                                             | 
          case 9:                                                                                                       | 
            if ( a4 != 8 )                                                                                              | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            LODWORD(Flink) = ObpReferenceObjectByHandleWithTag(                                                         | 
                               BugCheckParameter1,                                                                      | 
                               0x79517350u,                                                                             | 
                               (__int64)&Thread,                                                                        | 
                               0LL,                                                                                     | 
                               0LL);                                                                                    | 
            if ( (int)Flink >= 0 )                                                                                      | 
            {                                                                                                           | 
              Blink = Thread[1].ApcState.ApcListHead[0].Blink;                                                          | 
              ObfDereferenceObjectWithTag(Thread, 0x79517350u);                                                         | 
              *a3 = Blink;                                                                                              | 
              if ( a5 )                                                                                                 | 
              {                                                                                                         | 
                *a5 = 8;                                                                                                | 
              }                                                                                                         | 
            }                                                                                                           | 
            return (unsigned int)Flink;                                                                                 | 
          case 11:                                                                                                      | 
            if ( a4 != 8 )                                                                                              | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            ContextThread = ObpReferenceObjectByHandleWithTag(                                                          | 
                              BugCheckParameter1,                                                                       | 
                              0x79517350u,                                                                              | 
                              (__int64)&Thread,                                                                         | 
                              0LL,                                                                                      | 
                              0LL);                                                                                     | 
            if ( ContextThread >= 0 )                                                                                   | 
            {                                                                                                           | 
              ObfDereferenceObjectWithTag(Thread, 0x79517350u);                                                         | 
              *a3 = 0LL;                                                                                                | 
              if ( a5 )                                                                                                 | 
              {                                                                                                         | 
                *a5 = 8;                                                                                                | 
              }                                                                                                         | 
            }                                                                                                           | 
            return (unsigned int)ContextThread;                                                                         | 
          case 12:                                                                                                      | 
            if ( a4 != 4 )                                                                                              | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            LOBYTE(Flink) = LODWORD(CurrentThread->Process[1].CpuPartitionList.Blink) == 1;                             | 
            *(_DWORD *)a3 = (_DWORD)Flink;                                                                              | 
            if ( a5 )                                                                                                   | 
            {                                                                                                           | 
              *a5 = 4;                                                                                                  | 
            }                                                                                                           | 
            return 0LL;                                                                                                 | 
          case 14:                                                                                                      | 
            if ( a4 != 4 )                                                                                              | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            v48 = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);       | 
            ContextThread = v48;                                                                                        | 
            if ( v48 >= 0 )                                                                                             | 
            {                                                                                                           | 
              v49 = (*((_DWORD *)&Thread->0 + 1) >> 3) & 1;                                                             | 
              ObfDereferenceObjectWithTag(Thread, 0x79517350u);                                                         | 
              *(_DWORD *)a3 = v49;                                                                                      | 
              if ( a5 )                                                                                                 | 
              {                                                                                                         | 
                *a5 = 4;                                                                                                | 
              }                                                                                                         | 
            }                                                                                                           | 
            return (unsigned int)ContextThread;                                                                         | 
          case 16:                                                                                                      | 
            if ( a4 != 4 )                                                                                              | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);    | 
            if ( (int)result < 0 )                                                                                      | 
            {                                                                                                           | 
              return result;                                                                                            | 
            }                                                                                                           | 
            LOBYTE(Flink) = *(_QWORD *)&Thread[1].SystemCallNumber != (_QWORD)Thread + 1344;                            | 
            ObfDereferenceObjectWithTag(Thread, 0x79517350u);                                                           | 
            *(_DWORD *)a3 = (_DWORD)Flink;                                                                              | 
            if ( a5 )                                                                                                   | 
            {                                                                                                           | 
              *a5 = 4;                                                                                                  | 
            }                                                                                                           | 
            return 0LL;                                                                                                 | 
          case 17:                                                                                                      | 
            if ( a4 != 1 )                                                                                              | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);    | 
            LODWORD(Flink) = result;                                                                                    | 
            if ( (int)result < 0 )                                                                                      | 
            {                                                                                                           | 
              return result;                                                                                            | 
            }                                                                                                           | 
            v14 = Thread;                                                                                               | 
            *(_BYTE *)a3 = (*(_DWORD *)(&Thread[1].SwapListEntry + 1) & 4) != 0;                                        | 
            if ( a5 )                                                                                                   | 
            {                                                                                                           | 
              *a5 = 1;                                                                                                  | 
            }                                                                                                           | 
            goto LABEL_26;                                                                                              | 
          case 18:                                                                                                      | 
            if ( a4 != 4 )                                                                                              | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);    | 
            if ( (int)result < 0 )                                                                                      | 
            {                                                                                                           | 
              return result;                                                                                            | 
            }                                                                                                           | 
            v55 = (*((_DWORD *)&Thread[1].SwapListEntry + 2) >> 5) & 1;                                                 | 
            ObfDereferenceObjectWithTag(Thread, 0x79517350u);                                                           | 
            *(_DWORD *)a3 = v55;                                                                                        | 
            if ( a5 )                                                                                                   | 
            {                                                                                                           | 
              *a5 = 4;                                                                                                  | 
            }                                                                                                           | 
            return 0LL;                                                                                                 | 
          case 20:                                                                                                      | 
            if ( a4 != 4 )                                                                                              | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);    | 
            if ( (int)result < 0 )                                                                                      | 
            {                                                                                                           | 
              return result;                                                                                            | 
            }                                                                                                           | 
            v47 = *(_DWORD *)(&Thread[1].SwapListEntry + 1) & 1;                                                        | 
            ObfDereferenceObjectWithTag(Thread, 0x79517350u);                                                           | 
            *(_DWORD *)a3 = v47;                                                                                        | 
            if ( a5 )                                                                                                   | 
            {                                                                                                           | 
              *a5 = 4;                                                                                                  | 
            }                                                                                                           | 
            return 0LL;                                                                                                 | 
          case 21:                                                                                                      | 
            result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);    | 
            if ( (int)result < 0 )                                                                                      | 
            {                                                                                                           | 
              return result;                                                                                            | 
            }                                                                                                           | 
            LODWORD(Flink) = PspQueryLastCallThread(Thread, a3, a4, a5);                                                | 
            v14 = Thread;                                                                                               | 
            goto LABEL_26;                                                                                              | 
          case 22:                                                                                                      | 
          case 54:                                                                                                      | 
            if ( a4 != 4 )                                                                                              | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);    | 
            LODWORD(Flink) = result;                                                                                    | 
            v73 = result;                                                                                               | 
            if ( (int)result < 0 )                                                                                      | 
            {                                                                                                           | 
              return result;                                                                                            | 
            }                                                                                                           | 
            v36 = Thread;                                                                                               | 
            if ( a2 == 22 )                                                                                             | 
            {                                                                                                           | 
              IoPriorityThread = (*((_DWORD *)&Thread[1].SwapListEntry + 2) >> 9) & 7;                                  | 
            }                                                                                                           | 
            else                                                                                                        | 
            {                                                                                                           | 
              IoPriorityThread = PsGetIoPriorityThread(Thread);                                                         | 
            }                                                                                                           | 
            *(_DWORD *)a3 = IoPriorityThread;                                                                           | 
            if ( a5 )                                                                                                   | 
            {                                                                                                           | 
              *a5 = 4;                                                                                                  | 
            }                                                                                                           | 
            v14 = v36;                                                                                                  | 
            goto LABEL_26;                                                                                              | 
          case 24:                                                                                                      | 
          case 55:                                                                                                      | 
            if ( a4 != 4 )                                                                                              | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);    | 
            LODWORD(Flink) = result;                                                                                    | 
            if ( (int)result < 0 )                                                                                      | 
            {                                                                                                           | 
              return result;                                                                                            | 
            }                                                                                                           | 
            v26 = Thread;                                                                                               | 
            if ( a2 == 24 )                                                                                             | 
            {                                                                                                           | 
              PagePriorityThread = (*((_DWORD *)&Thread[1].SwapListEntry + 2) >> 12) & 7;                               | 
            }                                                                                                           | 
            else                                                                                                        | 
            {                                                                                                           | 
              PagePriorityThread = PsGetPagePriorityThread(Thread);                                                     | 
            }                                                                                                           | 
            *(_DWORD *)a3 = PagePriorityThread;                                                                         | 
            if ( a5 )                                                                                                   | 
            {                                                                                                           | 
              *a5 = 4;                                                                                                  | 
            }                                                                                                           | 
            goto LABEL_75;                                                                                              | 
          case 26:                                                                                                      | 
          case 58:                                                                                                      | 
            if ( a4 != 16 )                                                                                             | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            if ( PreviousMode )                                                                                         | 
            {                                                                                                           | 
              RtlCopyVolatileMemory(Address, a3, 0x10uLL);                                                              | 
              ProbeForWrite(Address[0], HIDWORD(Address[1]), 1u);                                                       | 
              a3 = Address;                                                                                             | 
            }                                                                                                           | 
            result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);    | 
            v73 = result;                                                                                               | 
            if ( (int)result < 0 )                                                                                      | 
            {                                                                                                           | 
              return result;                                                                                            | 
            }                                                                                                           | 
            if ( (Thread->MiscFlags & 0x400) != 0 )                                                                     | 
            {                                                                                                           | 
              ObfDereferenceObjectWithTag(Thread, 0x79517350u);                                                         | 
              return 3221225480LL;                                                                                      | 
            }                                                                                                           | 
            else                                                                                                        | 
            {                                                                                                           | 
              Process = Thread->Process;                                                                                | 
              v39 = KeGetCurrentThread()->ApcState.Process;                                                             | 
              CycleTimeStamp = (unsigned __int64)v39;                                                                   | 
              if ( a2 == 58 && Process != v39 )                                                                         | 
              {                                                                                                         | 
                goto LABEL_181;                                                                                         | 
              }                                                                                                         | 
              v40 = 6264LL;                                                                                             | 
              if ( Process[1].ReadyTime )                                                                               | 
              {                                                                                                         | 
                v40 = 12344LL;                                                                                          | 
                if ( WORD2(Process[3].PerProcessorCycleTimes) == 0x8664 )                                               | 
                {                                                                                                       | 
                  v40 = 14456LL;                                                                                        | 
                }                                                                                                       | 
              }                                                                                                         | 
              v41 = (unsigned int *)a3 + 3;                                                                             | 
              v81 = (PETHREAD)((char *)a3 + 12);                                                                        | 
              v42 = *((unsigned int *)a3 + 3);                                                                          | 
              if ( (_DWORD)v42 && (v43 = *((unsigned int *)a3 + 2), v43 < v40) && v42 <= v40 - v43 )                    | 
              {                                                                                                         | 
                v44 = (struct _EX_RUNDOWN_REF *)Thread;                                                                 | 
                if ( ExAcquireRundownProtection_0((PEX_RUNDOWN_REF)&Thread[1].WaitStatus) )                             | 
                {                                                                                                       | 
                  v45 = (_BYTE *)(v44[30].Count + *((unsigned int *)a3 + 2));                                           | 
                  if ( Process == (_KPROCESS *)CycleTimeStamp )                                                         | 
                  {                                                                                                     | 
                    v46 = *v41;                                                                                         | 
                    if ( a2 == 58 )                                                                                     | 
                    {                                                                                                   | 
                      if ( v46 == 1 )                                                                                   | 
                      {                                                                                                 | 
                        *(_BYTE *)*a3 = *v45;                                                                           | 
                        v44 = (struct _EX_RUNDOWN_REF *)Thread;                                                         | 
                      }                                                                                                 | 
                      else if ( v46 != 2 || ((unsigned __int8)v45 & 1) != 0 )                                           | 
                      {                                                                                                 | 
                        if ( v46 != 4 || ((unsigned __int8)v45 & 3) != 0 )                                              | 
                        {                                                                                               | 
                          if ( v46 != 8 || ((unsigned __int8)v45 & 7) != 0 )                                            | 
                          {                                                                                             | 
                            v73 = -1073741811;                                                                          | 
                          }                                                                                             | 
                          else                                                                                          | 
                          {                                                                                             | 
                            *(_QWORD *)*a3 = *(_QWORD *)v45;                                                            | 
                            v44 = (struct _EX_RUNDOWN_REF *)Thread;                                                     | 
                          }                                                                                             | 
                        }                                                                                               | 
                        else                                                                                            | 
                        {                                                                                               | 
                          *(_DWORD *)*a3 = *(_DWORD *)v45;                                                              | 
                          v44 = (struct _EX_RUNDOWN_REF *)Thread;                                                       | 
                        }                                                                                               | 
                      }                                                                                                 | 
                      else                                                                                              | 
                      {                                                                                                 | 
                        *(_WORD *)*a3 = *(_WORD *)v45;                                                                  | 
                        v44 = (struct _EX_RUNDOWN_REF *)Thread;                                                         | 
                      }                                                                                                 | 
                    }                                                                                                   | 
                    else                                                                                                | 
                    {                                                                                                   | 
                      memmove((void *)*a3, v45, *v41);                                                                  | 
                    }                                                                                                   | 
                  }                                                                                                     | 
                  else                                                                                                  | 
                  {                                                                                                     | 
                    v73 = MiCopyVirtualMemory(                                                                          | 
                            (_DWORD)Process,                                                                            | 
                            (_DWORD)v45,                                                                                | 
                            CycleTimeStamp,                                                                             | 
                            (unsigned int)*a3,                                                                          | 
                            *v41,                                                                                       | 
                            v71,                                                                                        | 
                            (__int64)v91,                                                                               | 
                            0);                                                                                         | 
                  }                                                                                                     | 
                  ExReleaseRundownProtection_0(v44 + 177);                                                              | 
                  ObfDereferenceObjectWithTag(v44, 0x79517350u);                                                        | 
                  if ( v73 >= 0 )                                                                                       | 
                  {                                                                                                     | 
                    if ( a5 )                                                                                           | 
                    {                                                                                                   | 
                      *a5 = *v41;                                                                                       | 
                    }                                                                                                   | 
                    return 0;                                                                                           | 
                  }                                                                                                     | 
                  return (unsigned int)v73;                                                                             | 
                }                                                                                                       | 
                else                                                                                                    | 
                {                                                                                                       | 
                  ObfDereferenceObjectWithTag(v44, 0x79517350u);                                                        | 
                  return 3221225547LL;                                                                                  | 
                }                                                                                                       | 
              }                                                                                                         | 
              else                                                                                                      | 
              {                                                                                                         | 
LABEL_181:                                                                                                              | 
                ObfDereferenceObjectWithTag(Thread, 0x79517350u);                                                       | 
                return 3221225485LL;                                                                                    | 
              }                                                                                                         | 
            }                                                                                                           | 
          case 29:                                                                                                      | 
            result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);    | 
            if ( (int)result < 0 )                                                                                      | 
            {                                                                                                           | 
              return result;                                                                                            | 
            }                                                                                                           | 
            ContextThread = PspWow64GetContextThread(Thread, a3, a4, v71);                                              | 
            ObfDereferenceObjectWithTag(Thread, 0x79517350u);                                                           | 
            if ( ContextThread >= 0 && a5 )                                                                             | 
            {                                                                                                           | 
              *a5 = 716;                                                                                                | 
            }                                                                                                           | 
            return (unsigned int)ContextThread;                                                                         | 
          case 30:                                                                                                      | 
            if ( a4 != 16 )                                                                                             | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);    | 
            ContextThread = result;                                                                                     | 
            if ( (int)result < 0 )                                                                                      | 
            {                                                                                                           | 
              return result;                                                                                            | 
            }                                                                                                           | 
            v97 = 2097153LL;                                                                                            | 
            memset_0(v98, 0, 0x100uLL);                                                                                 | 
            v51 = Thread;                                                                                               | 
            KeQueryUserAffinityThread(Thread, &v97);                                                                    | 
            UserAffinityPrimaryGroup = v51->UserAffinityPrimaryGroup;                                                   | 
            WORD4(v85) = UserAffinityPrimaryGroup;                                                                      | 
            if ( (unsigned __int16)UserAffinityPrimaryGroup < (unsigned __int16)v97 )                                   | 
            {                                                                                                           | 
              Flink = (struct _LIST_ENTRY *)v98[UserAffinityPrimaryGroup];                                              | 
            }                                                                                                           | 
            *(_QWORD *)&v85 = Flink;                                                                                    | 
            *(_OWORD *)a3 = v85;                                                                                        | 
            if ( a5 )                                                                                                   | 
            {                                                                                                           | 
              *a5 = 16;                                                                                                 | 
            }                                                                                                           | 
            ObfDereferenceObjectWithTag(v51, 0x79517350u);                                                              | 
            return (unsigned int)ContextThread;                                                                         | 
          case 32:                                                                                                      | 
            if ( a4 != 1 )                                                                                              | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);    | 
            ContextThread = result;                                                                                     | 
            if ( (int)result < 0 )                                                                                      | 
            {                                                                                                           | 
              return result;                                                                                            | 
            }                                                                                                           | 
            v57 = Thread;                                                                                               | 
            *(_BYTE *)a3 = Thread->WaitBlock[0].SparePtr != 0LL;                                                        | 
            ObfDereferenceObjectWithTag(v57, 0x79517350u);                                                              | 
            if ( a5 )                                                                                                   | 
            {                                                                                                           | 
              *a5 = a4;                                                                                                 | 
            }                                                                                                           | 
            return (unsigned int)ContextThread;                                                                         | 
          case 33:                                                                                                      | 
            if ( a4 != 4 )                                                                                              | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);    | 
            if ( (int)result < 0 )                                                                                      | 
            {                                                                                                           | 
              return result;                                                                                            | 
            }                                                                                                           | 
            v75 = 0x400000;                                                                                             | 
            KeSetIdealProcessorThreadByNumber(Thread, &v75, &v75);                                                      | 
            ObfDereferenceObjectWithTag(Thread, 0x79517350u);                                                           | 
            *(_DWORD *)a3 = v75;                                                                                        | 
            if ( a5 )                                                                                                   | 
            {                                                                                                           | 
              *a5 = 4;                                                                                                  | 
            }                                                                                                           | 
            return 0LL;                                                                                                 | 
          case 34:                                                                                                      | 
            if ( !a4 )                                                                                                  | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);    | 
            v73 = result;                                                                                               | 
            if ( (int)result < 0 )                                                                                      | 
            {                                                                                                           | 
              return result;                                                                                            | 
            }                                                                                                           | 
            *(_BYTE *)a3 = Thread->SchedulingGroup != 0LL;                                                              | 
            if ( a5 )                                                                                                   | 
            {                                                                                                           | 
              *a5 = 1;                                                                                                  | 
            }                                                                                                           | 
            LODWORD(Flink) = v73;                                                                                       | 
            goto LABEL_192;                                                                                             | 
          case 35:                                                                                                      | 
            if ( a4 != 4 )                                                                                              | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);    | 
            LODWORD(Flink) = result;                                                                                    | 
            if ( (int)result < 0 )                                                                                      | 
            {                                                                                                           | 
              return result;                                                                                            | 
            }                                                                                                           | 
            v26 = Thread;                                                                                               | 
            *(_DWORD *)a3 = Thread->SuspendCount + ((*((_DWORD *)&Thread->0 + 1) >> 14) & 1);                           | 
            if ( a5 )                                                                                                   | 
            {                                                                                                           | 
              *a5 = 4;                                                                                                  | 
            }                                                                                                           | 
            goto LABEL_75;                                                                                              | 
          case 36:                                                                                                      | 
            if ( a4 != 4 )                                                                                              | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            ContextThread = ObpReferenceObjectByHandleWithTag(                                                          | 
                              BugCheckParameter1,                                                                       | 
                              0x79517350u,                                                                              | 
                              (__int64)&Thread,                                                                         | 
                              0LL,                                                                                      | 
                              0LL);                                                                                     | 
            if ( ContextThread < 0 )                                                                                    | 
            {                                                                                                           | 
              return (unsigned int)ContextThread;                                                                       | 
            }                                                                                                           | 
            v58 = Thread;                                                                                               | 
            *(_DWORD *)a3 = MmGetDefaultPagePriority(Thread, 1LL);                                                      | 
            if ( a5 )                                                                                                   | 
            {                                                                                                           | 
              *a5 = 4;                                                                                                  | 
            }                                                                                                           | 
LABEL_306:                                                                                                              | 
            ObfDereferenceObjectWithTag(v58, 0x79517350u);                                                              | 
            return (unsigned int)ContextThread;                                                                         | 
          case 37:                                                                                                      | 
            if ( a4 != 16 )                                                                                             | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            if ( BugCheckParameter1 != -2LL )                                                                           | 
            {                                                                                                           | 
              return 3221225485LL;                                                                                      | 
            }                                                                                                           | 
            result = PsGetEffectiveContainerId(4LL, CurrentThread, &v93);                                               | 
            if ( (int)result >= 0 )                                                                                     | 
            {                                                                                                           | 
              *(_OWORD *)a3 = v93;                                                                                      | 
              if ( a5 )                                                                                                 | 
              {                                                                                                         | 
                *a5 = 16;                                                                                               | 
              }                                                                                                         | 
            }                                                                                                           | 
            return result;                                                                                              | 
          case 38:                                                                                                      | 
            v28 = 0;                                                                                                    | 
            v72 = 0;                                                                                                    | 
            v74 = 0;                                                                                                    | 
            v29 = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);       | 
            v73 = v29;                                                                                                  | 
            if ( v29 >= 0 )                                                                                             | 
            {                                                                                                           | 
              v72 = 1;                                                                                                  | 
              v30 = Thread;                                                                                             | 
              PspLockThreadSecurityShared(Thread, CurrentThread);                                                       | 
              v74 = 1;                                                                                                  | 
              v31 = *(_QWORD *)&v30[1].WaitBlockFill11[160];                                                            | 
              if ( v31 )                                                                                                | 
              {                                                                                                         | 
                v32 = *(const WCHAR **)(v31 + 8);                                                                       | 
                LOWORD(Flink) = *(_WORD *)v31;                                                                          | 
              }                                                                                                         | 
              else                                                                                                      | 
              {                                                                                                         | 
                v32 = &word_140AEC790;                                                                                  | 
              }                                                                                                         | 
              v76 = (__int16)Flink;                                                                                     | 
              v91[2] = v32;                                                                                             | 
              v33 = (unsigned __int16)Flink + 16;                                                                       | 
              v90 = v33;                                                                                                | 
              if ( v33 > a4 )                                                                                           | 
              {                                                                                                         | 
                v73 = -1073741789;                                                                                      | 
              }                                                                                                         | 
              else                                                                                                      | 
              {                                                                                                         | 
                memmove(a3 + 2, v32, (unsigned __int16)Flink);                                                          | 
                *(_WORD *)a3 = (_WORD)Flink;                                                                            | 
                *((_WORD *)a3 + 1) = (_WORD)Flink;                                                                      | 
                a3[1] = a3 + 2;                                                                                         | 
              }                                                                                                         | 
              if ( a5 )                                                                                                 | 
              {                                                                                                         | 
                *a5 = v33;                                                                                              | 
              }                                                                                                         | 
              v28 = v72;                                                                                                | 
              v29 = v73;                                                                                                | 
            }                                                                                                           | 
            v34 = Thread;                                                                                               | 
            if ( v74 )                                                                                                  | 
            {                                                                                                           | 
              PspUnlockThreadSecurityShared(Thread, CurrentThread);                                                     | 
            }                                                                                                           | 
            if ( v28 )                                                                                                  | 
            {                                                                                                           | 
              ObfDereferenceObjectWithTag(v34, 0x79517350u);                                                            | 
            }                                                                                                           | 
            return (unsigned int)v29;                                                                                   | 
          case 39:                                                                                                      | 
            if ( (a4 & 7) != 0 || a4 > 0x100 )                                                                          | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            v56 = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);       | 
            v73 = v56;                                                                                                  | 
            if ( v56 >= 0 )                                                                                             | 
            {                                                                                                           | 
              v59 = 8 * KeQueryCpuSetsThread(Thread, Src);                                                              | 
              ObfDereferenceObjectWithTag(Thread, 0x79517350u);                                                         | 
              v83 = v59;                                                                                                | 
              if ( a5 )                                                                                                 | 
              {                                                                                                         | 
                *a5 = v59;                                                                                              | 
              }                                                                                                         | 
              if ( v59 >= a4 )                                                                                          | 
              {                                                                                                         | 
                v59 = a4;                                                                                               | 
              }                                                                                                         | 
              v83 = v59;                                                                                                | 
              memmove(a3, Src, v59);                                                                                    | 
            }                                                                                                           | 
            return (unsigned int)v56;                                                                                   | 
          case 40:                                                                                                      | 
            if ( a4 != 80 )                                                                                             | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);    | 
            v56 = result;                                                                                               | 
            if ( (int)result < 0 )                                                                                      | 
            {                                                                                                           | 
              return result;                                                                                            | 
            }                                                                                                           | 
            v60 = Thread;                                                                                               | 
            KeQueryValuesThread(Thread, v92);                                                                           | 
            ThreadStartAddress = PsQueryThreadStartAddress(v60, 1LL);                                                   | 
            if ( ThreadStartAddress <= 0x7FFFFFFEFFFFLL )                                                               | 
            {                                                                                                           | 
              Flink = (struct _LIST_ENTRY *)ThreadStartAddress;                                                         | 
            }                                                                                                           | 
            *((_DWORD *)a3 + 17) = BYTE8(v92[0]);                                                                       | 
            *((_DWORD *)a3 + 18) = BYTE9(v92[0]);                                                                       | 
            *((_DWORD *)a3 + 15) = SBYTE11(v92[0]);                                                                     | 
            *((_DWORD *)a3 + 14) = SBYTE10(v92[0]);                                                                     | 
            *((_DWORD *)a3 + 6) = v92[0];                                                                               | 
            v62 = (unsigned int)KeMaximumIncrement;                                                                     | 
            *a3 = (volatile void *)((unsigned int)KeMaximumIncrement * (unsigned __int64)v60->SchedulerApc.SpareLong0); | 
            a3[1] = (volatile void *)(v62 * v60->UserTime);                                                             | 
            a3[2] = *(volatile void **)&v60[1].Header.Lock;                                                             | 
            *((_DWORD *)a3 + 16) = v60->WaitBlock[0].SpareLong;                                                         | 
            *(_OWORD *)(a3 + 5) = *(_OWORD *)&v60[1].CycleTime;                                                         | 
            a3[4] = Flink;                                                                                              | 
            if ( a5 )                                                                                                   | 
            {                                                                                                           | 
              *a5 = 80;                                                                                                 | 
            }                                                                                                           | 
            ObfDereferenceObjectWithTag(v60, 0x79517350u);                                                              | 
            return (unsigned int)v56;                                                                                   | 
          case 41:                                                                                                      | 
            if ( a4 != 16 )                                                                                             | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);    | 
            LODWORD(Flink) = result;                                                                                    | 
            if ( (int)result < 0 )                                                                                      | 
            {                                                                                                           | 
              return result;                                                                                            | 
            }                                                                                                           | 
            v63 = Thread;                                                                                               | 
            KeQueryPrimaryGroupAffinityThread(Thread, &v85);                                                            | 
            *(_OWORD *)a3 = v85;                                                                                        | 
            if ( a5 )                                                                                                   | 
            {                                                                                                           | 
              *a5 = 16;                                                                                                 | 
            }                                                                                                           | 
            v14 = v63;                                                                                                  | 
            goto LABEL_26;                                                                                              | 
          case 42:                                                                                                      | 
            if ( a4 != 4 )                                                                                              | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);    | 
            LODWORD(Flink) = result;                                                                                    | 
            if ( (int)result < 0 )                                                                                      | 
            {                                                                                                           | 
              return result;                                                                                            | 
            }                                                                                                           | 
            v14 = Thread;                                                                                               | 
            *(_DWORD *)a3 = (*((_DWORD *)&Thread[1].SwapListEntry + 2) >> 18) & 1;                                      | 
            if ( a5 )                                                                                                   | 
            {                                                                                                           | 
              *a5 = 4;                                                                                                  | 
            }                                                                                                           | 
            goto LABEL_26;                                                                                              | 
          case 43:                                                                                                      | 
            if ( a4 != 4 )                                                                                              | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);    | 
            v73 = result;                                                                                               | 
            if ( (int)result < 0 )                                                                                      | 
            {                                                                                                           | 
              return result;                                                                                            | 
            }                                                                                                           | 
            *(_DWORD *)a3 = (*((_DWORD *)&Thread[1].SwapListEntry + 2) >> 19) & 1;                                      | 
            if ( a5 )                                                                                                   | 
            {                                                                                                           | 
              *a5 = 4;                                                                                                  | 
            }                                                                                                           | 
            LODWORD(Flink) = v73;                                                                                       | 
LABEL_192:                                                                                                              | 
            v14 = Thread;                                                                                               | 
            goto LABEL_26;                                                                                              | 
          case 44:                                                                                                      | 
            v79 = 0;                                                                                                    | 
            if ( a4 != 16 )                                                                                             | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            if ( BugCheckParameter1 != -2LL )                                                                           | 
            {                                                                                                           | 
              return 3221225485LL;                                                                                      | 
            }                                                                                                           | 
            v86 = 0LL;                                                                                                  | 
            WorkOnBehalfThread = PsGetWorkOnBehalfThread(CurrentThread, &v79);                                          | 
            if ( WorkOnBehalfThread )                                                                                   | 
            {                                                                                                           | 
              PsEncodeThreadWorkOnBehalfTicket(WorkOnBehalfThread, &v86);                                               | 
              if ( v79 )                                                                                                | 
              {                                                                                                         | 
                ObfDereferenceObject(v50);                                                                              | 
              }                                                                                                         | 
            }                                                                                                           | 
            else                                                                                                        | 
            {                                                                                                           | 
              PsEncodeThreadWorkOnBehalfTicket(CurrentThread, &v86);                                                    | 
              DWORD2(v86) |= 1u;                                                                                        | 
            }                                                                                                           | 
            *(_OWORD *)a3 = v86;                                                                                        | 
            if ( a5 )                                                                                                   | 
            {                                                                                                           | 
              *a5 = 16;                                                                                                 | 
            }                                                                                                           | 
            return 0LL;                                                                                                 | 
          case 45:                                                                                                      | 
            if ( a4 != 4 )                                                                                              | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);    | 
            v53 = result;                                                                                               | 
            if ( (int)result >= 0 )                                                                                     | 
            {                                                                                                           | 
              v54 = Thread;                                                                                             | 
              if ( *(_QWORD *)&Thread[1].WaitBlockFill11[112] )                                                         | 
              {                                                                                                         | 
                LODWORD(Flink) = qword_140F05890;                                                                       | 
              }                                                                                                         | 
              *(_DWORD *)a3 = (_DWORD)Flink;                                                                            | 
              if ( a5 )                                                                                                 | 
              {                                                                                                         | 
                *a5 = 4;                                                                                                | 
              }                                                                                                         | 
              ObfDereferenceObjectWithTag(v54, 0x79517350u);                                                            | 
              return v53;                                                                                               | 
            }                                                                                                           | 
            return result;                                                                                              | 
          case 49:                                                                                                      | 
            if ( a4 < 4 )                                                                                               | 
            {                                                                                                           | 
              if ( a5 )                                                                                                 | 
              {                                                                                                         | 
                *a5 = 12;                                                                                               | 
              }                                                                                                         | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            v77[0] = *a3;                                                                                               | 
            v64 = *((_DWORD *)a3 + 2);                                                                                  | 
            if ( LODWORD(v77[0]) != 1 )                                                                                 | 
            {                                                                                                           | 
              return 3221225485LL;                                                                                      | 
            }                                                                                                           | 
            if ( a5 )                                                                                                   | 
            {                                                                                                           | 
              *a5 = 12;                                                                                                 | 
            }                                                                                                           | 
            if ( a4 != 12 )                                                                                             | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);    | 
            if ( (int)result < 0 )                                                                                      | 
            {                                                                                                           | 
              return result;                                                                                            | 
            }                                                                                                           | 
            ThreadPpmPolicy = PspGetThreadPpmPolicy(Thread);                                                            | 
            if ( ThreadPpmPolicy && (v66 = ThreadPpmPolicy - 1) != 0 && (v67 = v66 - 1) != 0 )                          | 
            {                                                                                                           | 
              v68 = v67 - 1;                                                                                            | 
              if ( v68 )                                                                                                | 
              {                                                                                                         | 
                if ( v68 == 5 )                                                                                         | 
                {                                                                                                       | 
                  HIDWORD(v77[0]) = 1;                                                                                  | 
                  v64 = 1;                                                                                              | 
                  LODWORD(v77[1]) = 1;                                                                                  | 
                }                                                                                                       | 
LABEL_290:                                                                                                              | 
                *a3 = (volatile void *)v77[0];                                                                          | 
                *((_DWORD *)a3 + 2) = v64;                                                                              | 
                v73 = 0;                                                                                                | 
LABEL_75:                                                                                                               | 
                v14 = v26;                                                                                              | 
                goto LABEL_26;                                                                                          | 
              }                                                                                                         | 
              *(_QWORD *)((char *)v77 + 4) = 1LL;                                                                       | 
            }                                                                                                           | 
            else                                                                                                        | 
            {                                                                                                           | 
              *(_QWORD *)((char *)v77 + 4) = 0LL;                                                                       | 
            }                                                                                                           | 
            v64 = 0;                                                                                                    | 
            goto LABEL_290;                                                                                             | 
          case 57:                                                                                                      | 
            if ( a4 != 8 )                                                                                              | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            ContextThread = ObpReferenceObjectByHandleWithTag(                                                          | 
                              BugCheckParameter1,                                                                       | 
                              0x79517350u,                                                                              | 
                              (__int64)&Thread,                                                                         | 
                              0LL,                                                                                      | 
                              0LL);                                                                                     | 
            v69 = Thread;                                                                                               | 
            if ( ContextThread >= 0 )                                                                                   | 
            {                                                                                                           | 
              v80 = 0LL;                                                                                                | 
              ContextThread = PspSchedulerSharedDataRegionSlotRetrieve(Thread, &v80);                                   | 
              if ( ContextThread >= 0 )                                                                                 | 
              {                                                                                                         | 
                *a3 = v80;                                                                                              | 
                if ( a5 )                                                                                               | 
                {                                                                                                       | 
                  *a5 = 8;                                                                                              | 
                }                                                                                                       | 
              }                                                                                                         | 
            }                                                                                                           | 
            if ( !v69 )                                                                                                 | 
            {                                                                                                           | 
              return (unsigned int)ContextThread;                                                                       | 
            }                                                                                                           | 
            ObfDereferenceObjectWithTag(v69, 0x79517350u);                                                              | 
            return (unsigned int)ContextThread;                                                                         | 
          case 59:                                                                                                      | 
            if ( a4 != 8 )                                                                                              | 
            {                                                                                                           | 
              return 3221225476LL;                                                                                      | 
            }                                                                                                           | 
            ContextThread = ObpReferenceObjectByHandleWithTag(                                                          | 
                              BugCheckParameter1,                                                                       | 
                              0x79517350u,                                                                              | 
                              (__int64)&Thread,                                                                         | 
                              0LL,                                                                                      | 
                              0LL);                                                                                     | 
            if ( ContextThread < 0 )                                                                                    | 
            {                                                                                                           | 
              v58 = Thread;                                                                                             | 
            }                                                                                                           | 
            else                                                                                                        | 
            {                                                                                                           | 
              v75 = 0;                                                                                                  | 
              LODWORD(v80) = 0;                                                                                         | 
              v58 = Thread;                                                                                             | 
              ContextThread = PspQueryThreadIndexInformation(Thread, &v75, &v80);                                       | 
              if ( ContextThread >= 0 )                                                                                 | 
              {                                                                                                         | 
                *(_DWORD *)a3 = v75;                                                                                    | 
                *((_DWORD *)a3 + 1) = (_DWORD)v80;                                                                      | 
                if ( a5 )                                                                                               | 
                {                                                                                                       | 
                  *a5 = 8;                                                                                              | 
                }                                                                                                       | 
              }                                                                                                         | 
            }                                                                                                           | 
            if ( v58 )                                                                                                  | 
            {                                                                                                           | 
              goto LABEL_306;                                                                                           | 
            }                                                                                                           | 
            return (unsigned int)ContextThread;                                                                         | 
          default:                                                                                                      | 
            return 3221225475LL;                                                                                        | 
        }                                                                                                               | 
      }                                                                                                                 | 
      if ( a4 != 48 )                                                                                                   | 
      {                                                                                                                 | 
        return 3221225476LL;                                                                                            | 
      }                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);          | 
      if ( (int)result < 0 )                                                                                            | 
      {                                                                                                                 | 
        return result;                                                                                                  | 
      }                                                                                                                 | 
      v16 = Thread;                                                                                                     | 
      if ( LOBYTE(Thread->Header.SignalState) )                                                                         | 
      {                                                                                                                 | 
        LODWORD(v88[0]) = PsGetThreadExitStatus(Thread);                                                                | 
      }                                                                                                                 | 
      else                                                                                                              | 
      {                                                                                                                 | 
        LODWORD(v88[0]) = 259;                                                                                          | 
      }                                                                                                                 | 
      *((_QWORD *)&v88[0] + 1) = v16->Teb;                                                                              | 
      if ( (IoThreadToProcess(v16)->SecureState.SecureHandle & 1) != 0 )                                                | 
      {                                                                                                                 | 
        VslGetSecureTebAddress(v16, (char *)v88 + 8);                                                                   | 
      }                                                                                                                 | 
      v17 = *(_OWORD *)&v16[1].CycleTime;                                                                               | 
      v18 = v16->UserAffinityPrimaryGroup;                                                                              | 
      UserAffinity = v16->UserAffinity;                                                                                 | 
      if ( (unsigned __int16)v18 < UserAffinity->Count )                                                                | 
      {                                                                                                                 | 
        Flink = (struct _LIST_ENTRY *)UserAffinity->Bitmap[v18];                                                        | 
      }                                                                                                                 | 
      *(_QWORD *)&v89 = Flink;                                                                                          | 
      DWORD2(v89) = v16->Priority;                                                                                      | 
      HIDWORD(v89) = KeQueryBasePriorityThread(v16);                                                                    | 
      ObfDereferenceObjectWithTag(v16, 0x79517350u);                                                                    | 
      *(_OWORD *)a3 = v88[0];                                                                                           | 
      *((_OWORD *)a3 + 1) = v17;                                                                                        | 
      *((_OWORD *)a3 + 2) = v89;                                                                                        | 
      if ( a5 )                                                                                                         | 
      {                                                                                                                 | 
        *a5 = 48;                                                                                                       | 
      }                                                                                                                 | 
    }                                                                                                                   | 
    return 0LL;                                                                                                         | 
  }                                                                                                                     | 
  if ( a4 != 16 )                                                                                                       | 
  {                                                                                                                     | 
    return 3221225476LL;                                                                                                | 
  }                                                                                                                     | 
  if ( BugCheckParameter1 == -2LL )                                                                                     | 
  {                                                                                                                     | 
    Thread = CurrentThread;                                                                                             | 
LABEL_22:                                                                                                               | 
    *a3 = (volatile void *)KeQueryTotalCycleTimeThread(CurrentThread, &CycleTimeStamp);                                 | 
    a3[1] = (volatile void *)CycleTimeStamp;                                                                            | 
    if ( a5 )                                                                                                           | 
    {                                                                                                                   | 
      *a5 = 16;                                                                                                         | 
    }                                                                                                                   | 
    if ( BugCheckParameter1 == -2LL )                                                                                   | 
    {                                                                                                                   | 
      return (unsigned int)Flink;                                                                                       | 
    }                                                                                                                   | 
    v14 = CurrentThread;                                                                                                | 
LABEL_26:                                                                                                               | 
    ObfDereferenceObjectWithTag(v14, 0x79517350u);                                                                      | 
    return (unsigned int)Flink;                                                                                         | 
  }                                                                                                                     | 
  result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Thread, 0LL, 0LL);              | 
  LODWORD(Flink) = result;                                                                                              | 
  if ( (int)result >= 0 )                                                                                               | 
  {                                                                                                                     | 
    CurrentThread = Thread;                                                                                             | 
    goto LABEL_22;                                                                                                      | 
  }                                                                                                                     | 
  return result;                                                                                                        | 
}                                                                                                                       | 
