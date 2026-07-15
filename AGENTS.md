# Eidolon repository conventions

- Use GNU Make for Eidolon itself. Upstream build systems may be used only for quarantined host
  tools and third-party dependencies.
- Keep dependencies as ordinary vendored source, not Git submodules.
- The first commit in a new repository is always named exactly `init`.
- Do not commit build output, downloaded compiler payloads, logs, or source-asset archives.
