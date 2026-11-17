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
                    AIG / Boolean network
                                  |
                    Logic optimization
                                  |
                Liberty + SDC + constraints
                                  |
                     Technology mapping
                                  |
                  Mapped Verilog + reports
```

## Implemented slices

### 0.1 — frontend and typed RTL IR

The compiler has source locations, structured diagnostics, a lexer, a recursive-descent/Pratt parser, module-local semantic checks, an AST, and a typed RTL IR for the supported continuous-assignment subset.

### 0.2 — initial AIG lowering

The AIG stage converts typed combinational RTL operations into a graph containing primary-input nodes and two-input AND nodes. Inversion is represented on edges, so NOT, OR, XOR, XNOR, and mux logic are constructed from AND nodes and complemented literals. AND construction uses structural hashing and basic constant/identity simplification. An iterative output-cone sweep removes logic that cannot affect a top-level output.

The first lowering pass includes ripple-carry add/subtract, truncated shift-add multiplication, bitwise/logical operations, equality and magnitude comparisons, logical shifts, and muxes. Division and modulo are diagnosed as unsupported in this stage.

## Architectural rules

1. Keep parsing, elaboration, RTL semantics, Boolean lowering, optimization, mapping, and output generation in separate modules.
2. Preserve source locations through every intermediate representation.
3. Define deterministic intermediate representations and stable serialization before optimizing them.
4. Treat width, signedness, four-state values, and expression sizing as language semantics.
5. Make unsupported constructs explicit diagnostics. Never silently reinterpret unsupported RTL.
6. Keep synthesis algorithms independently implemented and independently tested.
7. Validate each optimization with semantic regression tests and optional equivalence checking.

## Technology baseline

- Core implementation: C++20.
- Build system: CMake 3.20+.
- Tests: dependency-free C++ test harness.
- CI: Linux build/test workflow.
- CLI: `synvanta`.

## Key design risks

- SystemVerilog is a large language; a documented synthesizable subset is mandatory.
- Expression sizing, signed arithmetic, packed arrays, procedural blocks, and reset semantics need dedicated specifications and tests.
- AIG rewriting and mapping quality require benchmark suites and reproducible QoR measurement.
- Liberty parsing and timing arcs must follow the relevant Liberty language subset.
- Technology mapping quality requires benchmark suites and reproducible PPA measurement.
