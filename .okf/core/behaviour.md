---
type: Concept
title: Behaviour
description: What the Venusian SAPI reports and copies from the CLI, and where it finds the app.
resource: venusian.c
tags: [sapi, cli, phar, php-binary]
status: draft
generated: { by: "claude-fable-5-1", at: "2026-10-09T00:00:00Z" }
sources:
  - id: c
    resource: venusian.c
    title: venusian.c
  - id: smoke
    resource: tests/smoke.sh
    title: smoke test
---

# Facts

| Fact | Why | Pinned by |
|---|---|---|
| `PHP_SAPI === 'cli'` | symfony/var-dumper, error-handler, process, monolog, guzzle, whoops and collision key terminal behaviour on it; `php://fd` is served under `cli` only | smoke `sapi`, `phar_fd`[^smoke] |
| Phar at `$VENUSIAN_PHAR`, `<dir>/<name>.phar`, `<dir>/../Resources/<name>.phar`; `<name>` after symlinks | `.deb` and `.app` layouts; launcher symlinks on `PATH` | smoke symlink and `.app` cases |
| `PHP_BINARY` = resolved executable | process pools spawn it on a worker script; the phar stub dispatches `argv[1]` | smoke worker case |
| `$argv` = `[phar, args…]` | what venusian/build's stub reads | smoke `argv` |
| No host `php.ini`; CLI hard-coded ini; `display_errors=stderr`, `memory_limit=1024M` overridable | a packaged app must not pick up the host's extensions or limits; same values phpmicro shipped with | smoke `html_errors` |
| No chdir, exit code passes through, `STDIN/STDOUT/STDERR` defined | CLI parity | smoke `cwd`, exit 7, `stderr_const` |
| `#!` first line skipped in every compiled file (`CG(skip_shebang)`) | CLI parity: the stub requires the app's `rocket`, whose shebang otherwise prints, and in a pool worker lands on the protocol pipe | smoke worker run, first line `{` |

Copied from `sapi/cli/php_cli.c` of php-src 8.4.26: ini defaults macro, hard-coded ini, stream constants, write/flush/log handlers.[^c]

[^c]: venusian.c
[^smoke]: smoke test
