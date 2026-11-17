# SYNVANTA

**Independent SystemVerilog-to-standard-cell ASIC synthesis compiler.**

SYNVANTA is a compiler project focused on building an independently implemented ASIC synthesis engine. The initial engineering target is a reliable SystemVerilog frontend and a clean architecture that can grow into elaboration, RTL lowering, Boolean optimization, Liberty ingestion, technology mapping, and mapped-netlist generation.

> **Current status: 0.1 frontend bring-up.** The frontend lexes/parses a documented SystemVerilog subset, builds an AST, resolves module-local names, checks continuous-assignment targets and drivers, and warns about undriven outputs. It does not yet lower RTL into a synthesis IR, optimize logic, read Liberty libraries, or map standard cells.

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
```

`check` validates lexical and syntactic structure for the supported subset. `inspect` prints parsed module ports and continuous-assignment counts.

## Architecture

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

See [architecture overview](docs/architecture/overview.md), [roadmap](docs/roadmap.md), and [supported language subset](docs/supported-systemverilog.md).

## Project principles

- Independent synthesis implementation; no commercial synthesis engine as a runtime dependency.
- Explicit language-subset boundaries and diagnostics.
- Source-location preservation across compiler stages.
- Deterministic intermediate representations and reproducible tests.
- Correctness and semantic validation before PPA optimization.

## License

The licensing model is intentionally undecided at this early stage. No license grant is made by this repository until a license is selected and added.
