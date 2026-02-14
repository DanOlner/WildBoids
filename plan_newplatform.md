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
