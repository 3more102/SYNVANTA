# Contributing to SYNVANTA

## Build and test

```bash
cmake -S . -B build -DSYNVANTA_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Engineering requirements

- Keep each compiler stage independently testable.
- Add a regression test for every parser, semantic, or optimization change.
- Do not silently accept unsupported language constructs.
- Preserve source locations and emit stable diagnostic codes.
- Do not introduce vendor source code, copied proprietary algorithms, or undocumented dependencies.
- Update the supported-language document when syntax support changes.
- Avoid claiming synthesis, equivalence, or PPA capabilities until those stages exist and have repeatable evidence.
