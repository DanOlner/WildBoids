# Wild Boids: Platform Assessment for JavaScript Implementation

Claude generated doc via this prompt: "We'll be aiming to expand this (adding in various extra features) but first let's talk about platforms. Considering the info in @oldjavacode_summary.md, collate views via sources on whether implementing a similar simulation using javascript would be feasible given it's 19 years later. If using javascript, are there any appropriate frameworks, libraries or other tools that would support this? Bearing in mind we want to keep it lean so it runs fast - so it may be better to implement an agent framework ourselves. Answer this by adding text into the @plan_newplatform.md file."

**Question:** Is it feasible to implement the Wild Boids simulation in JavaScript in 2026, given it was originally written in Java in 2007?

**Short Answer:** Yes, absolutely. JavaScript in 2026 is vastly more capable than 2007's browser environment. The original simulation ran 75 predator/prey pairs (150 boids total) with 6 FOV checks each. Modern JavaScript can handle thousands of boids at 60fps with proper optimization.

---

## Performance Context: Then vs Now

### 2007 Java Implementation
- Swing-based GUI with Java2D rendering
- Single-threaded CPU execution
- Desktop-only deployment
- O(n²) collision detection with basic buffer optimization

### 2026 JavaScript Capabilities
- [GPU-accelerated rendering via WebGL/WebGPU](https://pixijs.com/blog/pixi-v8-launches)
- [Multi-threaded computation via Web Workers](https://developer.mozilla.org/en-US/docs/Web/API/Web_Workers_API)
- [OffscreenCanvas for thread-separated rendering](https://medium.com/@lightxdesign55/using-web-workers-and-offscreencanvas-for-smooth-rendering-in-javascript-1c9df43fdb52)
- Runs anywhere: desktop, mobile, embedded in web pages

---

## Benchmark Evidence

| Implementation | Performance |
|----------------|-------------|
| [hughsk/boids](https://github.com/hughsk/boids) | 1,000 boids @ 60fps on MacBook |
| [BoidsJS with WebWorkers](https://ercang.github.io/boids-js/) | 4 parallel threads, UI stays 60fps even when sim slows |
| [WebGL GPGPU boids](https://github.com/markdaws/webgl-boid) | 4,000 boids @ 60fps using GPU computation |
| [Canvas 2D (unoptimized)](https://jumpoff.io/blog/implementing-boids-in-javascript-canvas) | ~800 boids before dropping below 30fps |

**Key insight:** The original Wild Boids used ~150 agents. Even unoptimized Canvas 2D can handle 5x that number. With spatial partitioning and WebGL, we could scale to 10,000+ agents.

---

## Rendering Technology Options

### Option 1: Canvas 2D (Simplest)
**Best for:** Quick prototype, small agent counts (<500)

**Pros:**
- Simplest API, minimal learning curve
- No dependencies needed
- Good enough for 150-500 boids

**Cons:**
- [CPU-bound, can't batch draw calls](https://discourse.wicg.io/t/why-is-canvas-2d-so-slow/2232/)
- Performance degrades quickly above 800 agents
- Limited to ~25fps with 10,000 particles

**Verdict:** Suitable for initial prototype, but will limit future scaling.

---

### Option 2: WebGL (via library or vanilla)
**Best for:** Production implementation, 1,000-10,000 agents

**Pros:**
- [GPU parallel processing handles thousands of agents](https://dev.to/hexshift/building-a-custom-gpu-accelerated-particle-system-with-webgl-and-glsl-shaders-25d2)
- Can offload position/velocity calculations to GPU shaders
- 50,000 scatter plots: Canvas=22fps, WebGL=58fps ([benchmark](https://digitaladblog.com/2025/05/21/comparing-canvas-vs-webgl-for-javascript-chart-performance/))

**Cons:**
- Steeper learning curve than Canvas 2D
- Slower initial load (~40ms vs ~15ms for Canvas)
- Requires careful optimization to beat naive Canvas

**Library Options:**
- **[PixiJS v8](https://pixijs.com/8.x/guides/getting-started/intro):** Best balance of ease-of-use and performance. Now supports WebGPU fallback. Optimized batching.
- **Vanilla WebGL:** Maximum control, can run particles entirely on GPU, but significant development overhead.

**Verdict:** Recommended for production. PixiJS provides 80-90% of vanilla performance with 20% of the code.

---

### Option 3: WebGPU (Cutting Edge)
**Best for:** Future-proofing, massive scale (10,000+ agents), compute-heavy simulations

**Pros:**
- [10-100x speedups for data processing and simulations](https://bhavyansh001.medium.com/forget-webassembly-webgpu-is-the-real-revolution-developers-should-watch-4539ff7c57a5)
- True compute shaders (not hacks like WebGL GPGPU)
- [Now supported in all major browsers](https://web.dev/blog/webgpu-supported-major-browsers) (Chrome, Edge, Firefox 141+, Safari 26+)

**Cons:**
- Still maturing (some memory-related bugs)
- Variable device support (GPU memory limits differ)
- Overkill for <1000 agents

**Verdict:** Consider for v2 if we want massive swarms. PixiJS v8 already supports WebGPU as backend.

---

## Spatial Partitioning (Critical for Performance)

The original Java code used a Buffer system to optimize edge detection. For JavaScript, we need spatial partitioning for the O(n²) neighbor detection problem.

### Option A: Uniform Grid (Recommended)
**From research:** ["Speeds up collision detection by 10-100x"](http://buildnewgames.com/broad-phase-collision-detection/)

- Simple to implement
- Constant-time cell lookup
- [Works well with SIMD and multithreading](https://pvigier.github.io/2019/08/04/quadtree-collision-detection.html)
- Best when agents are similar sizes (which they are in Wild Boids)

### Option B: Quadtree
- Better for varying-size objects
- More memory efficient for sparse data
- [Slightly slower than grid for uniform objects](https://zufallsgenerator.github.io/2014/01/26/visually-comparing-algorithms)

**Verdict:** Implement uniform grid. Wild Boids has uniform-sized FOV polygons, so grid is optimal.

---

## Threading Architecture

### Recommended: Simulation in Web Worker + OffscreenCanvas

```
Main Thread          Worker Thread
    │                     │
    │  transferControl    │
    ├────────────────────>│
    │                     │
    │  (UI responsive)    │  ← Simulation loop
    │                     │  ← FOV checks
    │                     │  ← Movement calc
    │                     │  ← Evolution
    │                     │
    │<──── render ────────│  (via OffscreenCanvas)
```

**Benefits:**
- [UI never freezes even during heavy computation](https://macarthur.me/posts/animate-canvas-in-a-worker/)
- Simulation can run at different rate than rendering
- Clean separation of concerns

**Reference:** [BoidsJS implements this pattern](https://ercang.github.io/boids-js/) - even when sim slows, UI stays at 60fps.

---

## Framework Recommendation

### Build Custom (Recommended)

Given the goal to "keep it lean so it runs fast," I recommend **building a custom agent framework** rather than using a general ABM library.

**Reasons:**
1. [AgentScript](http://agentscript.org/) and [js-simulator](https://github.com/chen0040/js-simulator) are general-purpose and add overhead
2. Wild Boids has specific needs (FOV polygons, torus world, genetic crossover) that don't map to standard ABM patterns
3. Custom code can be optimized for our exact use case
4. Smaller bundle size, faster load times

### Rendering Layer: PixiJS v8
- Handles WebGL/WebGPU complexity
- [Excellent batching for many similar objects](https://fgfactory.com/webgl-libraries-for-2d-games)
- Lightweight (~200KB minified)
- Hardware-accelerated camera for panning/zooming
- Drop-in [OffscreenCanvas support](https://github.com/pmndrs/react-three-offscreen)

---

## Proposed Architecture

```
┌─────────────────────────────────────────────────────────┐
│                     Main Thread                          │
│  ┌─────────────────┐  ┌──────────────────────────────┐  │
│  │   UI Controls   │  │   PixiJS Renderer            │  │
│  │   (HTML/CSS)    │  │   (WebGL/WebGPU backend)     │  │
│  └─────────────────┘  └──────────────────────────────┘  │
│           │                        ▲                     │
│           │ params                 │ positions/states    │
│           ▼                        │                     │
│  ┌─────────────────────────────────┴──────────────────┐  │
│  │              SharedArrayBuffer                      │  │
│  └─────────────────────────────────┬──────────────────┘  │
└────────────────────────────────────│────────────────────┘
                                     │
┌────────────────────────────────────│────────────────────┐
│                    Worker Thread   │                     │
│  ┌─────────────────────────────────▼──────────────────┐  │
│  │              Simulation Engine                      │  │
│  │  ┌──────────────┐  ┌──────────────┐                │  │
│  │  │ Spatial Grid │  │  Boid Array  │                │  │
│  │  └──────────────┘  └──────────────┘                │  │
│  │  ┌──────────────┐  ┌──────────────┐                │  │
│  │  │ FOV Checker  │  │  Evolution   │                │  │
│  │  └──────────────┘  └──────────────┘                │  │
│  └─────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────┘
```

---

## Implementation Phases

### Phase 1: Proof of Concept
- Canvas 2D rendering (no dependencies)
- Single-threaded
- 150 boids (match original)
- Basic FOV detection (simplified to circles initially)
- No evolution yet

### Phase 2: Core Simulation
- Port full FOV polygon system
- Implement uniform grid spatial partitioning
- Add evolution/genetics system
- Still single-threaded, Canvas 2D

### Phase 3: Performance Optimization
- Move simulation to Web Worker
- Switch to PixiJS for rendering
- Add SharedArrayBuffer for zero-copy data transfer
- Target: 1,000+ boids @ 60fps

### Phase 4: Enhancements
- WebGPU compute shaders (optional, for massive scale)
- Parameter visualization/graphs
- Save/load evolved populations
- Multiple species

---

## Conclusion

**JavaScript is absolutely viable** for Wild Boids in 2026. The platform has matured enormously since 2007:

| Concern | 2007 | 2026 |
|---------|------|------|
| Threading | None | Web Workers + SharedArrayBuffer |
| GPU Access | None | WebGL, WebGPU |
| Performance | Interpreted, slow | JIT compiled, near-native |
| Deployment | Plugin required (Java) | Native browser support |

**Recommended Stack:**
- **Rendering:** PixiJS v8 (WebGL with WebGPU fallback)
- **Threading:** Web Worker for simulation
- **Spatial:** Custom uniform grid
- **Framework:** Custom (lean, purpose-built)

The original 150-boid simulation will be trivial. With optimization, we can scale to thousands of evolving agents running smoothly in any modern browser.

---

## Addendum: Native C++ / Apple Silicon Options

The JavaScript browser approach above is one path. Another is to go native — C++ targeting Apple Silicon directly, gaining access to GPU compute and (potentially) the Neural Engine. Here's what that looks like.

---

### Can We Access the Neural Engine?

**Short answer: not directly, and not for general-purpose compute.**

The Apple Neural Engine (ANE) is a specialised ML inference accelerator. There is no public API for programming it directly — the low-level `AppleNeuralEngine.framework` is private to Apple. The only way to run work on the ANE is indirectly:

- **Core ML:** You submit a trained ML model, and Core ML *automatically* decides whether to run it on CPU, GPU, or ANE. You can't force ANE usage or run arbitrary compute through it.
- **MLX (Apple's ML framework):** Can distribute tensor operations across ANE, GPU, and CPU, but again only for ML-shaped workloads (matrix ops, convolutions, etc.).

**Bottom line for Wild Boids:** The Neural Engine isn't useful here. Our workload is agent-based simulation (spatial queries, per-agent logic, physics), not ML inference. The GPU is the right accelerator for this kind of work.

*However* — if we later added neural-network brains to boids (instead of direct sensor→thruster weight mappings), Core ML or MLX could potentially run those on the ANE. That's a future consideration, not a starting point.

---

### Option 1: C++ with Metal Compute Shaders (Maximum Performance)

**The most powerful native option on Apple Silicon.**

Metal is Apple's low-level GPU API, and [metal-cpp](https://developer.apple.com/metal/cpp/) provides a header-only C++ wrapper. With Metal 4 (announced WWDC 2025), the API is mature and well-supported.

**Why this is attractive for boids:**

- **Unified memory architecture:** On Apple Silicon, CPU and GPU share the same memory pool. Boid state (positions, velocities, sensor data) lives in one place — no copying between CPU and GPU. This is a *major* advantage for simulations where both CPU logic and GPU compute need the same data.
- **Compute shaders:** Boid physics, spatial grid updates, and sensor queries can all run as Metal compute shaders on the GPU. Benchmarks show [GPU boids implementations running 42x faster than single-core CPU](https://www.diva-portal.org/smash/get/diva2:1191916/FULLTEXT01.pdf).
- **Scale:** GPU implementations handle [1 million boids at interactive framerates](https://toytag.net/posts/boids/). Even the M1's 1.36 FP32 TFLOPS is massive overkill for our use case; M4 delivers 2.9 TFLOPS.
- **Existing examples:** Apple provides N-body simulation sample code in metal-cpp. Several open-source projects demonstrate [scientific C++ computing on M1 GPUs](https://larsgeb.github.io/2022/04/20/m1-gpu.html) and [particle-based fluid simulation via Metal](https://github.com/Al0den/Fluid-Simulator).

**Architecture sketch:**

```
┌──────────────────────────────────────────────────────┐
│                    C++ Application                    │
│  ┌────────────────┐  ┌─────────────────────────────┐ │
│  │   UI / Window  │  │   Simulation Controller     │ │
│  │   (AppKit or   │  │   (dispatch compute,        │ │
│  │    SDL2/GLFW)  │  │    read results, evolve)    │ │
│  └────────────────┘  └─────────────────────────────┘ │
│           │                       │                   │
│           │ render                │ dispatch           │
│           ▼                       ▼                   │
│  ┌─────────────────────────────────────────────────┐ │
│  │              Metal (via metal-cpp)               │ │
│  │  ┌──────────────┐  ┌──────────────────────────┐ │ │
│  │  │ Render Pass  │  │   Compute Shaders        │ │ │
│  │  │ (draw boids) │  │   - Spatial grid build   │ │ │
│  │  │              │  │   - Neighbour queries     │ │ │
│  │  │              │  │   - Physics (forces)      │ │ │
│  │  │              │  │   - Sensor evaluation     │ │ │
│  │  └──────────────┘  └──────────────────────────┘ │ │
│  │              Unified Memory (shared buffers)     │ │
│  └─────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────┘
```

**Pros:**
- Maximum performance on Apple Silicon
- Zero-copy CPU↔GPU data sharing via unified memory
- Full control over compute pipeline
- Mature tooling (Xcode, Metal debugger, GPU profiler)

**Cons:**
- Apple-only (no Windows/Linux)
- Steeper learning curve than JavaScript
- Metal Shading Language is its own thing (C++-like but not C++)
- Windowing/UI requires either AppKit (Objective-C++) or a cross-platform layer like SDL2

---

### Option 2: C++ with Vulkan via MoltenVK (Cross-Platform)

If portability beyond macOS matters, Vulkan is the cross-platform GPU compute/rendering API. [MoltenVK](https://github.com/KhronosGroup/MoltenVK) translates Vulkan calls to Metal on Apple Silicon, and as of August 2025 supports Vulkan 1.4.

**Pros:**
- Same codebase runs on macOS, Windows, Linux
- Vulkan compute shaders are well-documented for simulation workloads
- Large ecosystem of tools and examples

**Cons:**
- Translation layer adds some overhead vs native Metal
- Vulkan API is notoriously verbose
- MoltenVK is "nearly conformant" — occasional edge cases

**Verdict:** Worth considering if you want to share the codebase with non-Mac users, but adds complexity for no performance gain on Apple Silicon.

---

### Option 3: C++ with SpriteKit/Metal Hybrid (Easiest Native Path)

SpriteKit is Apple's 2D game framework, built on Metal. You could use SpriteKit for rendering and Metal compute shaders for the simulation logic.

**Pros:**
- SpriteKit handles all 2D rendering, camera, batching
- Metal compute shaders handle the heavy simulation work
- Less boilerplate than pure Metal rendering
- Swift or Objective-C++ for app structure, C++ for compute

**Cons:**
- Apple-only
- SpriteKit adds constraints — less rendering flexibility than raw Metal
- Mixing Swift/ObjC++ and C++ can be awkward

---

### Option 4: C++ with CPU-Only Optimization (Simplest, No GPU)

Apple's [Accelerate framework](https://developer.apple.com/accelerate/) provides SIMD-optimized routines for vector math, matrix operations, and DSP — all tuned for Apple Silicon's ARM cores.

For moderate boid counts (hundreds to low thousands), a well-optimized CPU-only implementation using Accelerate's `vDSP` and `simd` types may be fast enough, avoiding GPU complexity entirely.

**Pros:**
- Simplest architecture — no GPU pipeline to manage
- Accelerate provides 2-10x speedups via SIMD vectorisation
- Easy to debug and profile
- Could use SDL2 or SFML for cross-platform windowing + rendering

**Cons:**
- Hits a ceiling at high boid counts (tens of thousands)
- Can't match GPU throughput for embarrassingly parallel workloads

---

### Comparison Summary

| Approach | Performance Ceiling | Portability | Complexity | Best For |
|----------|-------------------|-------------|------------|----------|
| **Metal + metal-cpp** | 100K+ boids | Apple only | High | Maximum scale, Apple-focused |
| **Vulkan + MoltenVK** | 100K+ boids | Cross-platform | Very high | Multi-platform deployment |
| **SpriteKit + Metal compute** | 100K+ boids | Apple only | Medium | Faster dev, Apple-focused |
| **CPU + Accelerate** | ~5K boids | Cross-platform (with SDL2) | Low | Quick start, moderate scale |
| **JavaScript (WebGPU)** | 10K+ boids | Universal (browser) | Medium | Widest reach, good enough perf |

### Neural Engine: Summary

| Question | Answer |
|----------|--------|
| Can we program the ANE directly? | No — private API |
| Can we run simulation logic on it? | No — it only does ML inference |
| Could neural-net boid brains use it? | Yes, via Core ML or MLX, in the future |
| Is the GPU sufficient? | Absolutely — M-series GPUs are overkill for this simulation |

### Recommendation

If going native C++, **Option 1 (Metal + metal-cpp)** is the strongest choice for Apple Silicon. The unified memory architecture is a genuine advantage for agent simulations where CPU-side evolution logic and GPU-side physics/spatial computation share the same boid data. Start with compute shaders for physics and spatial queries, render with Metal's render pipeline, and handle evolution/genetics on the CPU side where the branching logic is a natural fit.

That said, the JavaScript/WebGPU path from the main document remains compelling for its zero-install, runs-anywhere nature. The choice depends on whether you prioritise raw performance headroom or accessibility.

---

## Addendum: Loose Coupling and Cross-Platform Graphics in C++

The architecture in [evolution_neuralnet_thrusters.md](evolution_neuralnet_thrusters.md) deliberately separates the simulation into layers (sensors → processing network → thrusters → physics) that communicate through simple float arrays. This section extends that principle to the boundary between the **simulation** and **everything the user sees and touches** — rendering, UI controls, graphs — and recommends cross-platform libraries to keep graphics write-once-able.

### The Core Principle: Simulation Knows Nothing About Display

The simulation engine should be a pure library with no graphics dependencies. It takes inputs (time step, world parameters), updates internal state, and exposes read-only state for anyone who wants to look at it. The renderer is a consumer of state, never a producer.

```
┌─────────────────────────────────────────────────────────────┐
│                    SIMULATION (no GUI deps)                  │
│                                                             │
│  World state:  boid positions, velocities, headings,        │
│                energy, thruster states, sensor activations,  │
│                species populations, fitness stats            │
│                                                             │
│  API:          world.step(dt)                                │
│                world.getBoidState() → const BoidState*       │
│                world.getStats() → SimStats                   │
│                world.setParam(name, value)                   │
└────────────────────────┬────────────────────────────────────┘
                         │ reads state (never writes)
                         ▼
┌─────────────────────────────────────────────────────────────┐
│                    DISPLAY LAYER (all GUI deps)              │
│                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐  │
│  │ 2D Renderer  │  │ Parameter UI │  │ Graphs / Plots   │  │
│  │ (draw boids) │  │ (sliders,    │  │ (fitness, pop    │  │
│  │              │  │  buttons)    │  │  counts, etc.)   │  │
│  └──────────────┘  └──────────────┘  └──────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

**What this buys us:**

- The simulation compiles and runs with **zero graphics libraries** — headless mode for batch experiments, parameter sweeps, overnight evolution runs
- Swap rendering technology without touching simulation code
- Test simulation logic with unit tests, no window required
- GPU compute for simulation (Metal/Vulkan) doesn't conflict with GPU rendering — they're in different compilation units with different dependencies

### Fixed Timestep, Decoupled Frame Rate

The simulation and rendering should run at different rates. The simulation needs a fixed timestep for deterministic physics; the renderer should run as fast as the display allows.

The standard pattern ([Gaffer on Games](https://gafferongames.com/post/fix_your_timestep/)):

```cpp
double t = 0.0;
const double dt = 1.0 / 120.0;  // simulation at 120Hz
double accumulator = 0.0;

while (running) {
    double frameTime = getElapsedTime();
    accumulator += frameTime;

    // Fixed-step simulation updates
    while (accumulator >= dt) {
        world.step(dt);
        accumulator -= dt;
        t += dt;
    }

    // Render at whatever rate the display wants
    double alpha = accumulator / dt;  // for interpolation
    renderer.draw(world, alpha);
    ui.draw(world);
}
```

The simulation can also run **without any rendering at all** — just call `world.step(dt)` in a tight loop. This is essential for running evolution experiments at maximum speed where you don't care about visuals.

---

### Cross-Platform Graphics Library Options

For a few hundred boids and a parameter interface, the graphics are simple: draw coloured triangles/arrows for boids, draw sensor cones/arcs, draw UI controls and graphs. We don't need a 3D engine. We need something that handles windowing, basic 2D rendering, and UI — and works on macOS, Windows, and Linux without rewriting.

#### Option 1: SDL3 + Dear ImGui + ImPlot (Recommended)

**[SDL3](https://wiki.libsdl.org/SDL3/CategoryGPU)** — the successor to SDL2, released stable January 2025 (SDL 3.2.0), actively maintained through 2026 (3.2.30+).

SDL3's headline feature is the **SDL_GPU API**: a unified graphics abstraction that natively targets **Metal** (macOS), **Vulkan** (Linux, Windows), and **DirectX 12** (Windows). You write one rendering path; SDL3 picks the right backend. For our 2D boid rendering, `SDL_RenderGeometry` handles triangles, lines, and shapes with hardware acceleration.

**[Dear ImGui](https://github.com/ocornut/imgui)** — immediate-mode GUI for C++. Provides sliders, buttons, checkboxes, text inputs, collapsible panels — all the parameter controls we need. Has native SDL3 backends (`imgui_impl_sdl3.cpp` + `imgui_impl_sdlgpu3.cpp`). Integrates in ~20 lines of setup code.

**[ImPlot](https://github.com/epezent/implot)** — GPU-accelerated plotting for Dear ImGui. Real-time line graphs, scatter plots, histograms. Can plot tens of thousands of points smoothly. Perfect for fitness-over-generations, population counts, energy distributions, genome diversity metrics.

**Why this combination:**

- **Genuinely cross-platform:** SDL3's GPU API handles Metal/Vulkan/DX12 automatically. Write once, runs on macOS (Apple Silicon), Windows, Linux.
- **SDL3 is the future:** SDL2 is in maintenance mode. All new projects should use SDL3.
- **Dear ImGui + ImPlot** give us the full parameter/analysis UI with minimal effort. No need for a heavy GUI framework (Qt, wxWidgets) — ImGui is immediate-mode, header-only, and renders via SDL3's own backend.
- **Low coupling:** SDL3 is a C library called from a thin C++ wrapper. It doesn't infect the rest of the codebase with its types or patterns.
- **Coexists with Metal/Vulkan compute:** SDL3 handles the rendering window and 2D drawing; Metal compute shaders (or Vulkan compute) can run the simulation in separate command buffers on the same GPU. Unified memory on Apple Silicon means no data copying between compute and rendering.

```
┌────────────────────────────────────────────────────────────┐
│                     Application                            │
│                                                            │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  Simulation Engine (pure C++, no deps)              │   │
│  │  - Boid state, NEAT networks, evolution, physics    │   │
│  │  - Optional: Metal/Vulkan compute for spatial grid  │   │
│  └───────────────────────┬─────────────────────────────┘   │
│                          │ const state reference            │
│  ┌───────────────────────▼─────────────────────────────┐   │
│  │  Display Layer (SDL3 + ImGui + ImPlot)              │   │
│  │  - SDL3 window + GPU rendering (Metal/Vulkan/DX12) │   │
│  │  - SDL3 2D drawing: boid shapes, sensor arcs        │   │
│  │  - ImGui: parameter sliders, species controls       │   │
│  │  - ImPlot: fitness graphs, population charts        │   │
│  └─────────────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────────────┘
```

#### Option 2: SFML 3 + Dear ImGui + ImPlot (Simpler, C++ Native)

**[SFML 3](https://www.sfml-dev.org/)** — released December 2024 (3.0.0), uses C++17, batteries-included 2D graphics library. SFML feels more naturally C++ than SDL (which is a C library). Has built-in 2D drawing primitives, sprite rendering, text, views/cameras.

**Pros over SDL3:**
- More idiomatic C++ API — no C-style function calls
- Built-in 2D primitives (circles, rectangles, convex shapes, vertex arrays) — no need for manual triangle submission
- Easier to get started; less boilerplate

**Cons vs SDL3:**
- Uses OpenGL under the hood, not Metal/Vulkan/DX12 natively. On macOS, this means OpenGL 4.1 (Apple's last supported version). Adequate for our simple 2D rendering, but less future-proof.
- Narrower platform support (no mobile, no consoles — not that we need them)
- [ImGui-SFML](https://github.com/SFML/imgui-sfml) binding exists but is a third-party layer

**Verdict:** If you want the most pleasant C++ development experience and don't care about native Metal rendering for the *display* (Metal compute for simulation is separate), SFML 3 is excellent. The OpenGL-on-macOS limitation is irrelevant for drawing a few hundred triangles.

#### Option 3: Raylib (Fastest to Prototype)

**[Raylib](https://www.raylib.com/)** — radically simple C library for games and visualizations. Follows the KISS principle. A boid renderer in raylib is about 30 lines of code.

**Pros:**
- Lowest learning curve of any option
- Comprehensive 2D drawing functions built in
- Cross-platform (macOS, Windows, Linux, even WebAssembly)
- Single-header-ish simplicity

**Cons:**
- Less suitable for complex UI (no native ImGui integration, though third-party bindings exist)
- OpenGL-based (same macOS caveat as SFML)
- Smaller ecosystem than SDL3

**Verdict:** Best for "get something on screen today." Could be a good choice for early prototyping before committing to SDL3 for the final version.

### Library Comparison

| Feature | SDL3 + ImGui | SFML 3 + ImGui | Raylib |
|---------|-------------|---------------|--------|
| **macOS (Metal backend)** | Yes (native) | No (OpenGL) | No (OpenGL) |
| **Windows (DX12/Vulkan)** | Yes (native) | No (OpenGL) | No (OpenGL) |
| **Linux (Vulkan)** | Yes (native) | No (OpenGL) | No (OpenGL) |
| **2D drawing ease** | Moderate (geometry API) | Easy (built-in shapes) | Very easy |
| **ImGui integration** | Official backends | Third-party (ImGui-SFML) | Third-party |
| **ImPlot (graphs)** | Yes (via ImGui) | Yes (via ImGui) | Manual |
| **API style** | C with C++ wrappers | Native C++ | C |
| **Future-proof** | Most (SDL3 is actively evolving) | Good | Good |
| **Coexists with Metal compute** | Yes (same GPU, shared context) | Awkward (OpenGL ≠ Metal) | Awkward |
| **Actively maintained (2026)** | Yes | Yes | Yes |
| **Learning curve** | Moderate | Easy | Very easy |

### What About Qt / wxWidgets / Native GUI?

Heavy GUI frameworks like Qt are overkill here. They bring massive dependencies, complex build systems, and licensing considerations. Dear ImGui gives us everything we need for simulation controls in a fraction of the complexity. If we later want a polished standalone application, we could revisit — but for a simulation tool, ImGui is the standard choice.

### Do We Need ECS?

[Entity Component Systems](https://en.wikipedia.org/wiki/Entity_component_system) (like [EnTT](https://github.com/skypjack/entt) or [Flecs](https://github.com/SanderMertens/flecs)) are popular in game engines for managing entities with varying component sets. For Wild Boids:

**Probably not.** We have two entity types (predator, prey) with nearly identical component structures (position, velocity, brain, sensors, thrusters, energy). A simple array of `Boid` structs with a type flag is cleaner than an ECS for this use case. ECS shines when you have dozens of entity types with varying component combinations — not our situation.

If we later add many entity types (food sources, obstacles, environmental features, multiple prey species with different sensor configurations), ECS could become worthwhile. But starting with plain C++ classes and arrays is simpler and faster to develop.

### Recommendation

**SDL3 + Dear ImGui + ImPlot** is the strongest cross-platform option. SDL3's native Metal/Vulkan/DX12 backends mean we write one rendering path that uses the best graphics API on each platform. Dear ImGui + ImPlot give us simulation controls and real-time graphs with minimal code. And the simulation engine stays completely decoupled — it's pure C++ with no knowledge of SDL, ImGui, or any display technology.

The build structure would look like:

```
wildboids/
├── src/
│   ├── simulation/        ← Pure C++, no graphics deps
│   │   ├── world.h/cpp
│   │   ├── boid.h/cpp
│   │   ├── rigid_body.h/cpp
│   │   ├── neat_network.h/cpp
│   │   ├── sensors.h/cpp
│   │   ├── thrusters.h/cpp
│   │   ├── evolution.h/cpp
│   │   └── spatial_grid.h/cpp
│   │
│   ├── display/           ← SDL3 + ImGui + ImPlot
│   │   ├── renderer.h/cpp
│   │   ├── ui_controls.h/cpp
│   │   └── plots.h/cpp
│   │
│   └── main.cpp           ← Wires simulation + display together
│
├── compute/               ← Metal/Vulkan compute shaders (optional)
│   ├── spatial_grid.metal
│   └── physics.metal
│
└── tests/                 ← Test simulation without any display
    ├── test_physics.cpp
    ├── test_neat.cpp
    └── test_evolution.cpp
```

The `simulation/` directory compiles independently. The `display/` directory depends on SDL3/ImGui but never on simulation internals — it reads `const BoidState*` and `SimStats`. Tests run the simulation headless at full speed.

---

## Addendum: Data Collection, Analysis and Boid Import/Export

### 1. Data Collection and Analysis Framework

#### Design Principle

Data collection follows the same decoupling principle as graphics: the simulation produces structured state; a separate data layer reads it. The simulation never knows it's being observed.

```
┌──────────────┐       const BoidState*        ┌──────────────────────┐
│  Simulation  │ ──────────────────────────────►│   DataCollector      │
│  (world.h)   │       const SimStats           │                      │
│              │ ──────────────────────────────►│   - per-tick metrics │
└──────────────┘                                │   - time-series      │
                                                │   - export to CSV    │
                                                │   - export to JSON   │
                                                └──────────────────────┘
```

`DataCollector` sits alongside `Renderer` in the main loop — both consume const simulation state, neither modifies it.

#### What to Measure

**Per-boid instantaneous state:**
| Metric | Source | Purpose |
|--------|--------|---------|
| Position (x, y) | Physics | Spatial distribution, territory mapping |
| Velocity (vx, vy), speed | Physics | Movement patterns |
| Heading | Physics | Alignment detection |
| Energy level | Boid state | Survival fitness proxy |
| Age (ticks alive) | Boid state | Longevity tracking |
| Species ID | NEAT speciation | Lineage tracking |
| Genome complexity (node/connection count) | Genome | Bloat monitoring |

**Population-level metrics (computed per tick or at intervals):**
| Metric | Computation | Detects |
|--------|-------------|---------|
| **Average alignment** | Mean of `cos(heading_i - heading_j)` for nearest neighbours | Proto-flocking, coordinated movement |
| **Clustering coefficient** | Count of boids within radius R of each boid, averaged | Schooling, herding, group formation |
| **Nearest-neighbour distance** (mean, std) | Spatial query | Spacing regularity, personal space |
| **Velocity correlation** | Pearson correlation of velocities within radius R | Coordinated acceleration/turning |
| **Species population counts** | Count per species ID | Speciation dynamics, competitive exclusion |
| **Mean genome complexity** | Avg nodes + connections per species | Structural evolution rate, bloat detection |
| **Predator catch rate** | Catches per predator per N ticks | Predator fitness over time |
| **Prey survival rate** | Proportion surviving past age threshold | Prey evasion effectiveness |
| **Spatial entropy** | Grid-based Shannon entropy of boid positions | Uniform spread vs. clumping |

Many of these are cheap to compute from data already maintained by the spatial partitioning grid.

#### Time-Series Storage

```cpp
struct MetricSnapshot {
    uint64_t tick;
    float avgAlignment;
    float clusteringCoefficient;
    float meanNNDistance;
    float spatialEntropy;
    // ... per-species counts stored separately
};

class DataCollector {
public:
    void sample(const World& world);       // called every N ticks
    void exportCSV(const std::string& path) const;
    void exportJSON(const std::string& path) const;

    // Ring buffer or vector depending on run length
    std::vector<MetricSnapshot> timeSeries;

    // Configurable sample rate (every tick is expensive for long runs)
    int sampleInterval = 10;  // every 10th tick
};
```

#### Live Visualisation (via ImPlot)

Because ImPlot is already in the recommended display stack, live time-series plots come almost free:

- Scrolling line plots of alignment, clustering, species counts
- Scatter plot of boid positions coloured by species or energy
- Histogram of genome complexity across population
- All plotting reads from `DataCollector`'s time-series buffer — no simulation dependency

#### Export Formats

- **CSV**: One row per snapshot tick, columns for each metric. Importable into R, Python/pandas, Excel. Best for statistical analysis.
- **JSON**: Richer structure, can include per-boid detail at specific ticks. Best for replaying / visualising specific moments.

---

### 2. Boid Import/Export (JSON)

#### Purpose

Serialise an entire boid — its genome, current state, and network topology — to human-readable JSON. This enables:

- **Saving/loading interesting specimens** mid-run
- **Seeding new simulations** with evolved boids from previous runs
- **Visualising the extended phenotype** — the sensor layout, network wiring, thruster arrangement — in external tools (e.g. a web-based genome viewer, or Python/networkx graph plots)
- **Comparing genomes** across evolutionary runs
- **Archiving "champion" boids** with full provenance

#### JSON Schema

Based on the genome structure defined in [evolution_neuralnet_thrusters.md](evolution_neuralnet_thrusters.md):

```json
{
  "version": "1.0",
  "type": "prey",
  "generation": 247,
  "speciesId": 12,
  "fitness": 8340.5,

  "sensors": [
    {
      "id": 0,
      "centerAngleDeg": 0.0,
      "arcWidthDeg": 60.0,
      "maxRange": 150.0,
      "entityFilter": "prey",
      "signalType": "nearestDistance"
    },
    {
      "id": 1,
      "centerAngleDeg": 45.0,
      "arcWidthDeg": 90.0,
      "maxRange": 200.0,
      "entityFilter": "predator",
      "signalType": "sectorDensity"
    }
  ],

  "network": {
    "nodes": [
      { "id": 0, "type": "input",  "label": "sensor_0" },
      { "id": 1, "type": "input",  "label": "sensor_1" },
      { "id": 5, "type": "hidden", "bias": 0.12, "activationFn": "tanh" },
      { "id": 6, "type": "modulatory", "bias": -0.3, "activationFn": "sigmoid" },
      { "id": 10, "type": "output", "label": "thruster_0" },
      { "id": 11, "type": "output", "label": "thruster_1" }
    ],
    "connections": [
      {
        "innovation": 1,
        "source": 0,
        "target": 5,
        "weight": 0.72,
        "enabled": true,
        "plasticity": {
          "alpha": 0.3,
          "eta": 0.01,
          "A": 1.0, "B": 0.0, "C": 0.0, "D": 0.0
        }
      },
      {
        "innovation": 7,
        "source": 5,
        "target": 10,
        "weight": -0.45,
        "enabled": true,
        "plasticity": {
          "alpha": 0.0,
          "eta": 0.0,
          "A": 0.0, "B": 0.0, "C": 0.0, "D": 0.0
        }
      }
    ]
  },

  "thrusters": [
    {
      "id": 0,
      "positionX": 0.3,
      "positionY": 0.0,
      "fireAngleDeg": 180.0,
      "maxThrust": 50.0
    },
    {
      "id": 1,
      "positionX": -0.3,
      "positionY": 0.0,
      "fireAngleDeg": 180.0,
      "maxThrust": 50.0
    }
  ]
}
```

#### Key Design Choices

- **Human-readable.** Anyone can open the file and see that sensor 1 is a 90° arc looking for predators at 45° to the right. The network wiring is explicit — you can trace signal flow by hand.
- **`alpha: 0.0` means "no plasticity."** When plasticity parameters are all zero, the connection behaves as a standard fixed-weight connection. This makes the JSON self-documenting: you can immediately see which connections learn and which are static.
- **Modulatory neurons are labelled.** You can see the neuromodulatory architecture at a glance.
- **Innovation numbers preserved.** Enables crossover between imported boids and the current population — genomes stay NEAT-compatible.
- **Runtime state excluded.** `plasticWeight` is not serialised because it resets to 0 each generation. If lifetime state needs saving (e.g. for mid-run snapshots), it can go in an optional `"runtimeState"` block.

#### Implementation

```cpp
// Serialisation uses nlohmann/json (header-only, de facto C++ standard)
#include <nlohmann/json.hpp>

namespace BoidIO {
    nlohmann::json toJSON(const Boid& boid);
    Boid fromJSON(const nlohmann::json& j);

    void saveToFile(const Boid& boid, const std::string& path);
    Boid loadFromFile(const std::string& path);

    // Batch export/import for entire populations
    void savePopulation(const std::vector<Boid>& pop, const std::string& path);
    std::vector<Boid> loadPopulation(const std::string& path);
}
```

[nlohmann/json](https://github.com/nlohmann/json) is header-only, widely used, and provides direct struct-to-JSON mapping via `NLOHMANN_DEFINE_TYPE_INTRUSIVE` macros — minimal boilerplate.

#### Visualising the Extended Phenotype

With boids exported to JSON, external tools can render the phenotype:

- **Sensor layout**: Draw the arc sectors around a boid silhouette, colour-coded by entity filter and signal type
- **Network graph**: Render the NEAT network as a directed graph (nodes positioned by layer, connections coloured by weight sign, thickness by magnitude, dashed if plastic)
- **Thruster layout**: Show thruster positions and fire directions on the boid body
- **Combined view**: Overlay all three — a complete picture of how a boid perceives, thinks, and moves

This could be a lightweight web page using D3.js or similar, reading the JSON directly — entirely independent of the C++ simulation.

#### Extended Usage

- **Population snapshots**: Export entire populations at generation milestones to track macro-evolution
- **Cross-run seeding**: Load champion boids from run A into run B's initial population to test robustness
- **Comparative analysis**: Diff two boid JSONs to see exactly how genomes diverged (which connections were added/removed, how plasticity parameters shifted)
- **Reproducibility**: Archive initial population JSON + RNG seed = fully reproducible evolutionary run

---

## Sources

- [PixiJS v8 Launch Announcement](https://pixijs.com/blog/pixi-v8-launches)
- [WebGPU Browser Support](https://web.dev/blog/webgpu-supported-major-browsers)
- [Canvas vs WebGL Performance](https://digitaladblog.com/2025/05/21/comparing-canvas-vs-webgl-for-javascript-chart-performance/)
- [Broad Phase Collision Detection](http://buildnewgames.com/broad-phase-collision-detection/)
- [Quadtree vs Spatial Hashing](https://zufallsgenerator.github.io/2014/01/26/visually-comparing-algorithms)
- [Web Workers MDN](https://developer.mozilla.org/en-US/docs/Web/API/Web_Workers_API)
- [OffscreenCanvas Guide](https://macarthur.me/posts/animate-canvas-in-a-worker/)
- [GPU Particle Systems](https://dev.to/hexshift/building-a-custom-gpu-accelerated-particle-system-with-webgl-and-glsl-shaders-25d2)
- [AgentScript ABM Framework](http://agentscript.org/)
- [BoidsJS Implementation](https://ercang.github.io/boids-js/)
- [WebGL Boids (GPGPU)](https://github.com/markdaws/webgl-boid)
- [Getting started with metal-cpp](https://developer.apple.com/metal/cpp/)
- [Metal Overview - Apple Developer](https://developer.apple.com/metal/)
- [Metal 4 / What's New in Metal](https://developer.apple.com/metal/whats-new/)
- [M1 GPUs for C++ science: Getting started](https://larsgeb.github.io/2022/04/20/m1-gpu.html)
- [Performance Evaluation of Boids on GPU and CPU](https://www.diva-portal.org/smash/get/diva2:1191916/FULLTEXT01.pdf)
- [GPU Boids at scale](https://toytag.net/posts/boids/)
- [MoltenVK (Vulkan on Metal)](https://github.com/KhronosGroup/MoltenVK)
- [State of Vulkan on Apple, Jan 2026](https://www.lunarg.com/the-state-of-vulkan-on-apple-jan-2026/)
- [Apple Accelerate Framework](https://developer.apple.com/accelerate/)
- [Apple Neural Engine (community documentation)](https://github.com/hollance/neural-engine)
- [Core ML Overview](https://developer.apple.com/machine-learning/core-ml/)
- [Unified Memory on Apple Silicon](https://www.xda-developers.com/apple-silicon-unified-memory/)
- [Apple Silicon M-Series SoCs for HPC](https://arxiv.org/html/2502.05317v1)
- [SpriteKit - Apple Developer](https://developer.apple.com/documentation/spritekit/)
- [Metal Performance Shaders](https://developer.apple.com/documentation/metalperformanceshaders)
- [SDL3 GPU API](https://wiki.libsdl.org/SDL3/CategoryGPU)
- [SDL3 Official Release](https://www.phoronix.com/news/SDL3-Official-Release)
- [SDL2 Maintenance Mode](https://www.phoronix.com/news/SDL2-To-Maintenance-Mode)
- [SFML 3.0 Release](https://www.sfml-dev.org/)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [Dear ImGui Backends](https://github.com/ocornut/imgui/blob/master/docs/BACKENDS.md)
- [ImPlot — GPU-Accelerated Plotting for Dear ImGui](https://github.com/epezent/implot)
- [ImGui-SFML Binding](https://github.com/SFML/imgui-sfml)
- [Raylib](https://www.raylib.com/)
- [Fix Your Timestep! (Gaffer on Games)](https://gafferongames.com/post/fix_your_timestep/)
- [EnTT — Entity Component System](https://github.com/skypjack/entt)
- [Flecs — Fast and Lightweight ECS](https://github.com/SanderMertens/flecs)
- [Component Pattern (Game Programming Patterns)](https://gameprogrammingpatterns.com/component.html)
- [nlohmann/json — JSON for Modern C++](https://github.com/nlohmann/json)
- [D3.js — Data-Driven Documents](https://d3js.org/)
