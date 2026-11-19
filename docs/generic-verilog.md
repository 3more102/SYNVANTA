# Generic Verilog export

Command: synvanta emit-verilog <design.sv>

The command parses the supported SystemVerilog subset, lowers to RTL IR,
runs RTL optimization, bit-blasts to AIG, performs AND-tree balancing and
sweeping, then emits a deterministic, two-state Verilog netlist.

The exporter emits one wire and continuous assignment for each AND node,
bitwise inversions for complemented edges, vector input/output declarations,
and explicit 1-bit constants. It escapes Verilog identifiers and checks
topological node order, primary-input metadata, output drivers, port
collisions, and missing bus bits. Malformed graphs are rejected with VLOG
diagnostics; they do not produce partial netlists.

Example:
    cmake -S . -B build -DSYNVANTA_BUILD_TESTS=ON
    cmake --build build
    ./build/synvanta emit-verilog examples/half_adder.sv > half_adder_generic.v

This is a technology-independent netlist, NOT a Liberty-mapped ASIC netlist.
There are no standard-cell instances, timing arcs, SDC, or PPA reports.
