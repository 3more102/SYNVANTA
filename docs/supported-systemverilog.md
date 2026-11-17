# Supported SystemVerilog Subset — SYNVANTA 0.1

This document describes parser bring-up support. Parsing support does not imply complete language-semantic or synthesis support.

## Accepted constructs

- `module` / `endmodule` declarations.
- ANSI-style `input`, `output`, and `inout` ports.
- Port keywords: `wire`, `logic`, `reg`, and `bit`.
- Optional `signed` / `unsigned` qualifiers.
- Packed ranges with integer-literal bounds, such as `[7:0]`.
- Simple internal net declarations.
- Continuous assignments with identifier targets: `assign y = expression;`.
- Identifiers, integer literals, and sized based literals.
- The parser recognizes unbased unsized fill literals ('0 and '1), but RTL lowering rejects them until context-determined sizing is implemented. Wildcard `?` digits inside based literals are not yet accepted; `?` is tokenized as the conditional operator.
- Parenthesized expressions, unary operators, arithmetic, shifts, relational/equality, bitwise, logical, and conditional operators.
- Line comments and block comments.

## Current restrictions

- Every ANSI port must declare its direction explicitly.
- Parameterized module headers and parameter expressions are not yet supported.
- Packed-range bounds must be integer literals.
- Assignment targets must be simple identifiers; part-select and concatenation l-values are not supported.
- Procedural blocks, functions, tasks, generate constructs, interfaces, packages, structs, unions, enums, unpacked arrays, and user-defined types are not yet supported.
- Basic module-local name resolution and continuous-driver checks are implemented. The initial RTL IR carries width and signedness metadata, but its expression-sizing rules are limited and are not a complete implementation of IEEE SystemVerilog sizing, casting, or four-state semantics.

Unsupported constructs produce diagnostics. A successful parse is not proof that a design is synthesizable or semantically complete.
