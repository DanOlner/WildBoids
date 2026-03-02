# Cross-Platform Portability Plan

## Current State: Mostly Portable, With a Few Gotchas

No `#ifdef __APPLE__`, no platform-specific headers, no POSIX calls. The simulation library is pure C++ with no OS dependencies. The work is entirely in CMake configuration, one math constant, and (optionally) the ImGui migration.

---

## 1. `M_PI` Is Not Standard C++ (Medium priority)

Used in 5 source files and 5 test files. `M_PI` is a POSIX extension, not part of the C++ standard. It works on macOS and Linux because `<cmath>` defines it, but **MSVC does not define `M_PI` by default** unless you `#define _USE_MATH_DEFINES` before including `<cmath>`.

**Fix:** Define our own constant (e.g. in `vec2.h`):
```cpp
inline constexpr float PI = 3.14159265358979323846f;
```
Then replace all `M_PI` usages. This is the single most common Windows porting gotcha in C++ math code.

**Files affected:**
- `src/headless_main.cpp`
- `src/io/boid_spec.cpp`
- `src/simulation/sensor.h`
- `tests/test_boid_spec.cpp`
- `tests/test_sensor.cpp`
- `tests/test_dual_evolution.cpp`
- `tests/test_headless.cpp`
- `tests/test_evolution.cpp`

---

## 2. SDL3 Discovery — Switch `find_package` to `FetchContent` (High priority)

Currently `find_package(SDL3 REQUIRED)` relies on SDL3 being pre-installed via Homebrew on macOS. This won't work out-of-the-box on Windows or Linux.

**Recommended: FetchContent (matches existing pattern for Catch2 and nlohmann/json).**

```cmake
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.8  # or whatever current stable is
)
FetchContent_MakeAvailable(SDL3)
```

Then remove the `find_package(SDL3 REQUIRED)` line — everything else stays the same.

**Alternatives:**
- **vcpkg / Conan:** Cross-platform package managers, but add toolchain complexity
- **Pre-built binaries:** SDL provides official Windows dev packages (`.zip`), but manual

---

## 3. Sanitizer Flags Are GCC/Clang-Only (Low-medium priority)

```cmake
add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
add_link_options(-fsanitize=address,undefined)
```

MSVC doesn't understand `-fsanitize`. This will break the configure step on Windows with MSVC.

**Fix:**
```cmake
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    elseif(MSVC)
        add_compile_options(/fsanitize=address)  # MSVC has ASan but not UBSan
    endif()
endif()
```

---

## 4. Ninja Generator Assumption (Low priority)

Build instructions say `cmake -B build -G Ninja`. Ninja isn't installed by default on Windows or many Linux distros.

**Options:**
- Document that Ninja needs installing (`pip install ninja`, `choco install ninja`, `apt install ninja-build`)
- Or omit `-G Ninja` from cross-platform instructions and let CMake use the platform default (Visual Studio on Windows, Make on Linux)

Not a code issue — just documentation.

---

## 5. `std::filesystem` — Fine (Non-issue)

Used in `headless_main.cpp` for `create_directories`. This is C++17 standard and works everywhere now. On older GCC (< 9) you'd need `-lstdc++fs`, but since we target C++20 this is fine on any modern compiler.

---

## 6. SDL3 Rendering Backend (Already portable)

The renderer uses `SDL_CreateRenderer(window_, nullptr)` — passing `nullptr` lets SDL pick the best backend automatically (Metal on macOS, D3D11/12 on Windows, OpenGL/Vulkan on Linux). Only uses `SDL_RenderLine`, `SDL_RenderGeometry`, `SDL_RenderFillRect` — all backend-agnostic. **No issues here.**

---

## 7. C++20 Feature Compatibility (Low risk)

`CMAKE_CXX_STANDARD 20` is set. Features actually used:
- Designated initializers (e.g. `{.x = 1, .y = 2}`)
- `std::variant`, `std::optional` (C++17)
- No modules, no coroutines, no `std::format`

Safe territory. MSVC, GCC, and Clang all handle the C++20 subset we use.

---

## 8. Path Separators (Non-issue)

JSON data paths like `"data/sim_config.json"` use forward slashes. Windows APIs and the C++ standard library accept forward slashes. The one place to be careful: if constructing paths by string concatenation, use `std::filesystem::path` `/` operator instead.

---

## 9. `std::cerr` for Logging (Fine)

Works everywhere. If running from Windows Explorer (not a terminal), you'd need to attach a console or redirect — but for a CLI/dev tool this is fine.

---

## 10. Dear ImGui Integration Plan

ImGui is not a standalone framework — it needs a rendering backend + platform backend.

### Recommended: SDL3 + SDL_Renderer backend

This is the simplest option because:
- We already use SDL3 for windowing
- ImGui ships with `imgui_impl_sdl3.cpp` + `imgui_impl_sdlrenderer3.cpp` backends
- SDL_Renderer automatically picks the right GPU backend per platform (same as now)
- Existing draw code (triangles, lines, rects) stays almost identical — just add ImGui widget calls on top
- No need to write any OpenGL/Vulkan/Metal code

### Alternative: SDL3 + OpenGL3 backend

More power (custom shaders later), but requires:
- Replacing `SDL_RenderLine`/`SDL_RenderGeometry` calls with raw OpenGL draw calls (or ImGui draw list API)
- Handle OpenGL context creation
- Platform-specific GL headers
- More code, more flexibility

### CMake setup for ImGui

```cmake
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.8
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC SDL3::SDL3)
```

### Render loop integration

ImGui and existing SDL_Renderer draw calls coexist in the same frame — ImGui renders on top:

```cpp
// Start of frame
ImGui_ImplSDLRenderer3_NewFrame();
ImGui_ImplSDL3_NewFrame();
ImGui::NewFrame();

// Existing draw code (SDL_RenderGeometry etc.) stays
renderer_.draw(world_);

// Add ImGui widgets
ImGui::Begin("Controls");
ImGui::Checkbox("Show Thrusters", &show_thrusters);
ImGui::SliderInt("Speed", &speed, 1, 8);
if (ImGui::Button(paused ? "Resume" : "Pause")) paused = !paused;
ImGui::End();

// End of frame
ImGui::Render();
ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), sdl_renderer);
SDL_RenderPresent(sdl_renderer);
```

---

## Priority Summary

| Priority | Issue | Effort |
|---|---|---|
| **High** | SDL3 discovery — switch `find_package` to `FetchContent` | ~10 lines of CMake |
| **Medium** | `M_PI` — define own constant, search-and-replace | ~30 min |
| **Medium** | Sanitizer flags — guard with compiler ID check | ~5 lines of CMake |
| **Low** | Document Ninja as optional | README edit |
| **Optional** | Add ImGui for proper GUI controls | ~50 lines setup + incremental widget migration |

**The simulation library, NEAT brain code, JSON I/O, and headless runner need zero changes — they're already fully portable C++20.** The work is entirely in CMake configuration and the `M_PI` constant.
