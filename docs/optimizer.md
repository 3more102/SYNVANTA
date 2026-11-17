# Initial RTL Optimizer

## Pass sequence

1. Constant folding: evaluate supported operations when all operands are known constants.
2. Constant-condition mux simplification: replace a mux with its selected branch when the condition is known.
3. Common-subexpression elimination (CSE): merge structurally identical pure operations. Commutative operations use canonical operand order.
4. Dead-code elimination: retain only nodes reachable from observable output assignments. If a module has no output ports, retain all assignment roots.

## Supported constant evaluation

- Unary plus, negate, bitwise-not, logical-not, reduction NAND/NOR/XNOR.
- Add, subtract, multiply, divide, modulo, bitwise operations, logical operations, comparisons, logical shifts, and constant-condition muxes.
- Explicit resize nodes, including signed extension when the source node is signed.
- Integer constants with known values and widths up to 64 bits.

Operations are left intact when a value is unknown, contains X/Z/? digits, exceeds the supported 64-bit evaluator width, or would require unsafe evaluation such as division by zero or signed minimum divided by negative one.

## Local rewrites

- Remove unary plus and no-op resize nodes.
- Replace muxes with identical true/false data inputs by that data input.
- Do not apply x AND x, x XOR x, or similar two-state identities to logic inputs: four-state X/Z behavior has not been eliminated from the IR.
- Do not fold negative signed shift amounts; their result is unknown in SystemVerilog.

## Safety properties

- Optimization is performed on a copy of the input design; the original IR is not mutated.
- Node and assignment references are remapped after CSE and dead-node compaction.
- The optimizer does not claim formal equivalence. Every new rewrite needs regression coverage and, later, equivalence checking.
- These are RTL-level cleanup passes, not AIG rewriting or standard-cell technology mapping.

## CLI

```bash
synvanta optimize examples/half_adder.sv
```

The command prints constant-fold, mux-simplification, local simplification, CSE, dead-node, and removed-assignment counts together with the resulting IR.
