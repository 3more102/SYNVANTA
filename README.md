# SYNVANTA

**Independent SystemVerilog-to-standard-cell ASIC synthesis compiler.**

SYNVANTA is a compiler project focused on building an independently implemented ASIC synthesis engine. The initial engineering target is a reliable SystemVerilog frontend and a clean architecture that can grow into elaboration, RTL lowering, Boolean optimization, Liberty ingestion, technology mapping, and mapped-netlist generation.

> **Current status: 0.1 frontend, typed RTL IR, and early graph optimization.** SYNVANTA lexes/parses a documented SystemVerilog subset, performs module-local semantic checks, lowers supported continuous assignments into a typed bit-vector RTL IR, and applies structural common-subexpression elimination plus output-rooted dead-code elimination. It does not yet perform constant propagation, Boolean rewriting, read Liberty libraries, map standard cells, or emit mapped netlists.

## Build

Requirements: CMake 3.20+ and a C++20 compiler.

```bash
cmake -S . -B build -DSYNVANTA_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## CLI

```bash
synvanta --version
synvanta --help
synvanta check examples/half_adder.sv
synvanta inspect examples/mux2.sv
synvanta ir examples/half_adder.sv
synvanta optimize examples/half_adder.sv
```

- `check` runs lexical, syntax, and basic semantic checks.
- `inspect` prints module interfaces and assignment counts.
- `ir` lowers the supported combinational subset into a typed, technology-independent RTL IR and prints it.
- `optimize` applies structural common-subexpression elimination and removes logic that cannot affect a module output, then prints the optimized IR.

The optimizer is an early graph-cleanup pass. It does not yet perform arithmetic constant folding, Boolean rewriting, or technology mapping.

## Architecture

```text
SystemVerilog -> Lexer -> Parser -> AST
                                  |
                           Elaboration
                                  |
                         Typed RTL IR
                                  |
                 CSE + output-rooted DCE
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

See [architecture overview](docs/architecture/overview.md), [roadmap](docs/roadmap.md), and [supported language subset](docs/supported-systemverilog.md).

## Project principles

- Independent synthesis implementation; no commercial synthesis engine as a runtime dependency.
- Explicit language-subset boundaries and diagnostics.
- Source-location preservation across compiler stages.
- Deterministic intermediate representations and reproducible tests.
- Correctness and semantic validation before PPA optimization.

## License

The licensing model is intentionally undecided at this early stage. No license grant is made by this repository until a license is selected and added.
