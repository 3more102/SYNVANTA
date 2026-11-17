# SYNVANTA Architecture

## Product boundary

SYNVANTA is an independently implemented SystemVerilog-to-standard-cell ASIC synthesis compiler. It is not a wrapper around a commercial synthesis engine. External tools may be used as optional reference points for validation, but they are not part of the synthesis implementation.

## Planned compilation pipeline

```text
SystemVerilog -> Lexer -> Parser -> AST
                                  |
                           Elaboration
                                  |
                         Typed RTL IR
                                  |
                       Boolean network
                                  |
                    Logic optimization
                                  |
                Liberty + SDC + constraints
                                  |
                     Technology mapping
                                  |
                  Mapped Verilog + reports
```

## Initial implementation slice (0.1)

The first increment establishes a compiler executable, source locations, diagnostics, a standalone lexer, a recursive-descent/Pratt parser, an AST for modules and continuous assignments, and automated tests. It intentionally does not claim to synthesize gates or map a cell library yet.

## Architectural rules

1. Keep parsing, elaboration, RTL semantics, optimization, mapping, and output generation in separate modules.
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
- Liberty parsing and timing arcs must follow the relevant Liberty language subset.
- Technology mapping quality requires benchmark suites and reproducible PPA measurement.
