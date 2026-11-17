# SYNVANTA

**Independent SystemVerilog-to-standard-cell ASIC synthesis compiler.**

SYNVANTA is an independently implemented ASIC synthesis engine. Its long-term target is a commercial-quality SystemVerilog-to-standard-cell compiler with its own frontend, typed RTL IR, Boolean optimization, Liberty ingestion, technology mapper, and timing-aware synthesis flow.

> **Current status: 0.2 — typed RTL IR and initial AIG lowering.** SYNVANTA lexes and parses a documented SystemVerilog subset, performs module-local semantic checks, lowers supported continuous assignments into a typed bit-vector RTL IR, and converts supported combinational operations into an AND-inverter graph (AIG). It does not yet parse Liberty/SDC, map standard cells, or emit a mapped netlist.

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
synvanta aig examples/half_adder.sv
```

- `check` runs lexical, syntax, and basic semantic checks.
- `inspect` prints module interfaces and assignment counts.
- `ir` lowers the supported combinational subset into a typed, technology-independent RTL IR and prints it.
- `aig` lowers the RTL IR into an AIG and prints primary inputs, AND nodes, complemented edges, and assignments.

## Architecture

```text
SystemVerilog -> Lexer -> Parser -> AST -> Semantic checks
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

See [architecture overview](docs/architecture/overview.md), [RTL IR](docs/rtl-ir.md), [AIG design](docs/aig.md), [roadmap](docs/roadmap.md), and [supported language subset](docs/supported-systemverilog.md).

## Project principles

- Independent synthesis implementation; no commercial synthesis engine as a runtime dependency.
- Explicit language-subset boundaries and diagnostics.
- Source-location preservation across compiler stages.
- Deterministic intermediate representations and reproducible tests.
- Correctness and semantic validation before PPA optimization.
- No area, timing, or power claims before library-aware analysis exists.

## Current validation

The unit suite includes reference simulation of half-adder logic, four-bit addition/subtraction, muxing, signed comparison, shifts, and multiplication. Structural hashing and constant simplification are checked directly. Division and modulo currently produce an explicit AIG-lowering diagnostic.

This is not yet formal equivalence, Liberty mapping, PPA signoff, or tapeout-ready software.

## License

The licensing model is intentionally undecided at this early stage. No license grant is made by this repository until a license is selected and added.
