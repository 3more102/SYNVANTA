# SYNVANTA AIG representation — 0.2

## Representation

The AIG contains a single constant-zero node, primary-input nodes, and two-input AND nodes. Every edge is an `AigLit` encoded as:

```text
literal = (node_id << 1) | complemented
```

Literal `0` is constant false; literal `1` is constant true. For a non-constant node, the low bit selects the positive or complemented phase. NOT therefore does not create a node.

## Structural hashing and simplification

AND nodes are canonicalized by sorting their two input literals and looking up the pair in a structural hash table. The constructor also applies:

- `0 & x = 0`
- `1 & x = x`
- `x & x = x`
- `x & !x = 0`

OR, XOR, XNOR, and mux functions are composed from AND nodes and complemented edges. The resulting graph is deterministic for a fixed RTL IR node order.

## Lowered operations

The initial AIG lowerer supports:

- input and constant sources;
- resize / zero-extension / sign-extension;
- unary plus, negation, bitwise NOT, logical NOT, and reduction NAND/NOR/XNOR;
- ripple-carry addition and subtraction;
- truncated shift-add multiplication;
- bitwise and logical AND/OR/XOR/XNOR;
- equality and signed/unsigned magnitude comparisons;
- logical left/right barrel shifts;
- 2:1 muxes.

Division and modulo are intentionally diagnosed as unsupported rather than approximated.

## Validation

The C++ regression suite evaluates the AIG directly against reference functions for:

- all half-adder input combinations;
- all 4-bit addition and subtraction pairs;
- all 4-bit mux input pairs and both select values;
- all signed 4-bit comparison pairs;
- 4-bit shift and multiplication combinations;
- structural hashing and constant simplification.

This is reference-functional testing of the AIG, not a formal proof for arbitrary SystemVerilog or a comparison against an external simulator.

## Not yet included

The current graph has no AIG balancing, rewriting/refactoring, sequential elements, latch/clock modelling, observability cleanup, SAT-based equivalence, Liberty mapping, timing analysis, or physical estimation. Those are separate roadmap stages.
