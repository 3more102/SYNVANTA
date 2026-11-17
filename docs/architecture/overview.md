# SYNVANTA Architecture

## Product boundary

SYNVANTA is an independently implemented SystemVerilog-to-standard-cell ASIC synthesis compiler. It is not a wrapper around a commercial synthesis engine. External tools may be used as optional reference points for validation, but they are not part of the synthesis implementation.

## Compilation pipeline

```text
SystemVerilog -> Lexer -> Parser -> AST -> Semantic checks
                                  |
                           Elaboration
                                  |
                         Typed RTL IR
                                  |
                     RTL graph optimization
                                  |
                      Bit-level AIG network
                                  |
                  AIG rewriting and balancing
                                  |
                Liberty + SDC + constraints
                                  |
                     Technology mapping
                                  |
                  Mapped Verilog + reports
```

## Initial implementation boundary

The frontend currently supports a documented subset centered on ANSI-style modules and continuous assignments. The typed RTL IR preserves operation widths, signedness metadata, and source locations. The optimizer performs local identity reductions, structural common-subexpression elimination, and output-rooted dead-code elimination.

The initial AIG backend bit-blasts the supported combinational subset into input nodes and structurally hashed AND nodes with complemented edges. It explicitly rejects unsupported arithmetic operators and unknown four-state constants.

## Architectural rules

1. Keep parsing, elaboration, RTL semantics, optimization, AIG lowering, mapping, and output generation in separate modules.
2. Preserve source locations through every intermediate representation.
3. Define deterministic intermediate representations and stable serialization before optimizing them.
4. Treat width, signedness, four-state values, and expression sizing as language semantics.
5. Make unsupported constructs explicit diagnostics. Never silently reinterpret unsupported RTL.
6. Keep synthesis algorithms independently implemented and independently tested.
7. Validate each optimization with semantic regression tests and optional equivalence checking.

## Technology baseline

- Core implementation: C++20.
- Build system: CMake 3.20+.
- Tests: dependency-free C++ test harness initially.
- CI: Linux build/test workflow.
- CLI: `synvanta`.

## Key design risks

- SystemVerilog is a large language; a documented synthesizable subset is mandatory.
- Expression sizing, signed arithmetic, packed arrays, procedural blocks, and reset semantics need dedicated specifications and tests.
- AIG correctness depends on preserving bit ordering, signed comparisons, resize semantics, and two-state assumptions.
- Liberty parsing and timing arcs must follow the relevant Liberty language subset.
- Technology mapping quality requires benchmark suites and reproducible PPA measurement.
