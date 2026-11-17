# SYNVANTA

**Independent SystemVerilog-to-standard-cell ASIC synthesis compiler.**

SYNVANTA is an independently implemented ASIC synthesis compiler. Its current frontend accepts a documented SystemVerilog subset, lowers supported continuous assignments into a typed RTL IR, applies local RTL graph cleanup, and lowers supported combinational logic into a structurally hashed And-Inverter Graph (AIG).

> **Current status: early frontend, RTL IR, optimizer, and AIG prototype.** SYNVANTA does not yet support full IEEE SystemVerilog semantics, sequential procedural RTL, division/modulo in AIG lowering, Liberty ingestion, standard-cell mapping, or mapped-netlist generation.

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
synvanta opt examples/half_adder.sv
synvanta aig examples/half_adder.sv
```

- `check` validates the supported lexical, syntactic, and semantic subset.
- `inspect` prints module interfaces and assignment counts.
- `ir` prints the typed, technology-independent RTL IR.
- `opt` applies local identities, structural CSE, and output-rooted dead-code elimination.
- `aig` runs RTL optimization, then emits an AIG using complemented edges and structurally hashed AND nodes.

## Initial AIG support

The prototype lowers inputs, known two-state constants up to 64 bits, resize, unary/reduction logic, bitwise/logical operators, muxes, add/subtract, truncated multiply, comparisons, and logical shifts. Divide and modulo, unknown X/Z constants, and sequential elements are explicitly unsupported in this stage.

See [AIG design notes](docs/aig.md), [RTL optimization](docs/rtl-optimization.md), [architecture overview](docs/architecture/overview.md), and [roadmap](docs/roadmap.md).

## Project principles

- Independent synthesis implementation; no commercial synthesis engine as a runtime dependency.
- Explicit language-subset boundaries and diagnostics.
- Source-location preservation across compiler stages.
- Deterministic intermediate representations and reproducible tests.
- Correctness and semantic validation before PPA optimization.

## License

The licensing model is intentionally undecided at this early stage. No license grant is made by this repository until a license is selected and added.
