# Claude Code Conversation

*Source: `/Users/danolner/.claude/projects/-Users-danolner-Code-claude-wildboids/9680ca6d-dc5b-460b-bf14-f0a7bf264b0c.jsonl`*

---


## Human (1)

*[Opened file: /Users/danolner/Code/claude/wildboids/planningdocs/evolution_theory.md]*
At the end of @planningdocs/evolution_theory.md can you add a section with research covering the following. (1) What do we know about the evolution of animal collective movement behaviour (herds, flocking etc)?) and (2) What do we know - or what theories exist - about the genetics and evolutionary mechanisms behind them? E.g. one would be that overall, herding protects the group as a whole (even though some on the outside may be more vulnerable) - but how exactly does that get selected for and what are the genepool requirements so that protection of group-wide genes is the selection pressure? What other kinds of selection pressure for collective grouping / flocking could there be / exists in the theory? Answer with links to sources where possible.


## Assistant

*[Spawned agent: Research collective movement evolution]*

> **Tool result:** I now have comprehensive research across all the requested topics. Here is the organized summary:  ---  ## Topic 1: Evolution of Animal Collective Movement Behaviour  ### Deep Evolutionary Origins  Co
> **Tool result:** agentId: a7ddda4e133da3a42 (for resuming to continue this agent's work if needed) <usage>total_tokens: 36884 tool_uses: 17 duration_ms: 217268</usage>


## Assistant

Excellent research. Now let me read the end of the file and append the new section.


## Assistant

*[Read: `/Users/danolner/Code/claude/wildboids/planningdocs/evolution_theory.md`]*

> **Tool result:**    360→    361→## Sources    362→    363→- [Craig Reynolds' Boids](https://www.red3d.com/cwr/boids/)    364→- [Boids Wikipedia](https://en.wikipedia.org/wiki/Boids)    365→- [NEAT Algorithm Explained]...


## Assistant

*[Edit: `/Users/danolner/Code/claude/wildboids/planningdocs/evolution_theory.md`]*

> **Tool result:** The file /Users/danolner/Code/claude/wildboids/planningdocs/evolution_theory.md has been updated successfully.


## Assistant

Now let me add the new sources to the sources list.


## Assistant

*[Edit: `/Users/danolner/Code/claude/wildboids/planningdocs/evolution_theory.md`]*

> **Tool result:** The file /Users/danolner/Code/claude/wildboids/planningdocs/evolution_theory.md has been updated successfully.


## Assistant

Done. I've added a new **Part 3: The Biology — Evolution of Collective Movement in Animals** section covering:

**Why collective movement evolved:**
- Anti-predator benefits (selfish herd, dilution, confusion, many-eyes) as the primary driver
- Energy savings (fish in schools use 53% less energy)
- Enhanced foraging via information sharing
- Self-organization from simple rules

**The edge paradox** — how herding gets selected for despite edge individuals paying a cost. The resolution: selection acts on *movement rules* (aggregate vs. don't), not positions. Even edge individuals do better than loners.

**Group vs. individual selection** — the Williams/Pinker individual-selection view vs. Wilson's multilevel selection, with current consensus favoring individual selection as sufficient.

**Actual genetics** — specific genes identified in zebrafish (scn1lab, disc1), sticklebacks (Eda gene + lateral line), and guppies (high heritability, neuron migration genes). The zebrafish finding is particularly striking: just two visual reflexes produce collective behavior.

**Relevance to Wild Boids** — key takeaways including: predation pressure is essential for evolving flocking, simple sensory rules suffice (validated by zebrafish genetics), sensory system evolution is biologically realistic, and the "selfish herders finish last" result suggests cooperative movement rules may outperform pure selfish positioning in mobile groups.

20 new sources added to the bibliography.
