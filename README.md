# rkcfgtool

Command line utility for Rockchip RKDevTool configuration files.
It can list, edit and create CFG entries. Parts of this project were developed
with reference to [PenUniverse/RKCfgTool](https://github.com/PenUniverse/RKCfgTool).

## Build

```sh
make
```

## Usage

```sh
rkcfgtool <cfg> [--create] [actions...] [-o output.cfg]
rkcfgtool --help | --version
```

If `-o` is omitted, changes are written back to `<cfg>`.

Actions:

- `--list` – list entries (default)
- `--set-path <idx> <newPath>` – change path of entry
- `--set-name <idx> <newName>` – change name of entry
- `--add <name> <path>` – append a new entry
- `--del <idx>` – delete entry
- `--enable <idx> <1|0>` – set enable flag for entry
- `--json` – output entries as JSON
- `--create` – start a new config instead of reading one
- `--version` – show rkcfgtool version
- `--help` – show usage information
- `<idx>` may be `-1` to target the last entry

Plain output lists `index name path enabled`. JSON objects include an
`"enabled"` field.

## Tests

Run `./test.sh` to build and verify the tool against the sample files in `cfg/`.
