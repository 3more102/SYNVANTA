# SYNVANTA RTL optimization pass 0.1

## Scope

The first RTL optimizer operates on the technology-independent RtlDesign. It does not perform technology mapping and does not rewrite the source AST.

## Pass order

1. Validate module-local node IDs and topological operand references.
2. Apply local identity reductions.
3. Perform structural hashing / common-subexpression elimination.
4. Identify externally observable output assignments.
5. Perform dead-code elimination from output roots.
6. Compact node IDs and remap operands and assignments.

## Initial rewrites

- Unary plus of a value becomes the value.
- A resize that does not change width or signedness becomes the source value.
- x AND x and x OR x become x.
- x XOR x becomes a zero constant of the result width.
- x XNOR x becomes an all-ones constant of the result width.
- A mux with identical true and false data inputs becomes that data input.
- Commutative operations use canonical operand ordering for structural hashing.

## Observability

When a module has output ports, only assignments to those output ports are roots for dead-code elimination. The current RTL lowerer resolves internal continuous-net dependencies into the value graph, so nodes feeding an output remain reachable even when the intermediate net assignment record is removed.

For modules without output ports, all assignments are retained as roots.

## Statistics

Each module reports node counts before optimization, after structural hashing, and after dead-code elimination, along with CSE merges, local simplifications, removed dead nodes, and assignment counts.

## Limitations

This pass does not yet perform general constant propagation, arithmetic constant folding, Boolean rewriting across multiple levels, AIG conversion, SAT-based equivalence checking, or timing/area optimization. Those remain separate roadmap items. The pass rejects malformed or non-topological RTL graphs rather than attempting to repair them.
