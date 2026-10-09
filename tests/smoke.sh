#!/bin/sh
# Runs a built Venusian SAPI binary against a phar packed from tests/, and checks what a packaged app relies on.
# Usage: tests/smoke.sh <venusian binary> <php with ext/phar, phar.readonly off is set here>
set -e
BIN=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
PHP=${2:-php}
HERE=$(cd "$(dirname "$0")" && pwd)
WORK=$(cd "$(mktemp -d)" && pwd -P)   # resolved: PHP_BINARY is a realpath
trap 'rm -rf "$WORK"' EXIT

cp "$BIN" "$WORK/demo"
"$PHP" -d phar.readonly=0 -r '
    $phar = new Phar($argv[1]);
    $phar->addFile($argv[2], "app.php");
    $phar->addFile($argv[3], "worker.php");
    $phar->setStub(<<<PHP
<?php
Phar::interceptFileFuncs();
\$self = ["phar://".__FILE__."/", "phar://".realpath(__FILE__)."/"];
\$script = \$_SERVER["argv"][1] ?? "";
if (\$script !== "" && (str_starts_with(\$script, \$self[0]) || str_starts_with(\$script, \$self[1]))) {
    \$argv = \$_SERVER["argv"] = array_slice(\$_SERVER["argv"], 1);
    \$argc = \$_SERVER["argc"] = count(\$argv);
    require \$script;
    return;
}
require "phar://".__FILE__."/app.php";
__HALT_COMPILER();
PHP);
' "$WORK/demo.phar" "$HERE/app.php" "$HERE/worker.php"

fail() { echo "FAIL: $1"; exit 1; }

echo "== app run"
OUT=$(cd / && "$WORK/demo" one two)
echo "$OUT"
echo "$OUT" | grep -q '"sapi":"cli"' || fail "PHP_SAPI is not cli"
echo "$OUT" | grep -q "\"binary\":\"$WORK/demo\"" || fail "PHP_BINARY is not the executable"
echo "$OUT" | grep -q '"argv":\["'"$WORK"'/demo.phar","one","two"\]' || fail "argv is not [phar, one, two]"
echo "$OUT" | grep -q '"phar_fd":true' || fail "php://fd refused"
echo "$OUT" | grep -q '"stderr_const":true' || fail "STDERR undefined"
echo "$OUT" | grep -q '"cwd":"\/"' || fail "cwd changed"
echo "$OUT" | grep -q '"html_errors":"0"' || fail "html_errors on"

echo "== worker run (argv[1] names a script inside the phar)"
OUT=$("$WORK/demo" "phar://$WORK/demo.phar/worker.php" a b)
echo "$OUT"
echo "$OUT" | grep -q '"worker":"worker.php"' || fail "worker script did not run"
[ "$(echo "$OUT" | head -1 | cut -c1)" = "{" ] || fail "a script's shebang line was printed (CG(skip_shebang) unset)"

echo "== symlink elsewhere still finds the phar"
mkdir "$WORK/bin" && ln -s "$WORK/demo" "$WORK/bin/demo"
"$WORK/bin/demo" | grep -q '"sapi":"cli"' || fail "symlinked launch failed"

echo "== .app layout: Contents/MacOS/demo + Contents/Resources/demo.phar"
mkdir -p "$WORK/Demo.app/Contents/MacOS" "$WORK/Demo.app/Contents/Resources"
cp "$BIN" "$WORK/Demo.app/Contents/MacOS/demo" && cp "$WORK/demo.phar" "$WORK/Demo.app/Contents/Resources/demo.phar"
"$WORK/Demo.app/Contents/MacOS/demo" | grep -q "Resources/demo.phar" || fail ".app layout not found"

echo "== no phar: exit 1 and the paths it looked at"
mkdir "$WORK/none" && cp "$BIN" "$WORK/none/demo"
if "$WORK/none/demo" 2>"$WORK/err"; then fail "ran without a phar"; fi
grep -q "none/demo.phar" "$WORK/err" || fail "missing-phar message lacks the path"

echo "== exit code passes through"
printf '<?php exit(7);' > "$WORK/exit.php"
"$PHP" -d phar.readonly=0 -r '$p = new Phar($argv[1]); $p->addFile($argv[2], "x.php"); $p->setStub("<?php require \"phar://\".__FILE__.\"/x.php\"; __HALT_COMPILER();");' "$WORK/seven.phar" "$WORK/exit.php"
cp "$BIN" "$WORK/seven"
"$WORK/seven" && fail "exit code lost" || [ $? -eq 7 ] || fail "exit code was not 7"

echo "SMOKE_OK"
