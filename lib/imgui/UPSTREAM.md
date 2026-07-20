# Dear ImGui vendor record

- upstream: https://github.com/ocornut/imgui
- version: `v1.92.6`
- revision: `6ded5230d043aa32c755e65c910c2af5002fb9f9`

This is an ordinary vendored snapshot, not a submodule or nested repository. Eidolon compiles the
Dear ImGui core and official SDL3/SDL_Renderer3 backends with `clang++`; application code consumes
them only through the generated Dear Bindings C API.
