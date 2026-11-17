# SYNVANTA Development Roadmap

## Phase 0 — Compiler foundation

- [x] C++20/CMake project skeleton.
- [x] Source locations and structured diagnostics.
- [x] SystemVerilog lexer for the initial subset.
- [x] Module/port/continuous-assignment AST.
- [x] Pratt expression parser.
- [x] CLI check and inspect commands.
- [x] Unit tests and CI workflow.
- [x] Module-local symbol table and basic semantic diagnostics.

## Phase 1 — Language semantics and elaboration

- [x] Module-local symbol table and duplicate/undeclared identifier checks.
- [ ] Constant-expression evaluation and parameter/localparam elaboration.
- [ ] Complete bit widths, signedness, casts, and expression sizing rules.
- [ ] Packed vectors, concatenations, slices, and bit-selects.
- [ ] always_comb and synthesizable procedural assignment lowering.
- [ ] Versioned supported-SystemVerilog specification.

## Phase 2 — Technology-independent RTL IR

- [x] Initial typed bit-vector RTL IR with explicit widths and signedness metadata.
- [x] Lower supported continuous assignments into the IR.
- [x] Insert explicit resize nodes for assignment width/signedness conversion.
- [x] Detect combinational dependency cycles during lowering.
- [x] Deterministic textual IR dump and lowering regression tests.
- [x] Common-subexpression elimination with canonical operand ordering.
- [x] Dead-code elimination rooted at observable output assignments.
- [x] Local identities for redundant resize, unary plus, duplicate bitwise operands, and identical mux branches.
- [ ] General constant propagation and arithmetic constant folding.
- [ ] Complete IEEE SystemVerilog expression sizing and context-determined width rules.
- [ ] Lower procedural assignments and sequential processes into the IR.

## Phase 3 — Boolean network and optimization

- [x] Initial bit-level AIG representation with complemented edges.
- [x] Lower supported combinational RTL operations into AIG nodes.
- [x] Structural hashing, constant simplification, and output-cone sweeping.
- [x] Reference-functional tests for add/subtract, muxes, comparisons, shifts, and multiply.
- [ ] Divide and modulo lowering.
- [ ] Multi-level AIG rewriting and balancing.
- [ ] Sequential boundary modelling.
- [ ] Formal equivalence checking.

## Phase 4 — ASIC library and constraints

- [ ] Liberty subset parser.
- [ ] Cells, pins, Boolean functions, area, timing arcs, and sequential semantics.
- [ ] Defined SDC subset parser.
- [ ] Independent library fixtures.

## Phase 5 — Technology mapping

- [ ] Cut enumeration and cut-based mapping.
- [ ] Area-oriented and delay-oriented mapping cost functions.
- [ ] Fanout-aware buffering and sizing.
- [ ] Mapped Verilog and machine-readable reports.

## Phase 6 — Product hardening

- [ ] Sequential cell mapping and clock/reset semantics.
- [ ] Benchmark suite and reproducible PPA reporting.
- [ ] Formal/equivalence validation hooks.
- [ ] Incremental compilation and stable CLI/API.
- [ ] Packaging, licensing, support, and release process.

## Current release boundary

The AIG stage is an initial two-state combinational prototype. It is not a complete SystemVerilog synthesis engine and does not produce technology-mapped standard-cell netlists.
