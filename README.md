# Venusian SAPI

[![CI](https://github.com/VenusianPHP/sapi/actions/workflows/ci.yml/badge.svg)](https://github.com/VenusianPHP/sapi/actions/workflows/ci.yml)

The executable a packaged Venusian app is. A PHP SAPI in plain C, built into a php-src tree as `sapi/venusian` with the app's extensions compiled in. `venusian build` uses it; nothing else needs to.

What it does:

* Boots PHP and runs the phar beside the executable. Looked for, in order: `$VENUSIAN_PHAR`, `<dir>/<name>.phar` (a `.deb` layout), `<dir>/../Resources/<name>.phar` (a `.app` layout). `<name>` is the executable's file name after symlinks are resolved, so a launcher symlinked onto `PATH` still finds its app.
* Reports `PHP_SAPI` as `cli`, so var-dumper, console, process and the rest treat the process as a terminal program. `php://fd` works. `STDIN`, `STDOUT`, `STDERR` are defined.
* Sets `PHP_BINARY` to the resolved executable. The framework's process pools spawn it on a worker script; the phar's stub runs a script inside the phar when `argv[1]` names one.
* Reads no `php.ini` from the host. Hard-coded: the CLI's settings (`html_errors=0`, `register_argc_argv=1`, `implicit_flush=1`, `output_buffering=0`, no time limits). Overridable defaults: `display_errors=stderr`, `memory_limit=1024M`.
* Does not change the working directory. Exit codes pass through. Nothing is appended to the executable, so it signs strictly on macOS.

## Build

In a php-src tree:

```sh
cp -r /path/to/sapi php-src/sapi/venusian
cd php-src && ./buildconf --force
./configure --disable-all --enable-venusian --enable-phar [--enable-zts] [extension flags…]
make -j"$(nproc)"
# sapi/venusian/venusian
```

`venusian --venusian-version` prints the SAPI and PHP versions. Any other invocation runs the app.

## Test

```sh
tests/smoke.sh sapi/venusian/venusian sapi/cli/php
```

Packs `tests/` into a phar with the stub shape `venusian/build` writes and checks the SAPI name, `PHP_BINARY`, `argv`, `php://fd`, the stream constants, worker dispatch, symlinked launch, the `.app` layout, the missing-phar message, exit codes, and output going to the app's log when it is discarded (a pipe keeps it). Prints `SMOKE_OK`.

## Platforms

Linux and macOS, PHP 8.4, NTS and ZTS. Windows is not supported.

## License

MIT.
