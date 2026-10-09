# Agent guidelines — Venusian SAPI

Plain C against PHP's own headers (`main/SAPI.h`, `main/php_main.h`). No Zephir, no helpers from the extensions.

* `sapi_module_struct.name` is `"cli"` on purpose: libraries decide terminal-versus-web by `PHP_SAPI`. Packaged-ness is the framework's question, answered by the phar base path.
* The phar is never appended to the executable. Lookup order and the `.deb` / `.app` layouts are in the README; change them in `venusian_find_phar()` and the README together.
* `PHP_BINARY` comes from `executable_location`, which PHP realpaths; keep it the resolved executable.
* Anything the CLI does that a library might depend on is copied from `sapi/cli/php_cli.c` of the pinned php-src, not improvised: ini defaults, stream constants, `SAPI_OPTION_NO_CHDIR`.
* Verify with `tests/smoke.sh` on a real build (Linux in the `ubuntu:24.04` build environment, macOS natively). The test needs a `php` with ext/phar.
* Knowledge bundle in [`.okf/`](.okf/); update it when behaviour changes.
* Commits: one change in the subject, one paragraph.
