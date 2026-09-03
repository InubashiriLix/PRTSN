# PRTSN — Agent Instructions

## Environment

All tooling is managed by Nix via `flake.nix`. Enter the dev shell with `nix develop` before running any build or flash commands.

**Do not add package managers, lock files, or tool directories to the project root.** This means no `package.json`, `requirements.txt`, standalone `Cargo.toml` at root, or any other dependency artifact outside what already exists.

Extra tooling may only be added if the user explicitly grants permission.

## Building

Verify builds inside the Nix dev shell:

```
nix develop
make build
```

Do not invoke `arduino-cli`, `cargo`, or other tools directly outside `nix develop`.
if you want to modify nix environment, you SHOULD ACCQUIRE user permission first.

## Flashing

Flash firmware through the Nix shell:

```
nix develop
make upload
```

## Project structure

- `src/` — firmware source (C++)
- `src/fw/inc/Result.h` — Rust-style `Result<T, E>` / `ErrorSet` type
- `src/svc/` — services (BLE HID, serial console, etc.)
- `src/app/examples/` — example/app entry points
- `config/` — board and project make config
- `tools/` — local development tools (serial TUI, BLE client)
- `flake.nix` — Nix dev environment

## Error handling convention

Use `result.is_err()` + `console.errorResult("context", result.error())` for all `Result`-typed setup calls. `errorResult` prints the top-level error and the full cause chain. See `BleAgentLightApp.h` for the established pattern.
