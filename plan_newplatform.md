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
