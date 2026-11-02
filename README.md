# ntverpctl

A small Windows command-line utility for inspecting and updating build numbers
in `ntverp.h` files and the build lab in `tools\razzle.cmd`.

It is a native Win32/stdio implementation. It does not include or require any
external source tree; it operates only on the files you point it at.

## Build

Cross-compile from Linux with MinGW-w64:

```bash
make
```

Or build from MSYS2 on Windows with the same `x86_64-w64-mingw32-gcc` toolchain.
The result is a single `ntverpctl.exe`.

## Usage

```text
ntverpctl show
ntverpctl bump
ntverpctl bump-build
ntverpctl set-build N
ntverpctl set-qfe N
ntverpctl set-lab NAME
```

- `show` prints the current build lab and the build/QFE values from each
  matching `ntverp.h`.
- `bump` increments `VER_PRODUCTBUILD_QFE` by 1.
- `bump-build` increments `VER_PRODUCTBUILD` by 1.
- `set-build N` sets `VER_PRODUCTBUILD` to `N`.
- `set-qfe N` sets `VER_PRODUCTBUILD_QFE` to `N`.
- `set-lab NAME` replaces `_BuildBranch=` in `tools\razzle.cmd`.

## Environment

- `TREE` — source tree root. Accepts `D:\src`, `D:/src`, `/d/src`, or
  `/mnt/d/src`. If it is not set, ntverpctl prompts for it.
- `NTVERP` — optional path to a specific `ntverp.h`. When set, the tree is not
  searched.

## Behavior notes

- `ntverp.h` files are discovered recursively under `TREE` and must contain a
  `VER_PRODUCTBUILD` define.
- If several files match, ntverpctl prompts for a number or `a` for all.
- Each edited file is backed up once as `<file>.orig`.
- A `// Windows NT Build X.Y` line is kept in sync when both
  `VER_PRODUCTBUILD` and `VER_PRODUCTBUILD_QFE` have numeric values. The
  comment may be indented, and trailing `//` or `/* ... */` comments on the
  defines are ignored.
- Exit code is `0` on success and `1` on error.

## License

MIT. See `LICENSE`.

This project is not affiliated with Microsoft and contains no Microsoft source
code.
