# Java to C++: Mental Model Shifts

A practical guide for a Java developer moving to C++, written in the context of a boid simulation with NEAT neural networks, evolutionary algorithms, and classes like `SensorySystem`, `ProcessingNetwork`, `ThrusterArray`, `Boid`, `RigidBody`, `Genome`.

This is not a syntax guide. It's about where your Java instincts will silently betray you.

---

## 1. Mental Model Differences

### 1.1 Memory Management: You Are the Garbage Collector

In Java, you `new` objects freely and the GC cleans up. In C++, every allocation is your responsibility. The primary tool is **RAII** (Resource Acquisition Is Initialization): objects clean up after themselves in their destructors, and destructors run deterministically when scope ends.

```cpp
void simulate() {
    std::vector<float> inputs(sensor_count);  // allocated here
    // ... use inputs ...
}  // destroyed here, memory freed — no GC, no finalize(), guaranteed
```

Smart pointers replace raw `new`/`delete`:

- **`std::unique_ptr<T>`** — sole ownership, zero overhead. This is your default. The `Boid` class owns its `SensorySystem`, `ProcessingNetwork`, and `ThrusterArray` via `unique_ptr`.
- **`std::shared_ptr<T>`** — reference-counted shared ownership. Use sparingly — it has overhead and creates ambiguous ownership graphs. Legitimate use: a `World` object shared among multiple subsystems where no single owner makes sense.
- **Raw pointers (`T*`)** — non-owning observers. Fine for "I need to look at this but someone else owns it." The `perceive(const World& world, const Boid& self, ...)` signature uses references for this purpose.

**Java instinct to fight:** Stop thinking "I'll just `new` it." Ask instead: "Who owns this? When should it die?" If the answer is "the enclosing scope," put it on the stack. If it's "this object owns it for its lifetime," use `unique_ptr`. Only reach for `shared_ptr` if ownership is genuinely shared.

### 1.2 Value Semantics: Objects Live on the Stack by Default

This is the single biggest mental shift. In Java, `Boid b` is a reference (pointer) to a heap object. In C++, `Boid b` **is** the object — allocated on the stack, copied by value when passed to functions.

```cpp
RigidBody body;           // This IS a RigidBody, on the stack. Not a pointer.
body.position = {10, 20}; // Direct access, no heap allocation.

void applyForce(RigidBody b, Vec2 force);  // b is a COPY of the argument
void applyForce(RigidBody& b, Vec2 force); // b is a reference (like Java's default)
```

**Consequences for this project:**
- A `RigidBody` with position, velocity, orientation, angular velocity is ~48 bytes. Perfectly fine as a value member of `Boid` — no pointer, no heap allocation. In Java you'd have `private RigidBody body;` meaning a pointer to a separate heap object. In C++, the `RigidBody` data lives physically inside the `Boid` object.
- A `std::vector<RigidBody>` stores bodies contiguously in memory — superb cache performance for iterating over thousands of boids per frame. In Java, `ArrayList<RigidBody>` stores pointers to scattered heap objects.
- When you pass something to a function, think about whether you want a copy, a reference (`&`), or a const reference (`const&`). Java always passes references to objects; C++ defaults to copying.

**Rule of thumb:** Pass small things (primitives, `Vec2`) by value. Pass larger things by `const&` if you just read them. Pass by `&` if you need to mutate them.

### 1.3 No Universal Base Class

There is no `Object`. There is no default `toString()`, `hashCode()`, or `equals()`. This means:

- You can't put arbitrary objects in a single container. `std::vector<???>` needs a concrete type (or a pointer to a common base).
- You must explicitly define comparison operators, hash functions, and string representations when you need them.
- Polymorphic containers require pointers: `std::vector<std::unique_ptr<SensorySystem>>`, not `std::vector<SensorySystem>`.

### 1.4 Header/Source Split

Java: one file, one class, the compiler figures out dependencies.

C++: declarations go in `.h` (or `.hpp`) files, definitions go in `.cpp` files. Every `.cpp` file that needs to know about `Boid` must `#include "Boid.h"`. This is not just convention — it's how the compilation model works.

```
// SensorySystem.h — declaration
class SensorySystem {
public:
    virtual ~SensorySystem() = default;
    virtual int inputCount() const = 0;
    virtual void perceive(const World& world, const Boid& self,
                          float* outputs) const = 0;
};

// SectorDensitySensor.cpp — definition
#include "SensorySystem.h"
#include "World.h"
#include "Boid.h"

void SectorDensitySensor::perceive(const World& world, const Boid& self,
                                    float* outputs) const {
    // implementation
}
```

**Forward declarations** reduce compile-time coupling. If `Boid.h` only uses a pointer or reference to `World`, it doesn't need `#include "World.h"` — just `class World;` at the top. This matters because changing `World.h` won't force a recompile of everything that includes `Boid.h`.

### 1.5 Compilation Model: Translation Units, Not Classpaths

Java: `javac` sees your whole classpath, resolves everything. You think in terms of packages and imports.

C++: each `.cpp` file is compiled independently into an object file (`.o`). The compiler has no idea what's in other `.cpp` files. The **linker** then stitches object files together. This means:

- You can get "compiles fine, fails to link" errors — the declaration existed (header was included) but the definition wasn't compiled or linked.
- Order of includes matters. Circular dependencies are your problem to solve (via forward declarations and careful header design).
- There's no classpath. You tell the build system where headers and libraries are.

### 1.6 Templates vs Generics

Java generics are type-erased at runtime — `List<Genome>` and `List<Boid>` are the same class at runtime. C++ templates generate separate code for each type at compile time. This means:

- Templates are more powerful: you can specialize on type, use types that don't inherit from anything, do compile-time computation.
- Templates are more painful: error messages are legendary for their opacity, and template code must typically be in headers (the compiler needs to see the full definition to instantiate it).
- There's no `<? extends SensorySystem>` wildcard. Instead, templates just require that the type supports the operations you use on it. C++20 concepts formalize this, but historically it was "if it compiles, it works."

For this project, you likely won't write many templates yourself — the STL containers (`vector`, `unordered_map`, etc.) and algorithms (`std::sort`, `std::transform`) are the main template-using code.

### 1.7 No `interface` Keyword — Abstract Classes Instead

Java's `interface SensorySystem` becomes a C++ class with pure virtual functions:

```cpp
class SensorySystem {
public:
    virtual ~SensorySystem() = default;          // MUST have virtual destructor
    virtual int inputCount() const = 0;           // = 0 means pure virtual
    virtual void perceive(const World& world,
                          const Boid& self,
                          float* outputs) const = 0;
};
```

The `= 0` is what makes it abstract. You can also have non-pure virtual functions with default implementations in the same class — there's no distinction between "interface" and "abstract class."

### 1.8 Inheritance: Multiple Inheritance and Explicit Virtual Dispatch

Two critical differences from Java:

**Virtual dispatch must be opted into.** By default, C++ method calls are resolved at compile time (static dispatch). You must mark methods `virtual` for polymorphism to work:

```cpp
class Base {
public:
    void update();          // NOT virtual — calling through Base* always calls Base::update
    virtual void tick();    // virtual — calling through Base* dispatches to derived class
};
```

If you forget `virtual`, your polymorphism silently doesn't work. Use `override` in derived classes to catch this at compile time:

```cpp
class Derived : public Base {
public:
    void tick() override;   // Compiler error if Base::tick isn't virtual
};
```

**Multiple inheritance exists.** A class can inherit from multiple base classes. This is occasionally useful (e.g., a `Serializable` mixin alongside `SensorySystem`) but introduces complexity (the diamond problem). Java's multiple interface implementation is the safe subset of this.

### 1.9 No Runtime Reflection

You cannot ask an object what class it is, enumerate its fields, or call methods by name at runtime (not without building that machinery yourself). `dynamic_cast` exists for checked downcasting, but there's nothing like Java's reflection API.

**Impact on this project:** No generic serialization. If you want to save/load `Genome` objects, you write explicit serialization code. No dependency injection frameworks that auto-wire by type. No annotation-driven anything.

### 1.10 Const Correctness as a Design Tool

`const` in C++ is a contract enforced by the compiler. It's not like Java's `final` (which only prevents reassignment of the variable, not mutation of the object).

```cpp
class SensorySystem {
public:
    // This method promises not to modify the SensorySystem
    virtual void perceive(const World& world, const Boid& self,
                          float* outputs) const = 0;
    //                    ^^^^^                 ^^^^^
    //                    won't modify World    won't modify this
};
```

Mark everything `const` that should be. `perceive` doesn't modify the sensor system or the world — the compiler will enforce this. This catches bugs and communicates intent. Java developers tend to under-use `const` because they've never had it.

**Cascading effect:** If you call a method on a `const` reference, that method must itself be `const`. This "const infection" forces you to think clearly about what mutates and what doesn't — painful at first, invaluable later.

### 1.11 Move Semantics

In Java, assigning one object reference to another just copies the pointer. In C++, assigning one object to another copies all the data. For large objects (a `Genome` with a big neural network topology), this can be expensive.

Move semantics let you transfer ownership of resources instead of copying:

```cpp
std::vector<Genome> nextGeneration;
Genome offspring = crossover(parentA, parentB);
nextGeneration.push_back(std::move(offspring));
// offspring is now in a "moved-from" state — don't use it anymore
// The vector took ownership of offspring's guts without copying
```

`std::move` doesn't move anything — it casts to an rvalue reference, signaling "you may steal my resources." The move constructor/assignment operator does the actual transfer.

For most of this project, the compiler will handle moves automatically (returning objects from functions, inserting into containers). You mainly need to be aware that it exists and that `unique_ptr` can be moved but not copied (which is the whole point of unique ownership).

---

## 2. Build, Compile, Deploy

### 2.1 Build Systems: CMake

Java has Maven/Gradle with standardized project layout and dependency resolution. C++ has CMake, which is the de facto standard but is really a build-system generator — it produces Makefiles, Ninja files, or IDE projects.

A minimal `CMakeLists.txt` for this project:

```cmake
cmake_minimum_required(VERSION 3.20)
project(wildboids)

set(CMAKE_CXX_STANDARD 20)

add_executable(wildboids
    src/main.cpp
    src/Boid.cpp
    src/RigidBody.cpp
    src/SensorySystem.cpp
    src/ProcessingNetwork.cpp
    src/ThrusterArray.cpp
    src/Genome.cpp
    src/World.cpp
)

target_include_directories(wildboids PRIVATE include)
```

You explicitly list sources (or glob them, controversially). There's no `src/main/java` convention. You choose your layout.

### 2.2 Compilation Is Slow, Incremental Builds Matter

A full rebuild of a large C++ project can take minutes to hours. Java compilation is fast by comparison. This is why:

- **Forward declarations** matter — they reduce header dependencies, meaning fewer files recompile when you change something.
- **Precompiled headers** can speed up builds.
- **Build generators like Ninja** (via CMake) are faster than Make.
- **You develop differently.** You don't rebuild-the-world after every change. You rely on incremental builds and structure headers to minimize cascading recompilation.

#### How This Maps to the Wildboids Architecture

The loose coupling in our design (Sensory → Processing → Thruster, communicating via float arrays) maps directly onto separate compilation units. Each layer gets its own `.h`/`.cpp` pair. When you change the internals of `NeatNetwork::activate()`, only `neat_network.cpp` recompiles. The sensory layer, thruster layer, physics — all stay as already-compiled `.o` files. The linker stitches them together at the end.

A typical edit-compile-test cycle:

1. You're working on the NEAT network
2. You edit `neat_network.cpp`
3. `cmake --build build` recompiles *just that file* (sub-second), then re-links (fast)
4. You run your tests for that module
5. Everything else — sensors, thrusters, physics, evolution — stays compiled

This is why the header/source split (§1.4) and forward declarations (§4.7) matter so much more in C++ than Java's single-file model. A "leaky" header that `#include`s too much creates cascading recompilation — change one thing, rebuild half the project. A clean header that exposes only the interface keeps rebuilds fast.

**Development strategy:** Build each layer as a self-contained unit with its own tests. Once a layer's tests pass and its interface is stable, you rarely recompile it again — it just sits there as a compiled `.o` while you work on other layers. The float-array interfaces mean the layers genuinely don't need to know each other's internals. You could be deep in NEAT implementation work and never recompile the physics or sensor code at all.

### 2.3 No Standard Package Manager

There's no Maven Central. Options:

- **vcpkg** (Microsoft) or **Conan** — closest to a package manager. Can fetch and build dependencies.
- **Git submodules / FetchContent** — pull source code directly into your project tree.
- **Vendoring** — just copy the source into your repo. Common for single-header libraries.
- **System packages** — `apt install libfoo-dev` on Linux, `brew install foo` on macOS.

For this project, NEAT libraries and math libraries can often be vendored or pulled via CMake's `FetchContent`. There's no `pom.xml` equivalent that "just works."

### 2.4 Static vs Dynamic Linking

Java loads `.class` files at runtime. C++ links at build time (static linking produces one big binary) or at load time (dynamic linking uses `.so`/`.dll`/`.dylib` files).

Static linking is simpler for deployment — you get one binary with everything baked in. Dynamic linking shares libraries between programs and allows updating libraries without recompiling. For a simulation like this, static linking is fine and avoids "works on my machine" issues.

### 2.5 Platform-Specific Binaries

There is no JVM. The compiler emits native machine code for a specific OS and architecture. A binary built on macOS ARM doesn't run on Linux x86. Cross-compilation is possible but non-trivial.

---

## 3. Testing

### 3.1 Test Frameworks

The two main choices:

- **Google Test (gtest)** — widely used, xUnit style, good IDE integration.
- **Catch2** — header-only (easy to integrate), BDD-style, no macros for test registration.

```cpp
// Google Test
TEST(GenomeTest, CrossoverPreservesStructure) {
    Genome parentA = createTestGenome(10);
    Genome parentB = createTestGenome(10);
    Genome child = crossover(parentA, parentB);
    EXPECT_EQ(child.nodeCount(), 10);
}

// Catch2
TEST_CASE("Crossover preserves structure") {
    Genome parentA = createTestGenome(10);
    Genome parentB = createTestGenome(10);
    Genome child = crossover(parentA, parentB);
    REQUIRE(child.nodeCount() == 10);
}
```

### 3.2 When a Test Crashes, the Test Runner Dies

In Java, a `NullPointerException` in a test produces a nice stack trace and the next test runs. In C++, a segfault **kills the process**. The test runner is dead. No subsequent tests run. No report is generated.

This is not a minor inconvenience — it fundamentally changes how you develop. You must be more careful about memory errors, and you lean heavily on sanitizers (below) to catch them before they become mysterious crashes.

### 3.3 Sanitizers Are Essential

These are compiler flags that instrument your code to catch memory errors at runtime. **Use them in every debug build and every test run.**

| Sanitizer | Flag | What It Catches |
|-----------|------|-----------------|
| AddressSanitizer (ASan) | `-fsanitize=address` | Buffer overflows, use-after-free, double-free, memory leaks |
| UndefinedBehaviorSanitizer (UBSan) | `-fsanitize=undefined` | Signed overflow, null deref, misaligned access, type punning |
| MemorySanitizer (MSan) | `-fsanitize=memory` | Use of uninitialized memory (Clang only) |
| ThreadSanitizer (TSan) | `-fsanitize=thread` | Data races (relevant if parallelizing boid updates) |

In CMake:

```cmake
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(wildboids PRIVATE -fsanitize=address,undefined)
    target_link_options(wildboids PRIVATE -fsanitize=address,undefined)
endif()
```

ASan + UBSan together are the minimum. Turn them on from day one.

### 3.4 Valgrind

An alternative to sanitizers — runs your program in a virtual machine that tracks every memory operation. Slower (~20x) but catches things sanitizers miss. Use it periodically, not in your inner development loop.

```bash
valgrind --leak-check=full ./wildboids_tests
```

### 3.5 No Guaranteed Exception on Null Pointer

In Java, dereferencing `null` always throws `NullPointerException`. In C++, dereferencing a null pointer is **undefined behavior**. It might crash. It might silently corrupt memory. It might appear to work perfectly in debug builds and crash in release. The compiler is allowed to assume it never happens and optimize accordingly.

This is why sanitizers exist — they turn undefined behavior into reliable, diagnosable crashes.

### 3.6 Debug vs Release Builds Behave Differently

Java bytecode is Java bytecode — optimization happens in the JIT and doesn't change semantics. C++ optimized builds (`-O2`, `-O3`) can expose bugs that debug builds (`-O0`) hide:

- Uninitialized variables may happen to be zero in debug mode (stack is more predictable) but contain garbage in release.
- Undefined behavior may be "optimized" into something catastrophic — the compiler assumes UB can't happen and removes code paths accordingly.
- `assert()` is removed in release builds.

**Always test in both modes.** Run sanitizers in debug. Run performance tests in release. Be suspicious if something only breaks in release — it's usually UB that debug happened to tolerate.

---

## 4. Practical Gotchas for This Project

### 4.1 Object Slicing

If you store polymorphic objects by value, you lose the derived part:

```cpp
// WRONG — slices derived SectorDensitySensor down to base SensorySystem
std::vector<SensorySystem> sensors;         // Stores by value
sensors.push_back(SectorDensitySensor());   // Sliced! Only base portion stored.

// CORRECT — stores polymorphic objects by pointer
std::vector<std::unique_ptr<SensorySystem>> sensors;
sensors.push_back(std::make_unique<SectorDensitySensor>());
```

Your Java instinct to write `List<SensorySystem>` and put derived objects in it will silently truncate objects in C++. Any time you have a polymorphic hierarchy, store pointers.

### 4.2 Virtual Destructors in Base Classes

If `SensorySystem` doesn't have a virtual destructor, deleting a `SectorDensitySensor` through a `SensorySystem*` (which is what `unique_ptr<SensorySystem>` does) is undefined behavior — the derived destructor never runs.

```cpp
class SensorySystem {
public:
    virtual ~SensorySystem() = default;  // REQUIRED for any class used polymorphically
    // ...
};
```

**Rule:** If a class has any virtual methods, give it a virtual destructor. The project's interface classes already do this correctly.

### 4.3 Smart Pointer Choices

For this project's ownership model:

| Relationship | Smart Pointer | Example |
|---|---|---|
| Boid owns its brain | `unique_ptr<ProcessingNetwork>` | Sole owner, transferred on move |
| Population owns its boids | `vector<Boid>` (by value) or `vector<unique_ptr<Boid>>` | If Boid is polymorphic, use unique_ptr |
| Boid observes the world | Raw pointer or reference | `const World&` — doesn't own it |
| Shared config/parameters | `shared_ptr<Config>` or just `const&` | Only shared_ptr if lifetime is ambiguous |

**Java instinct to fight:** Don't reach for `shared_ptr` as your default — it's the closest thing to Java references but it has overhead (atomic reference counting) and obscures ownership. Prefer `unique_ptr` and restructure ownership if needed.

### 4.4 STL Containers and Iterator Invalidation

Modifying a container while iterating over it is undefined behavior in most cases. Adding an element to a `std::vector` can reallocate, invalidating **all** iterators and pointers into it.

```cpp
// DANGEROUS — if push_back reallocates, the loop iterator is invalid
for (auto it = boids.begin(); it != boids.end(); ++it) {
    if (it->shouldReproduce()) {
        boids.push_back(it->reproduce());  // May invalidate it!
    }
}

// SAFE — collect offspring separately, merge after
std::vector<Boid> offspring;
for (auto& boid : boids) {
    if (boid.shouldReproduce()) {
        offspring.push_back(boid.reproduce());
    }
}
boids.insert(boids.end(),
             std::make_move_iterator(offspring.begin()),
             std::make_move_iterator(offspring.end()));
```

This matters directly for the evolutionary loop where you're selecting, culling, and breeding boids.

### 4.5 Undefined Behavior Is Silent

This deserves its own section because it's the most dangerous difference. Java has defined behavior for everything — array bounds checks, null dereferences, integer overflow all produce specific exceptions or defined results.

C++ has a vast space of **undefined behavior** (UB) where the language says "anything can happen." The program may:
- Appear to work perfectly for weeks, then crash in a demo
- Produce correct results in debug and wrong results in release
- Work on your machine and fail on CI
- Corrupt memory in one place and crash in a completely unrelated place later

Common UB in a project like this:
- Buffer overrun when writing sensor outputs to a `float*` array
- Use-after-free if a `Boid` is removed from a container while something else holds a pointer to it
- Signed integer overflow in fitness calculations
- Reading uninitialized memory in a newly allocated neural network

**Defense:** Sanitizers, assertions, `std::vector::at()` (bounds-checked) during development, code review, and a healthy paranoia.

### 4.6 The Rule of Five (or Rule of Zero)

In Java, every class gets sensible default copying (shallow clone) and the GC handles cleanup. In C++, if your class manages resources (raw pointers, file handles, GPU buffers), you may need to define:

1. Destructor
2. Copy constructor
3. Copy assignment operator
4. Move constructor
5. Move assignment operator

If you define any one, you likely need all five. This is the **Rule of Five**.

The better approach is the **Rule of Zero**: don't manage resources directly. Use smart pointers and standard containers, which handle their own resources. Then you need to define none of the five — the compiler-generated defaults just work.

```cpp
// Rule of Zero — no custom destructor/copy/move needed
class Boid {
    RigidBody body;                              // value type, copies fine
    std::unique_ptr<ProcessingNetwork> brain;     // unique_ptr handles ownership
    std::unique_ptr<SensorySystem> sensors;
    std::unique_ptr<ThrusterArray> thrusters;
    Genome genome;                                // assuming Genome follows Rule of Zero too
    float energy;
};
// Boid is movable but not copyable (because unique_ptr isn't copyable)
// This is probably what you want — boids have unique brains
```

**Java instinct to fight:** In Java, you rarely think about copy semantics. In C++, you must decide for every class: is it copyable? Movable? Neither? The answer affects how it can be stored in containers and passed to functions.

### 4.7 Forward Declarations to Manage Compile Dependencies

In Java, `import` has no compile-time cost. In C++, `#include` literally pastes the file contents. Every header you include increases compile time for every file that includes yours.

```cpp
// Boid.h — BEFORE (heavy, slow to compile)
#include "World.h"           // Pulls in everything World needs
#include "SensorySystem.h"
#include "ProcessingNetwork.h"
#include "ThrusterArray.h"

class Boid {
    std::unique_ptr<SensorySystem> sensors;  // Only needs pointer, not full definition
    // ...
    void update(const World& world, float dt);  // Only needs reference
};

// Boid.h — AFTER (light, fast to compile)
#include <memory>
class World;              // Forward declaration — enough for references and pointers
class SensorySystem;
class ProcessingNetwork;
class ThrusterArray;

class Boid {
    std::unique_ptr<SensorySystem> sensors;  // Works with forward declaration
    // ...
    void update(const World& world, float dt);  // Works with forward declaration
};

// Boid.cpp — include the full headers here where you actually use them
#include "Boid.h"
#include "World.h"
#include "SensorySystem.h"
// ...
```

**Note:** `unique_ptr` with a forward-declared type works in the header, but the destructor must be defined in the `.cpp` file where the full type definition is visible. If the compiler-generated destructor is in the header, it can't destroy the `unique_ptr` contents because it doesn't know the type's size. The fix is to declare the destructor in the header and define it (even as `= default`) in the `.cpp`:

```cpp
// Boid.h
class Boid {
public:
    ~Boid();  // Declared, not defined
    // ...
};

// Boid.cpp
#include "Boid.h"
#include "SensorySystem.h"  // Now compiler knows the full type
Boid::~Boid() = default;    // Generated destructor, but here where types are complete
```

---

## Summary: Where Java Instincts Go Wrong

| Java Instinct | C++ Reality |
|---|---|
| `new` everything, GC handles it | Stack allocation by default, `unique_ptr` for heap |
| Objects are always references | Objects are values; you choose value, reference, or pointer |
| `List<Base>` holds derived objects | `vector<Base>` slices; use `vector<unique_ptr<Base>>` |
| `null` dereference = clean exception | Null dereference = undefined behavior, possible silent corruption |
| Everything has `toString()` | No universal base class, no default string representation |
| Reflection-based serialization | Write it by hand |
| One build, runs everywhere (JVM) | Platform-specific binaries, CMake-based build |
| Test failure = exception + next test runs | Test crash = process dead, use sanitizers |
| Refactor freely, IDE resolves everything | Header dependencies matter, forward-declare to decouple |
| Copy is cheap (just a reference) | Copy can be expensive (deep copy), understand move semantics |
| `final` prevents reassignment | `const` prevents mutation, and it's infectious (and useful) |

---

## 5. Development Setup: VS Code on macOS (Apple Silicon)

### 5.1 What You Already Have

Your Mac already has:
- **Apple Clang 17** (`clang++`) — a full C++20 compiler, installed via Xcode Command Line Tools. This is your compiler; you don't need to install gcc.
- **Homebrew** — for installing everything else.

### 5.2 What You Need to Install

**Build tools (via Homebrew):**

```bash
brew install cmake ninja
```

- **CMake** — the build system generator. Reads `CMakeLists.txt`, produces build files.
- **Ninja** — a fast build executor. CMake can generate Makefiles, but Ninja is significantly faster for incremental builds. CMake + Ninja is the standard pairing.

**VS Code extensions (install from the Extensions panel or command line):**

```bash
code --install-extension llvm-vs-code-extensions.vscode-clangd
code --install-extension ms-vscode.cmake-tools
code --install-extension twxs.cmake
```

| Extension | What it does |
|-----------|-------------|
| **clangd** | C++ language intelligence — autocomplete, go-to-definition, errors, refactoring. Far better than Microsoft's C/C++ extension for navigation and diagnostics. Uses the same Clang compiler you're building with. |
| **CMake Tools** | Integrates CMake into VS Code — configure, build, select build type (Debug/Release), pick compiler, run targets, all from the status bar and command palette. |
| **CMake** (twxs) | Syntax highlighting and autocomplete for `CMakeLists.txt` files. |

**Optional but recommended:**

```bash
code --install-extension vadimcn.vscode-lldb
```

| Extension | What it does |
|-----------|-------------|
| **CodeLLDB** | Debugger integration. Set breakpoints, inspect variables, step through code — all in VS Code. Uses LLDB, which is the debugger that ships with Apple's toolchain. |

### 5.3 How It Fits Together

Once installed, the workflow is:

**1. Create a `CMakeLists.txt`** in the project root (we'll do this when we start coding).

**2. CMake Tools auto-detects it.** When you open the folder in VS Code, CMake Tools will prompt you to configure. Select:
- **Kit:** "Clang" (it will find Apple Clang automatically)
- **Build type:** "Debug" (for development; switch to Release for performance testing)
- **Generator:** Ninja (CMake Tools will use it if installed)

**3. The status bar** shows build controls:
- Click "Build" (or `Cmd+Shift+B`) to compile. Only changed files recompile.
- Click the target name to switch between building the main binary, tests, etc.
- Click "Debug" / play button to run with the debugger attached.

**4. clangd provides IntelliSense** by reading the `compile_commands.json` that CMake generates. This gives you:
- Accurate autocomplete (knows your types, your includes, everything)
- Go-to-definition (`Cmd+Click`) — works across headers and source files
- Real-time error squiggles as you type
- Rename symbol, find all references, etc.

To make clangd work, add this to your CMakeLists.txt:

```cmake
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

This generates the `compile_commands.json` that clangd reads. CMake Tools usually handles this automatically, but it's good to have it explicit.

### 5.4 The Edit-Compile-Test Cycle in Practice

```
1.  Edit src/simulation/neat_network.cpp in VS Code
2.  Cmd+Shift+B → Ninja rebuilds just that file (~0.5s)
3.  Run tests:
    - From terminal: cmake --build build --target test
    - Or click the test target in CMake Tools
    - Or use the Testing panel if using CTest integration
4.  If a test segfaults, it will say "signal 11" and stop.
    Re-run with sanitizers enabled (your Debug build should have them on by default).
5.  clangd shows you errors as you type — many bugs caught before you even compile.
```

### 5.5 Recommended VS Code Settings

Add to `.vscode/settings.json` in the project:

```json
{
    "cmake.generator": "Ninja",
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "cmake.configureOnOpen": true,
    "clangd.arguments": [
        "--background-index",
        "--clang-tidy",
        "--header-insertion=iwyu"
    ],
    "editor.formatOnSave": true,
    "C_Cpp.intelliSenseEngine": "disabled"
}
```

Notes:
- `"C_Cpp.intelliSenseEngine": "disabled"` — disables Microsoft's C++ IntelliSense so it doesn't fight with clangd. Only needed if you also have the Microsoft C/C++ extension installed.
- `--clang-tidy` — enables lint warnings in clangd (catches common mistakes).
- `--header-insertion=iwyu` — suggests missing `#include`s automatically.

### 5.6 Debugging

With CodeLLDB installed, add a `.vscode/launch.json`:

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug wildboids",
            "type": "lldb",
            "request": "launch",
            "program": "${workspaceFolder}/build/wildboids",
            "args": [],
            "cwd": "${workspaceFolder}"
        },
        {
            "name": "Debug tests",
            "type": "lldb",
            "request": "launch",
            "program": "${workspaceFolder}/build/wildboids_tests",
            "args": [],
            "cwd": "${workspaceFolder}"
        }
    ]
}
```

Then hit `F5` to run with breakpoints, variable inspection, and step-through — very similar to debugging Java in IntelliJ/Eclipse.

### 5.7 What You Don't Need

- **Xcode** (the full IDE) — not needed. The command line tools you already have are sufficient. Xcode is a 12GB download you can skip.
- **Microsoft C/C++ extension** — clangd is better for code intelligence. Don't install both.
- **gcc/g++** — Apple Clang handles everything. Installing gcc via Homebrew is unnecessary unless you hit a Clang-specific edge case (unlikely).
- **Valgrind** — doesn't run on Apple Silicon. Use sanitizers instead (ASan, UBSan), which are built into Clang and work natively. They're actually better for day-to-day use anyway.

### 5.8 Summary: What to Install

```bash
# Build tools
brew install cmake ninja

# VS Code extensions
code --install-extension llvm-vs-code-extensions.vscode-clangd
code --install-extension ms-vscode.cmake-tools
code --install-extension twxs.cmake
code --install-extension vadimcn.vscode-lldb
```

That's it. After this, open the project folder in VS Code, write a `CMakeLists.txt`, and you're developing C++ with full IDE support.
