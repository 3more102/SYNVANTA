# SYNVANTA Technology-Independent RTL IR (Initial)

## Purpose

The initial RTL IR is the boundary between parsed SystemVerilog syntax and later optimization/lowering stages. It is a deterministic, technology-independent graph of typed operations. It is not a gate-level netlist and does not imply that technology mapping has occurred.

## Core records

- `RtlModule`: module name, port metadata, operation nodes, and assignment records.
- `RtlPort`: name, direction, width, and signedness.
- `RtlNode`: stable module-local `ValueId`, operation, result width, signedness, operands, optional literal/name, and source location.
- `RtlAssignment`: destination signal, value ID, destination width, and destination signedness.

## Initial operations

- Sources: `input`, `const`.
- Type conversion: `resize`.
- Unary: positive, negate, bitwise-not, logical-not, reduction NAND/NOR/XNOR.
- Arithmetic: add, subtract, multiply, divide, modulo.
- Bitwise/logical: AND, OR, XOR, XNOR, logical AND/OR.
- Comparisons: equal, not-equal, less/less-equal/greater/greater-equal.
- Shifts: left and right logical shift.
- Selection: three-input mux.

## Initial width rules

- Scalar ports and nets are one bit; packed `[left:right]` declarations use `abs(left-right)+1` bits.
- Sized literals preserve their declared width; unsized decimal literals are currently represented as signed 32-bit values.
- Unary arithmetic/bitwise operators preserve operand width. Logical-not and reduction operators produce one bit.
- Arithmetic and bitwise binary operators use the maximum operand width. Their signed result is marked signed only when both operands are signed.
- Shifts preserve the left operand width and signedness.
- Comparisons and logical binary operators produce one bit.
- Conditional expressions use the maximum width of the two data branches.
- Assignment conversions are explicit `resize` nodes when width or signedness differs from the destination.

These are initial implementation rules, not a claim of full IEEE 1800 expression-sizing conformance. Do not use this IR for signoff until sizing, casting, and four-state semantics are expanded and validated.

## Lowering behavior

- Input ports become source nodes.
- Continuous assignments become assignment records and expression nodes.
- Internal nets are resolved through their continuous drivers.
- Multi-driver wire nets are accepted by frontend semantic checks but rejected by the initial RTL lowerer with `LOW0009`, because this IR has no net-resolution model; no driver is silently discarded.
- Undriven references, undriven output ports, and combinational dependency cycles are reported as errors.
- Inout ports are rejected by this initial lowering stage.
- Node IDs are local to each module and assigned in deterministic creation order.

## CLI

```bash
synvanta ir examples/half_adder.sv
```

The textual dump is intended for debugging and regression tests. The initial optimizer consumes this in-memory IR and performs constant folding, common-subexpression elimination, and dead-node removal. A stable machine-readable serialization format will be defined separately before external tools depend on it.


The RTL lowerer rejects unbased unsized fill literals ('0 and '1) until context-determined width propagation is implemented. It also leaves constant shifts with a negative signed shift amount unfolded rather than replacing their unknown result with zero.
