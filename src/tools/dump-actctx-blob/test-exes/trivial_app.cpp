// Deliberately does nothing. Only exists to be launched CREATE_SUSPENDED by dump-actctx-blob -
// the activation-context blob this experiment cares about is built by CSRSS before any of this
// code ever runs, so the body is irrelevant. See test-manifests/ for what actually varies.
int main()
{
    return 0;
}
