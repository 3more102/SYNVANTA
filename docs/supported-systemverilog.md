# Supported SystemVerilog Subset — SYNVANTA Frontend

This document describes frontend parsing and basic semantic support. Acceptance by the parser does not imply complete language semantics or synthesis support.

## Accepted constructs

- module and endmodule declarations.
- ANSI-style input, output, and inout ports; omitted port kinds default to wire in this frontend subset.
- Port and internal declaration keywords: wire, logic, reg, and bit.
- Optional signed and unsigned qualifiers.
- Packed ranges with integer-literal bounds, such as [7:0].
- Simple internal net declarations, including comma-separated names.
- Continuous assignments with identifier targets: assign y = expression;
- Identifiers, integer literals, sized based literals, and unbased unsized literals.
- Parenthesized expressions, unary operators, arithmetic, shifts, relational/equality, bitwise, logical, and conditional operators.
- Line comments and block comments.

## Basic semantic checks

- Duplicate module names are diagnosed.
- Duplicate port/internal-signal declarations are diagnosed.
- Assignment targets and expression identifiers must be declared in the current module.
- Continuous assignments may not drive input ports.
- Multiple continuous drivers are rejected for variable-like declarations; multiple drivers on wire nets are permitted.
- Undriven output ports produce warnings in the current continuous-assignment-only subset.

## Current restrictions

- Every ANSI port must declare its direction explicitly.
- Parameterized module headers and parameter expressions are not yet supported.
- Packed-range bounds must be integer literals.
- Assignment targets must be simple identifiers; part-select and concatenation l-values are not supported.
- Procedural blocks, functions, tasks, generate constructs, interfaces, packages, structs, unions, enums, unpacked arrays, and user-defined types are not yet supported.
- Full type checking, width inference, signedness propagation, and four-state semantic evaluation are not yet implemented.

Unsupported constructs produce diagnostics. A successful frontend check is not proof that a design is synthesizable or semantically complete.
