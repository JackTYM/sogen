# Two trivial exes with deliberately different embedded manifests, used only to differentially
# analyze the real ACTIVATION_CONTEXT_DATA blob Windows builds - see
# docs/superpowers/plans/2026-09-24-comctl32-sxs-activation-context.md Task 6 for why. Not part
# of the shipped analyzer; scratch tooling for this one task's investigation.

add_executable(actctx-test-app-zero-deps trivial_app.cpp zero_deps.rc)
sogen_strip_target(actctx-test-app-zero-deps)

add_executable(actctx-test-app-one-dep trivial_app.cpp one_dep.rc)
sogen_strip_target(actctx-test-app-one-dep)

if(MSVC)
  # The linker auto-embeds its own default manifest (resource ID 1) unless told not to - our
  # .rc files already embed our own hand-written manifest at ID 1, and having both collides.
  target_link_options(actctx-test-app-zero-deps PRIVATE /MANIFEST:NO)
  target_link_options(actctx-test-app-one-dep PRIVATE /MANIFEST:NO)
endif()
