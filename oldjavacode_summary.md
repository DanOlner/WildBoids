# Wild Boids - Original Java Code Summary

**Created:** May 2007

**Author:** Claude Code

**Prompt:** "Read the CLAUDE.md file and then analyse the contents of the orig_java folder and its java scripts, where I carried out the original programming for the wild boids project. In the currently empty file @oldjavacode_summary.md, outline details of each java script in that folder. What does each do, how do they connect, and what does the whole project do?"

## Project Overview

Wild Boids is an evolutionary predator-prey simulation based on Craig Reynolds' classic "boids" flocking algorithm. Unlike traditional boids which follow fixed rules (separation, alignment, cohesion), Wild Boids uses **genetically-encoded sensory rules** that evolve over time through natural selection:

- **Predators** evolve to become better hunters (those that catch prey survive longer)
- **Prey** evolve to become better at escaping (those that avoid being eaten survive to breed)

The simulation runs on a **toroidal world** (wrap-around edges) where boids detect others through multiple configurable **fields of view** and react based on their evolved genetic parameters.

---

## File-by-File Breakdown

### Core Classes

#### 1. `BoidsStartUp.java` (Entry Point & GUI)
**Purpose:** Application entry point with Swing GUI for configuration.

**Key Features:**
- Main method launches the application
- Swing JFrame with controls for:
  - Number of boids (predator/prey pairs)
  - Turning speed (direction momentum)
  - Display speed (animation delay)
  - Three field-of-view radius sliders (max 300 pixels each)
  - Field-of-view visualization selector (0-6)
- Tracks and displays death counts for prey and predators
- Creates and launches `BoidsCage` on a separate thread when START clicked

**Key Parameters:**
- `CAGE_XY`: World dimensions (1000x1000 default)
- `FIELDOFVIEWMAXRADIUS`: Array of 3 FOV sizes `{20, 40, 60}` default
- `boidsNumber`: Number of each type (75 default, so 150 total)
- `turningSpeed`: Controls direction momentum (3 default)

---

#### 2. `Boid.java` (Abstract Base Class)
**Purpose:** Abstract superclass defining shared boid behavior and genetics.

**Key Genetic Parameters (evolvable):**
- `directionMomentum`: How quickly a boid can change direction (0-1)
- `speedInertia`: How speed changes affect movement (0-1)
- `dirRule[6]`: Six DirectionRule objects encoding sensory responses

**Position System:**
- `xyLocation[9]`: Array where `[0]` is real position, `[1-8]` are virtual points for torus wrapping
- Virtual points allow detection across world boundaries

**Movement Logic:**
1. If no boids detected: `repeatPreviousHeading()` - continue current direction
2. If boids detected: `setNewHeading()` - calculate weighted average from all active rules
   - Each rule contributes position-reaction and vector-reaction components
   - Weights normalized, results combined
   - Direction momentum smooths turning

**Key Methods:**
- `setVirtualPoints()`: Creates 8 virtual copies around world edges
- `torus()`: Handles wrap-around when boid leaves world boundary
- `moveFieldsOfView()`: Translates/rotates all FOV polygons to match movement
- `nextMoment()`: Per-tick updates (age increment, etc.)

**Abstract Methods (implemented by subclasses):**
- `isPredator()`: Type identification
- `iAmDinnerForWho(Boid b)`: Predator registration for eating
- `distanceToScoff()`: Check if prey in eating range
- `eat()`: Consume prey

---

#### 3. `PredatorBoid.java` (Predator Implementation)
**Purpose:** Predator boid that hunts prey.

**Unique Behavior:**
- Has `metabolism` (400-700, decreases each tick)
- `digestion` counter (10 ticks after eating - can't eat again)
- Dies and triggers evolution when metabolism reaches 0

**Eating Mechanics:**
- `distanceToScoff()`: Creates circular "mouth" (10px diameter) around position
- Checks if any prey in `foodList` are within mouth range (including virtual points)
- Registers interest with prey via `iAmDinnerForWho()`
- `eat()`: Adds 100 to metabolism, sets digestion to 10

**Evolution Trigger:** Death from starvation (metabolism = 0) creates new baby predator via `Evolution.EvolveNow()`

---

#### 4. `PreyBoid.java` (Prey Implementation)
**Purpose:** Prey boid that tries to survive.

**Unique Behavior:**
- Maintains `whoWantsToEatMe` list of predators that want to eat it
- When eaten, triggers evolution for new prey boid
- Tells first predator in list to `eat()` when consumed

**Evolution Trigger:** Being eaten by a predator creates new baby prey via `Evolution.EvolveNow()`

---

#### 5. `DirectionRule.java` (Genetic Sensory Rule)
**Purpose:** Encodes one of six sensory rules per boid (3 for detecting prey, 3 for detecting predators).

**Genetic Parameters:**
- `fieldOfView`: 8-vertex GeneralPath polygon defining detection area
- `pathGenes[8]`: Point2D array storing the FOV vertices for inheritance
- `speed`: Response speed to detected boids (0-10)
- `ruleweight`: Importance of this rule vs others (0-1)
- `relativeAngle`: Direction to move relative to detected boids (0-2π)
- `distanceImportance`: Weight based on distance (0-2)

**Vector Reaction Parameters (response to detected boid movement):**
- `vectorspeed`: Reaction to detected boids' speed (-1 to 1)
- `vectorruleweight`: Importance of vector reaction
- `vectorrelativeAngle`: Direction relative to detected boids' heading

**Key Methods:**
- `randomise()`: Generates random genes at start of evolution
- `boidsInView(Boid)`: Adds detected boid to list
- `avPosDistAngle(Point2D)`: Returns array with:
  - `[0]`: Average distance to detected boids
  - `[1]`: Angle to average position
  - `[2,3]`: Average x,y coordinates
  - `[4]`: Average direction of detected boids
  - `[5]`: Average speed of detected boids

---

#### 6. `BoidsCage.java` (Simulation Engine)
**Purpose:** Main simulation loop and rendering.

**Initialization:**
- Creates equal numbers of `PreyBoid` and `PredatorBoid`
- Sets up `Buffer` for edge detection
- Creates `Evolution` instance
- Launches JFrame with `DrawPanel`

**Main Loop (`go()`):**
For each tick:
1. **Detection Phase:** For each boid, check all 6 FOV rules against all other boids
   - Uses buffer zones (RectA-D) to optimize virtual point checking
   - Only checks relevant virtual points based on boid's edge position
2. **Movement Phase:** Each boid either:
   - Continues straight if no boids detected
   - Calculates new heading from detected boids
3. **Predator Eating:** PredatorBoids check `distanceToScoff()` if digestion=0
4. **End-of-tick:** Call `nextMoment()` on all boids (age, metabolism, death checks)
5. **Render:** Repaint panel, sleep for display speed

**DrawPanel (inner class):**
- Draws arrow-shaped boids (green=prey, red=predator)
- Optionally draws selected field-of-view polygon
- Blue circle around boid 0 for debugging

---

#### 7. `Evolution.java` (Genetic Algorithm)
**Purpose:** Handles breeding and gene inheritance when boids die.

**Selection Algorithm:**
1. Collect all living boids of same type as deceased
2. Sort by age (oldest = most fit = survived longest)
3. Select two parents using **cube-root bias**: `index = (random * cbrt(n))³`
   - Heavily favors older boids but doesn't exclude young ones

**Crossover:**
For each gene, randomly (50/50) select from parent 1 or parent 2:
- `directionMomentum`
- `speedInertia`
- For each of 6 DirectionRules:
  - `relativeAngle`, `ruleweight`, `speed`
  - `vectorrelativeAngle`, `vectorruleweight`, `vectorspeed`
  - All 8 `pathGenes` points (FOV shape vertices)

**Finalization:**
- Builds GeneralPath FOV shapes from inherited pathGenes
- Transforms to baby boid's initial position/rotation
- Replaces dead boid in array with new baby

**Note:** Mutation is mentioned but not implemented ("lets leave mutation for now")

---

### Utility Classes

#### 8. `Utils.java`
**Purpose:** Shared utilities.
- Contains static `Random` instance with seed=1 for reproducibility

---

#### 9. `AngleFromTwoPoints.java`
**Purpose:** Trigonometric utility for angle calculations.

**Methods:**
- `getAngleFromPoints(Point2D, Point2D)`: Angle between two points
- `getAngleFromXY(double, double)`: Angle from x,y offset
- Handles quadrant adjustments for `atan()` limitations

---

#### 10. `Buffer.java`
**Purpose:** Edge detection optimization.

Creates four rectangles along world edges (width = max FOV radius):
- `RectA`: Top edge
- `RectB`: Left edge
- `RectC`: Bottom edge
- `RectD`: Right edge

Used by BoidsCage to determine which virtual points need checking.

---

#### 11. `GraveYard.java`
**Purpose:** Store recently deceased boids (for death marker display).
- `deadBoids` ArrayList
- Not fully integrated in current code

---

#### 12. `SortArray.java`
**Purpose:** Bubble sort utilities.
- `SortIt(int[])`: Sort integer array
- `SortTheBoids(ArrayList<Boid>)`: Sort boids by age (descending)

---

#### 13. `Bits.java`
**Purpose:** Development scratch file with commented-out test code.
- Contains various geometry/trig tests
- Rotation experiments
- Path manipulation tests
- Not used in production

---

## Class Relationships

```
BoidsStartUp (GUI/Entry)
     │
     ▼
BoidsCage (Simulation Engine)
     │
     ├──► Boid[] (array of all boids)
     │         │
     │         ├──► PreyBoid extends Boid
     │         │
     │         └──► PredatorBoid extends Boid
     │                   │
     │                   └──► DirectionRule[6] (sensory genes)
     │
     ├──► Evolution (breeding algorithm)
     │
     ├──► Buffer (edge detection zones)
     │
     └──► DrawPanel (rendering)

Utilities: Utils, AngleFromTwoPoints, SortArray
Unused: GraveYard, Bits
```

---

## Simulation Flow Summary

1. **Startup:** User configures parameters, clicks START
2. **Initialization:** Create N prey + N predators with random genes
3. **Each Tick:**
   - Detect: Each boid's 6 FOV polygons check for other boids
   - React: Calculate weighted heading from all detecting rules
   - Move: Update position with momentum smoothing
   - Eat: Predators attempt to catch nearby prey
   - Age: Increment ages, decrement predator metabolism
   - Death/Birth: Dead boids replaced with offspring of fittest survivors
4. **Evolution emerges:** Over time, successful genes propagate

---

## Key Design Concepts

1. **Toroidal World:** Boids wrap around edges; virtual points enable cross-boundary detection

2. **Multi-Rule Sensing:** Each boid has 6 independent FOV rules (3 for own kind, 3 for other kind) with different sizes and responses

3. **Weighted Averaging:** Multiple detected rules contribute to final heading based on their weights

4. **Fitness = Survival:** No explicit fitness function - longest survivors breed most

5. **Position + Vector Reactions:** Boids can react to both WHERE others are AND which direction they're moving

6. **Genetic Encoding:** FOV shapes stored as 8-point polygons that can be inherited and combined
