# Dear Bindings vendor record

- upstream: https://github.com/dearimgui/dear_bindings
- revision: `c9ff64913915df41c0f4beef485b98a1c685eda5`

This is an ordinary vendored snapshot, not a submodule or nested repository. The committed
`generated/` C API targets the adjacent Dear ImGui snapshot. Regeneration requires Python and the
pinned `ply` version in `requirements.txt`; ordinary Eidolon builds do not.
