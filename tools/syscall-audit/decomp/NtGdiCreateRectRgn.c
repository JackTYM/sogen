__int64 __fastcall NtGdiCreateRectRgn(LONG a1, LONG a2, LONG a3, LONG a4)                     | 
{                                                                                             | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                               | 
                                                                                              | 
  v18.left = a1;                                                                              | 
  v18.top = a2;                                                                               | 
  v18.right = a3;                                                                             | 
  v18.bottom = a4;                                                                            | 
  if ( (a1 & 0xF8000000) != 0 && (a1 & 0xF8000000) != -134217728                              | 
    || (v11 = a4 & 0xF8000000) != 0 && v11 != -134217728                                      | 
    || (v12 = a3 & 0xF8000000) != 0 && v12 != -134217728                                      | 
    || (a2 & 0xF8000000) != 0 && (a2 & 0xF8000000) != 0xF8000000 )                            | 
  {                                                                                           | 
    EngSetLastError(0x57u);                                                                   | 
    return 0LL;                                                                               | 
  }                                                                                           | 
  else                                                                                        | 
  {                                                                                           | 
    v9 = 0LL;                                                                                 | 
    ObjectAttr = (void *)HmgAllocateObjectAttr();                                             | 
    if ( ObjectAttr && (RGNMEMOBJ::RGNMEMOBJ((RGNMEMOBJ *)&v15), (v14 = v15) != 0) )          | 
    {                                                                                         | 
      left = v18.left;                                                                        | 
      if ( v18.left > v18.right )                                                             | 
      {                                                                                       | 
        v18.left = v18.right;                                                                 | 
        v18.right = left;                                                                     | 
      }                                                                                       | 
      top = v18.top;                                                                          | 
      if ( v18.top > v18.bottom )                                                             | 
      {                                                                                       | 
        v18.top = v18.bottom;                                                                 | 
        v18.bottom = top;                                                                     | 
      }                                                                                       | 
      RGNOBJ::vSet((RGNOBJ *)&v15, &v18);                                                     | 
      HmgInsertObjectHelper::HmgInsertObjectHelper((HmgInsertObjectHelper *)v16);             | 
      inserted = 0LL;                                                                         | 
      if ( v17 || (LOBYTE(v7) = 4, (inserted = HmgInsertObjectInternal(v14, 1LL, v7)) == 0) ) | 
      {                                                                                       | 
        v9 = inserted;                                                                        | 
        RGNOBJ::vDeleteRGNOBJ((RGNOBJ *)&v15);                                                | 
        HmgFreeObjectAttr(ObjectAttr);                                                        | 
      }                                                                                       | 
      else                                                                                    | 
      {                                                                                       | 
        v17 = v14;                                                                            | 
        v9 = inserted;                                                                        | 
        v10 = GreEncodeUserModePointer(ObjectAttr);                                           | 
        *(_QWORD *)(HmgPentryFromPobj(v14) + 16) = v10;                                       | 
      }                                                                                       | 
      HmgInsertObjectHelper::~HmgInsertObjectHelper((HmgInsertObjectHelper *)v16);            | 
    }                                                                                         | 
    else                                                                                      | 
    {                                                                                         | 
      EngSetLastError(8u);                                                                    | 
    }                                                                                         | 
    return v9;                                                                                | 
  }                                                                                           | 
}                                                                                             | 
59 row(s)
