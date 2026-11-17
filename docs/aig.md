# SYNVANTA AIG Backend

## Purpose

The AIG backend is the first independently implemented Boolean-network representation in SYNVANTA. It bit-blasts the supported combinational RTL IR into an And-Inverter Graph (AIG) that can become the input to later rewriting and standard-cell mapping stages.

The AIG is an internal synthesis representation. It is **not** a Liberty-mapped netlist and does not by itself provide area, delay, or power results.

## Literal representation

An AIG literal is a 32-bit integer:

- Node index: literal shifted right by one bit.
- Complement flag: bit zero.
- Literal 0: constant false.
- Literal 1: constant true.
- For node index n, the positive literal is n << 1 and the complemented literal is (n << 1) | 1.

Each module starts with node zero as constant false. Primary-input nodes and two-input AND nodes are appended in topological order. OR, XOR, XNOR, and mux functions are represented using AND nodes and complemented edges.

## Implemented construction

The builder performs:

- Bit-blasting of vector inputs and outputs.
- Explicit resize handling, including sign extension from the source RTL node.
- Structural hashing of commutative AND nodes.
- Constant and complemented-edge simplifications during node construction.
- Ripple-carry addition and two's-complement subtraction.
- Truncated shift-and-add multiplication.
- Equality and signed/unsigned relational comparisons.
- Bitwise and logical operations, reductions, muxes, and logical shifts.
- Deterministic AIG text dumps for inspection.

## Supported RTL operations

The current builder supports the operations represented by the initial continuous-assignment RTL IR: input, known constant, resize, unary plus, negate, bitwise/logical NOT, reduction NAND/NOR/XNOR, add, subtract, multiply, bitwise AND/OR/XOR/XNOR, logical AND/OR, equality/inequality, relational comparisons, left/right logical shifts, and conditional mux.

## Explicit limitations

- Division and modulo are rejected with diagnostic AIG0006.
- Four-state constants containing X, Z, or ? are rejected. The AIG is a two-state representation and does not guess an unknown value.
- Numeric constants wider than 64 bits are not supported by this first implementation.
- Procedural blocks, registers, latches, memories, module instances, and sequential boundaries are not yet represented.
- The RTL IR still has a deliberately limited SystemVerilog sizing model; AIG correctness is relative to that IR and does not imply complete IEEE SystemVerilog semantic coverage.
- No Liberty cell mapping, SDC timing analysis, buffering, or PPA estimation is performed.

## CLI

Build the project and run:

    synvanta aig examples/half_adder.sv

The command runs parsing, semantic analysis, RTL lowering, RTL optimization, and AIG construction, then prints the resulting graph and input/AND/output counts. A nonzero exit status indicates a parse, lowering, optimization, or AIG construction error.

## Validation

The AIG tests exhaustively compare generated networks against expected truth tables for a half adder, a four-bit adder, signed four-bit comparison, a four-bit mux, and a three-bit multiplier. Additional tests cover fill literals, structural hashing, and explicit rejection of division and unknown constants.
