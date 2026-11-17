# Initial RTL Optimizer

## Pass sequence

1. Validate module-local node IDs, operation arity, and topological operand references.
2. Constant folding: evaluate supported operations when all operands are known constants.
3. Common-subexpression elimination (CSE): merge structurally identical pure operations. Commutative operations use canonical operand order.
4. Observability filtering: when output ports exist, retain assignment records for those output ports only. Internal continuous-net dependencies have already been resolved into the value graph by the RTL lowerer.
5. Dead-node removal: retain only nodes reachable from observable output assignment roots. For modules without output ports, all assignment records remain roots.

## Supported constant evaluation

- Unary plus, negate, bitwise-not, logical-not, reduction NAND/NOR/XNOR.
- Add, subtract, multiply, divide, modulo, bitwise operations, logical operations, comparisons, logical shifts, and constant-condition muxes.
- Explicit resize nodes, including signed extension when the source node is signed.
- Unbased unsized literal context fill: a resize from '1 fills the destination width with ones; '0 fills with zeros.
- Integer constants with known values and widths up to 64 bits.

Operations are left intact when a value is unknown, contains X/Z/? digits, exceeds the supported 64-bit evaluator width, or would require unsafe evaluation such as division by zero or signed minimum divided by negative one.

## Safety properties

- Optimization is performed on a copy of the input design; the original IR is not mutated.
- Node IDs, operation arity, topological operands, and assignment value references are validated before graph indexing.
- Node and assignment references are remapped after CSE and dead-node compaction.
- The optimizer does not claim formal equivalence. Every new rewrite needs regression coverage and, later, equivalence checking.
- These are RTL-level cleanup passes, not AIG rewriting or standard-cell technology mapping.

## CLI

    synvanta optimize examples/half_adder.sv

The command prints folded/CSE/dead-node/assignment counts and the resulting IR.
