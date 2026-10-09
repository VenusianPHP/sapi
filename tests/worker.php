#!/usr/bin/env php
<?php
// Runs when the binary is given this file's phar path as its first argument, the way a process pool spawns PHP_BINARY on a worker script.
echo json_encode(['worker' => basename(__FILE__), 'argv' => $argv], JSON_UNESCAPED_SLASHES), "\n";
