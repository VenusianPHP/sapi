<?php
// Packed into app.phar by tests/smoke.sh with venusian/build's stub shape: a script inside the phar named as argv[1] runs, else this.
echo json_encode([
    'sapi' => PHP_SAPI,
    'binary' => PHP_BINARY,
    'argv' => $argv,
    'phar_fd' => fopen('php://fd/1', 'w') !== false,
    'stderr_const' => defined('STDERR'),
    'script' => $_SERVER['SCRIPT_FILENAME'],
    'cwd' => getcwd(),
    'html_errors' => ini_get('html_errors'),
    'memory_limit' => ini_get('memory_limit'),
    'display_errors' => ini_get('display_errors'),
], JSON_UNESCAPED_SLASHES), "\n";
