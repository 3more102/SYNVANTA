# SYNVANTA Architecture

## Product boundary

SYNVANTA is an independently implemented SystemVerilog-to-standard-cell ASIC synthesis compiler. It is not a wrapper around a commercial synthesis engine. External tools may be used as optional reference points for validation, but they are not part of the synthesis implementation.

## Planned compilation pipeline

```text
SystemVerilog -> Lexer -> Parser -> AST -> Semantic checks
                                  |
                           Elaboration
                                  |
                         Typed RTL IR
                                  |
                  RTL optimization / lowering
                                  |
                   AIG / Boolean network
                                  |
                    AIG rewriting and mapping
                                  |
                Liberty + SDC + constraints
                                  |
                     Technology mapping
                                  |
                  Mapped Verilog + reports
```

## Implemented foundation

The v0.1 foundation provides source locations, structured diagnostics, a lexer, parser, AST, module-local semantic checks, and a typed RTL IR for the supported continuous-assignment subset. The RTL optimizer performs initial constant folding, structural common-subexpression elimination, and dead-node removal.

The next engineering increment adds a complemented-edge AIG representation. The builder bit-blasts supported combinational RTL operations into a structurally hashed network and exposes it through the CLI AIG dump command. This is an intermediate synthesis representation, not a mapped netlist.

## Architectural rules

1. Keep parsing, elaboration, RTL semantics, optimization, mapping, and output generation in separate modules.
2. Preserve source locations through every intermediate representation.
3. Define deterministic intermediate representations and stable serialization before optimizing them.
4. Treat width, signedness, four-state values, and expression sizing as language semantics.
5. Make unsupported constructs explicit diagnostics. Never silently reinterpret unsupported RTL.
6. Keep synthesis algorithms independently implemented and independently tested.
7. Validate each optimization with semantic regression tests and optional equivalence checking.
8. Keep generic Boolean-network construction separate from technology mapping and timing-driven optimization.

## Technology baseline

- Core implementation: C++20.
- Build system: CMake 3.20+.
- Tests: dependency-free C++ test harness, including exhaustive small-vector AIG truth tables.
- CI: Linux build/test workflow.
- CLI: synvanta.

## Key design risks

- SystemVerilog is a large language; a documented synthesizable subset is mandatory.
- Expression sizing, signed arithmetic, packed arrays, procedural blocks, and reset semantics need dedicated specifications and tests.
- AIG construction currently uses a two-state model; X/Z semantics require an explicit policy.
- Liberty parsing and timing arcs must follow the relevant Liberty language subset.
- Technology mapping quality requires benchmark suites and reproducible PPA measurement.
