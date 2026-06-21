# KCT Audit — Independent Review Protocol

Per-wave blind review files live here as `kct-wave-NN.tsv`.

## Protocol

A fresh subagent is given only:
- The committed `decomp/<fn>.c` artifact for each slot in the wave
- The `fn_*_message` struct(s) from `src/windows-emulator/syscalls/user.cpp`

It is NOT given the primary evidence text.

The reviewer independently:
1. Re-extracts all `[a1+N]` read offsets and the icall offset from the artifact
2. Confirms sogen's `static_assert(offsetof/sizeof)` cover every extracted offset
3. Verifies the `k_fn_*_callback_id` constant equals the ledger index
4. Pulls a **different** `[xref:TOKEN]` token than what appears in the primary evidence
5. Renders AGREE or DISAGREE

## File format

```tsv
# kct-wave-NN.tsv — Independent review of wave N slots
# index<TAB>verdict<TAB>xref_token<TAB>notes
2	AGREE	fastcall **)(a1 + 40)	icall at 0x28 confirmed; reads 0,8,16,24,32 all covered
```

Columns:
- `index` — KCT slot index (0–188)
- `verdict` — `AGREE` or `DISAGREE`
- `xref_token` — verbatim token from the decomp artifact, different from the primary evidence token
- `notes` — one line: extracted icall offset, read offsets checked, any DISAGREE reason

## Gate integration

`kct_audit.py --ledger` requires an AGREE entry for every `implemented` or `deferred` row.
A DISAGREE blocks the wave: the primary analysis must be corrected before the ledger is updated.
