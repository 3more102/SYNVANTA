# SYNVANTA Technology-Independent RTL IR (Initial)

## Purpose

The initial RTL IR is the boundary between parsed SystemVerilog syntax and later optimization/lowering stages. It is a deterministic, technology-independent graph of typed operations. It is not a gate-level netlist and does not imply that technology mapping has occurred.

## Core records

- RtlModule: module name, port metadata, operation nodes, and assignment records.
- RtlPort: name, direction, width, and signedness.
- RtlNode: stable module-local ValueId, operation, result width, signedness, operands, optional literal/name, and source location.
- RtlAssignment: destination signal, value ID, destination width, and destination signedness.

## Initial operations

- Sources: input, const.
- Type conversion: resize.
- Unary: positive, negate, bitwise-not, logical-not, reduction NAND/NOR/XNOR.
- Arithmetic: add, subtract, multiply, divide, modulo.
- Bitwise/logical: AND, OR, XOR, XNOR, logical AND/OR.
- Comparisons: equal, not-equal, less/less-equal/greater/greater-equal.
- Shifts: left and right logical shift.
- Selection: three-input mux.

## Initial width rules

- Scalar ports and nets are one bit; packed ranges use abs(left-right)+1 bits.
- Sized literals preserve their declared width; unsized decimal literals are represented as signed 32-bit values.
- Unary arithmetic/bitwise operators preserve operand width. Logical-not and reduction operators produce one bit.
- Arithmetic and bitwise binary operators use the maximum operand width. Their signed result is marked signed only when both operands are signed.
- Shifts preserve the left operand width and signedness.
- Comparisons and logical binary operators produce one bit.
- Conditional expressions use the maximum width of the two data branches.
- Assignment conversions are explicit resize nodes when width or signedness differs from the destination.

These are initial implementation rules, not a claim of full IEEE 1800 expression-sizing conformance. Do not use this IR for signoff until sizing, casting, and four-state semantics are expanded and validated.

## Lowering behavior

- Input ports become source nodes.
- Continuous assignments become assignment records and expression nodes.
- Internal nets are resolved through their continuous drivers.
- Undriven references and combinational dependency cycles are reported as errors.
- Inout ports are rejected by this initial lowering stage.
- Node IDs are local to each module and assigned in deterministic creation order.

## Optimization contract

The RTL optimizer validates topological node references, applies local identities, performs structural hashing, and removes nodes that cannot reach an observable output assignment. Internal continuous-net dependencies are already resolved into the value graph by the lowerer, so intermediate internal assignment records may be removed after optimization.

The optimizer currently handles common-subexpression elimination, dead-code elimination, redundant resize/unary-plus nodes, duplicate bitwise operands, and muxes with identical data inputs. General constant propagation and arithmetic folding are not yet implemented.

## CLI

- synvanta ir examples/half_adder.sv — dump the lowered RTL IR.
- synvanta opt examples/half_adder.sv — optimize the RTL IR, print pass statistics, and dump the result.

The textual dump is intended for debugging and regression tests. A stable machine-readable serialization format will be defined separately before external optimization passes depend on it.
