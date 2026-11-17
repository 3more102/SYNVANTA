# SYNVANTA AIG backend — initial subset

## Representation

- AIG literal `2 * node_id` references a node; bit zero complements the edge.
- Literal `0` is constant false and literal `1` is constant true.
- Nodes are constant-false, primary-input, or two-input AND nodes.
- OR, XOR, XNOR, mux, and inversion are represented using AND nodes and complemented edges.
- AND nodes are structurally hashed using canonical operand order.

## Initial lowering support

- Input ports are split into LSB-indexed input bits.
- Known two-state constants are accepted up to 64 bits.
- Resize supports truncation, zero extension, and sign extension from the source IR type.
- Unary plus, negation, bitwise-not, logical-not, and reduction NAND/NOR/XNOR are supported.
- Add/subtract use a ripple-carry Boolean construction.
- Bitwise/logical operations, equality and relational comparisons, muxes, and logical shifts are supported.
- Output port bits are connected to the corresponding lowered assignment values.

## Explicit limitations

- Multiply, divide, and modulo currently produce an unsupported-operation diagnostic.
- X, Z, and wildcard digits are not modeled; the initial AIG is strictly two-state.
- Sequential elements and clock/reset behavior are not yet represented.
- The frontend's SystemVerilog expression-sizing rules remain incomplete; AIG lowering preserves the current RTL IR semantics and does not repair frontend sizing.
- No SAT/equivalence proof, AIG rewriting, balancing, timing optimization, or technology mapping is performed.

## CLI

```bash
synvanta aig examples/half_adder.sv
```

The textual dump lists each AIG node and each module output bit. A complemented literal is prefixed with `!`.
