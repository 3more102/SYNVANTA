# SYNVANTA Development Roadmap

## Phase 0 — Compiler foundation

- [x] C++20/CMake project skeleton.
- [x] Source locations and structured diagnostics.
- [x] SystemVerilog lexer for the initial subset.
- [x] Module/port/continuous-assignment AST.
- [x] Pratt expression parser.
- [x] CLI `check` and `inspect` commands.
- [x] Unit tests and CI workflow.
- [x] Module-local symbol table and basic semantic diagnostics.

## Phase 1 — Language semantics and elaboration

- [x] Module-local symbol table and duplicate/undeclared identifier checks.
- [ ] Constant-expression evaluation and parameter/localparam elaboration.
- [ ] Bit widths, signedness, casts, and expression sizing rules.
- [ ] Packed vectors, concatenations, slices, and bit-selects.
- [ ] `always_comb` and synthesizable procedural assignment lowering.
- [ ] Versioned supported-SystemVerilog specification.

## Phase 2 — Technology-independent RTL IR

- [x] Initial typed bit-vector RTL IR with explicit widths and signedness metadata.
- [x] Lower supported continuous assignments into the IR.
- [x] Insert explicit resize nodes for assignment width/signedness conversion.
- [x] Detect combinational dependency cycles during lowering.
- [x] Deterministic textual IR dump and lowering regression tests.
- [ ] Complete IEEE SystemVerilog expression sizing and context-determined width rules.
- [ ] Lower procedural assignments and sequential processes into the IR.
- [ ] Constant propagation, dead-code elimination, and common-subexpression elimination.

## Phase 3 — Boolean network and optimization

- [x] Lower supported combinational RTL into an AIG with complemented edges.
- [x] Structural hashing for AND nodes and basic Boolean identity simplification.
- [x] Output-cone sweeping to remove unreachable AIG logic.
- [x] Lower add/subtract, multiply, bitwise/logical operations, comparisons, shifts, and muxes.
- [x] Reference-functional tests for arithmetic, muxing, comparison, and shifts.
- [ ] AIG balancing, rewriting, and refactoring passes.
- [ ] Observability-based cleanup and sequential boundary modelling.
- [ ] Formal equivalence hooks and larger randomized regression campaigns.
- [ ] Division/modulo lowering.

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

Version 0.2 demonstrates frontend-to-RTL-IR-to-AIG lowering for a restricted combinational subset. It is not yet a standard-cell synthesis tool: no Liberty/SDC ingestion, cell mapping, timing analysis, or mapped-netlist generation is implemented.
