# Design specifications

These documents define behavior that should survive implementation rewrites.

- [Expression performance](expression-performance.md)
- [Procedural motion](procedural-motion.md)
- [Multi-session dialogue](session-dialogue.md)
- [Presence contract](presence-contract.md)
- [Body capabilities](body-capabilities.md)
- [Native presentation and graphics stack](native-presentation.md)

## Specification template

A new design should answer:

1. What user-visible problem exists?
2. What are the goals and explicit non-goals?
3. Which component owns each piece of state?
4. What data crosses each boundary, and at what cadence?
5. Which invariants prevent known regressions?
6. How does failure degrade safely?
7. What proves the implementation is acceptable?
8. Which decisions remain open?

Specifications are not chronological logs. Record the settled reason for a decision, not every path
taken to reach it.
