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
- [x] Preserve the fill semantics of unbased unsized '0 and '1 literals in the initial lowering subset.

## Phase 2 — Technology-independent RTL IR

- [x] Initial typed bit-vector RTL IR with explicit widths and signedness metadata.
- [x] Lower supported continuous assignments into the IR.
- [x] Insert explicit resize nodes for assignment width/signedness conversion.
- [x] Detect combinational dependency cycles during lowering.
- [x] Reject multi-driver wire nets and undriven outputs at the RTL-lowering boundary.
- [x] Deterministic textual IR dump and lowering regression tests.
- [x] Initial constant folding for supported two-state operations up to 64 bits.
- [x] Common-subexpression elimination and dead-node removal.
- [ ] Complete IEEE SystemVerilog expression sizing and context-determined width rules.
- [ ] Lower procedural assignments and sequential processes into the IR.
- [ ] Broader bit-level constant propagation and rewrite passes.

## Phase 3 — Boolean network and optimization

- [x] Complemented-edge AIG literal representation.
- [x] Bit-blast supported combinational RTL IR into AIG nodes.
- [x] Structural hashing and basic constant/complement simplifications.
- [x] AIG construction for Boolean logic, muxes, add/subtract, multiply, comparisons, and logical shifts.
- [x] Positive-polarity AND-tree balancing and unreachable-node sweeping.
- [x] Exhaustive truth-table tests for representative small combinational designs.
- [ ] AIG rewriting and cut-based local optimization.
- [ ] Observability-based cleanup and sequential boundary modelling.
- [ ] Broader equivalence-oriented regression tests and independent formal checking.

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

The main branch's v0.1 milestone establishes the parser, semantic checks, and typed combinational RTL IR. The feature branch adds an independently implemented two-state AIG builder, structural hashing, balancing, sweeping, and exhaustive small-design tests. Neither milestone is a complete ASIC synthesis product: sequential synthesis, full SystemVerilog semantics, Liberty mapping, SDC timing analysis, and PPA optimization remain future work.
