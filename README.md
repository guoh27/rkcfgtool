# rkcfgtool

Command line utility for Rockchip RKDevTool configuration files.
It can list, edit and create CFG entries.

## Build

```sh
make
```

## Usage

```sh
rkcfgtool <cfg> [actions...] [-o output.cfg]
rkcfgtool --create <output.cfg> [actions...]
```

Actions:
- `--list` – list entries (default)
- `--set-path <idx> <newPath>` – change path of entry
- `--add <name> <path>` – append a new entry
- `--del <idx>` – delete entry

## Tests

Run `./test.sh` to build and verify the tool against the sample files in `cfg/`.
