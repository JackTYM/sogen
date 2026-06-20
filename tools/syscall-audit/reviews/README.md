# Independent Wave Reviews

Each file `wave-NN.tsv` contains the result of an independent blind review of a wave's verified rows.
A fresh subagent reads the kernel artifact + sogen handler for N sampled rows (without seeing the
primary evidence) and renders an independent verdict.

Format (tab-separated, no header):
```
nt_name    AGREE|DISAGREE    [xref:TOKEN]    notes
```

- `AGREE`: the reviewer independently confirms the [MATCH]/[FIXED]/[DEFER] claim is credible
- `DISAGREE`: the reviewer found a discrepancy — the wave is blocked until resolved
- `[xref:TOKEN]`: a verbatim token the reviewer independently pulled from the artifact (different
  from the primary evidence token, proving independent read)

The audit (`audit.py --ledger`) requires every verified handler to appear here with AGREE before
the row can count as verified.
