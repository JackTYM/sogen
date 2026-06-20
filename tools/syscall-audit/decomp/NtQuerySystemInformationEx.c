__int64 __fastcall NtQuerySystemInformationEx(int a1, __int64 a2, int a3)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  if ( !a2 || !a3 )
  {
    return 3221225485LL;
  }
  if ( a1 > 180 )
  {
    if ( a1 > 230 )
    {
      v26 = a1 - 231;
      if ( !v26 )
      {
        goto LABEL_42;
      }
      v27 = v26 - 1;
      if ( !v27 )
      {
        goto LABEL_42;
      }
      v28 = v27 - 6;
      if ( v28 )
      {
        v29 = v28 - 1;
        if ( !v29 )
        {
          goto LABEL_42;
        }
        v30 = v29 - 1;
        if ( !v30 )
        {
          goto LABEL_42;
        }
        v31 = v30 - 14;
        if ( v31 )
        {
          if ( v31 == 2 )
          {
            goto LABEL_42;
          }
          return 3221225475LL;
        }
      }
    }
    else if ( a1 != 230 )
    {
      v20 = a1 - 181;
      if ( v20 )
      {
        v21 = v20 - 13;
        if ( !v21 )
        {
          goto LABEL_42;
        }
        v22 = v21 - 15;
        if ( v22 )
        {
          v23 = v22 - 1;
          if ( !v23 )
          {
            goto LABEL_42;
          }
          v24 = v23 - 1;
          if ( v24 )
          {
            v25 = v24 - 11;
            if ( !v25 )
            {
              goto LABEL_42;
            }
            v19 = v25 == 1;
LABEL_23:
            if ( v19 )
            {
              goto LABEL_24;
            }
            return 3221225475LL;
          }
        }
      }
    }
LABEL_24:
    v12 = 8;
    goto LABEL_43;
  }
  if ( a1 == 180 )
  {
    goto LABEL_42;
  }
  if ( a1 > 107 )
  {
    v13 = a1 - 108;
    if ( !v13 )
    {
      goto LABEL_15;
    }
    v14 = v13 - 13;
    if ( !v14 )
    {
      goto LABEL_15;
    }
    v15 = v14 - 20;
    if ( !v15 )
    {
      goto LABEL_15;
    }
    v16 = v15 - 19;
    if ( !v16 )
    {
      goto LABEL_15;
    }
    v17 = v16 - 5;
    if ( v17 )
    {
      v18 = v17 - 10;
      if ( v18 )
      {
        v19 = v18 == 3;
        goto LABEL_23;
      }
    }
    goto LABEL_24;
  }
  if ( a1 == 107 )
  {
    goto LABEL_42;
  }
  v5 = a1 - 8;
  if ( !v5 )
  {
    goto LABEL_15;
  }
  v6 = v5 - 15;
  if ( !v6 )
  {
    goto LABEL_15;
  }
  v7 = v6 - 19;
  if ( !v7 )
  {
    goto LABEL_15;
  }
  v8 = v7 - 19;
  if ( !v8 )
  {
    goto LABEL_15;
  }
  v9 = v8 - 11;
  if ( !v9 )
  {
LABEL_42:
    v12 = 4;
    goto LABEL_43;
  }
  v10 = v9 - 1;
  if ( v10 )
  {
    v11 = v10 - 10;
    if ( v11 )
    {
      if ( v11 != 17 )
      {
        return 3221225475LL;
      }
    }
  }
LABEL_15:
  v12 = 2;
LABEL_43:
  if ( KeGetCurrentThread()->PreviousMode )
  {
    if ( ((v12 - 1) & (unsigned int)a2) != 0 )
    {
      ExRaiseDatatypeMisalignment();
    }
  }
  return ExpQuerySystemInformation((unsigned int)a1, a2);
}
