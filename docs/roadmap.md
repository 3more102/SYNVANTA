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
- [x] Reject multi-driver wire nets and undriven outputs at the RTL-lowering boundary.
- [x] Deterministic textual IR dump and lowering regression tests.
- [x] Initial constant folding for supported two-state operations up to 64 bits.
- [x] Common-subexpression elimination and output-rooted dead-code elimination.
- [x] Constant-condition mux simplification and conservative local identities.
- [ ] Complete IEEE SystemVerilog expression sizing and context-determined width rules.
- [ ] Lower procedural assignments and sequential processes into the IR.
- [ ] Broader bit-level constant propagation and rewrite passes.

## Phase 3 — Boolean network and optimization

- [ ] Lower combinational RTL into an AIG or equivalent Boolean network.
- [ ] Structural hashing and rewriting.
- [ ] Observability-based cleanup and sequential boundary modelling.
- [ ] Equivalence-oriented regression tests.

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

Version 0.1 is a parser bring-up milestone only. It is not yet a synthesis tool and does not produce optimized or technology-mapped netlists.
