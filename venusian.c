/*
 * The Venusian SAPI: the executable a packaged Venusian app is.
 *
 * It boots PHP with every extension compiled in, runs the phar that sits
 * beside it, and reports itself as "cli" so libraries that ask PHP_SAPI
 * (var-dumper, console, process) treat the process as a terminal program.
 * Nothing is appended to the executable, so it signs strictly on macOS.
 *
 * The phar is found, in order, at $VENUSIAN_PHAR, <dir>/<name>.phar beside
 * the executable (a .deb lays the app out this way) and
 * <dir>/../Resources/<name>.phar (a .app bundle). <name> is the executable's
 * own file name after symlinks are resolved. PHP_BINARY is the resolved
 * executable, so the framework's process pools spawn this program; the
 * phar's stub runs a script inside the phar when argv[1] names one.
 */

#include "php.h"
#include "php_globals.h"
#include "php_variables.h"
#include "php_main.h"
#include "php_ini.h"
#include "php_streams.h"
#include "SAPI.h"
#include "zend_stream.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#define VENUSIAN_SAPI_VERSION "0.10.0"

/* The CLI's own hard-coded settings, so the process behaves like php on a terminal. */
static const char HARDCODED_INI[] =
	"html_errors=0\n"
	"register_argc_argv=1\n"
	"implicit_flush=1\n"
	"output_buffering=0\n"
	"max_execution_time=0\n"
	"max_input_time=-1\n";

static char *venusian_phar = NULL;

#define INI_DEFAULT(name, value) \
	ZVAL_NEW_STR(&tmp, zend_string_init(value, sizeof(value) - 1, 1)); \
	zend_hash_str_update(configuration_hash, name, sizeof(name) - 1, &tmp);

/* Overridable defaults: the same values venusian/build gave phpmicro. */
static void venusian_ini_defaults(HashTable *configuration_hash)
{
	zval tmp;
	INI_DEFAULT("display_errors", "stderr");
	INI_DEFAULT("memory_limit", "1024M");
}

static inline ssize_t venusian_single_write(const char *str, size_t str_length)
{
	ssize_t ret = write(STDOUT_FILENO, str, str_length);
	return ret <= 0 ? 0 : ret;
}

static size_t venusian_ub_write(const char *str, size_t str_length)
{
	const char *ptr = str;
	size_t remaining = str_length;

	while (remaining > 0) {
		ssize_t ret = venusian_single_write(ptr, remaining);
		if (ret <= 0) {
			EG(exit_status) = 255;
			php_handle_aborted_connection();
			break;
		}
		ptr += ret;
		remaining -= ret;
	}

	return str_length;
}

static void venusian_flush(void *server_context)
{
	if (fflush(stdout) == EOF && errno != EBADF) {
		php_handle_aborted_connection();
	}
}

static void venusian_log_message(const char *message, int syslog_type_int)
{
	fprintf(stderr, "%s\n", message);
}

static int venusian_deactivate(void)
{
	fflush(stdout);
	return SUCCESS;
}

static void venusian_register_variables(zval *track_vars_array)
{
	size_t len;
	char *docroot = "";

	php_import_environment_variables(track_vars_array);

	len = strlen(venusian_phar);
	if (sapi_module.input_filter(PARSE_SERVER, "PHP_SELF", &venusian_phar, len, &len)) {
		php_register_variable("PHP_SELF", venusian_phar, track_vars_array);
	}
	if (sapi_module.input_filter(PARSE_SERVER, "SCRIPT_NAME", &venusian_phar, len, &len)) {
		php_register_variable("SCRIPT_NAME", venusian_phar, track_vars_array);
	}
	if (sapi_module.input_filter(PARSE_SERVER, "SCRIPT_FILENAME", &venusian_phar, len, &len)) {
		php_register_variable("SCRIPT_FILENAME", venusian_phar, track_vars_array);
	}
	if (sapi_module.input_filter(PARSE_SERVER, "PATH_TRANSLATED", &venusian_phar, len, &len)) {
		php_register_variable("PATH_TRANSLATED", venusian_phar, track_vars_array);
	}
	len = 0;
	if (sapi_module.input_filter(PARSE_SERVER, "DOCUMENT_ROOT", &docroot, len, &len)) {
		php_register_variable("DOCUMENT_ROOT", docroot, track_vars_array);
	}
}

static int venusian_header_handler(sapi_header_struct *h, sapi_header_op_enum op, sapi_headers_struct *s)
{
	return 0;
}

static int venusian_send_headers(sapi_headers_struct *sapi_headers)
{
	return SAPI_HEADER_SENT_SUCCESSFULLY;
}

static void venusian_send_header(sapi_header_struct *sapi_header, void *server_context)
{
}

static char *venusian_read_cookies(void)
{
	return NULL;
}

static int venusian_startup(sapi_module_struct *sapi_module)
{
	return php_module_startup(sapi_module, NULL);
}

static sapi_module_struct venusian_sapi_module = {
	"cli",                          /* name: what PHP_SAPI reports */
	"Venusian",                     /* pretty name */

	venusian_startup,               /* startup */
	php_module_shutdown_wrapper,    /* shutdown */

	NULL,                           /* activate */
	venusian_deactivate,            /* deactivate */

	venusian_ub_write,              /* unbuffered write */
	venusian_flush,                 /* flush */
	NULL,                           /* get uid */
	NULL,                           /* getenv */

	php_error,                      /* error handler */

	venusian_header_handler,        /* header handler */
	venusian_send_headers,          /* send headers handler */
	venusian_send_header,           /* send header handler */

	NULL,                           /* read POST data */
	venusian_read_cookies,          /* read Cookies */

	venusian_register_variables,    /* register server variables */
	venusian_log_message,           /* Log message */
	NULL,                           /* Get request time */
	NULL,                           /* Child terminate */

	STANDARD_SAPI_MODULE_PROPERTIES
};

/* STDIN, STDOUT and STDERR, as the CLI defines them. */
static void venusian_register_file_handles(void)
{
	php_stream *s_in, *s_out, *s_err;
	zend_constant ic, oc, ec;

	s_in = php_stream_open_wrapper_ex("php://stdin", "rb", 0, NULL, NULL);
	s_out = php_stream_open_wrapper_ex("php://stdout", "wb", 0, NULL, NULL);
	s_err = php_stream_open_wrapper_ex("php://stderr", "wb", 0, NULL, NULL);

	if (s_in) s_in->flags |= PHP_STREAM_FLAG_NO_RSCR_DTOR_CLOSE;
	if (s_out) s_out->flags |= PHP_STREAM_FLAG_NO_RSCR_DTOR_CLOSE;
	if (s_err) s_err->flags |= PHP_STREAM_FLAG_NO_RSCR_DTOR_CLOSE;

	if (s_in == NULL || s_out == NULL || s_err == NULL) {
		if (s_in) php_stream_close(s_in);
		if (s_out) php_stream_close(s_out);
		if (s_err) php_stream_close(s_err);
		return;
	}

	php_stream_to_zval(s_in, &ic.value);
	php_stream_to_zval(s_out, &oc.value);
	php_stream_to_zval(s_err, &ec.value);

	Z_CONSTANT_FLAGS(ic.value) = 0;
	ic.name = zend_string_init_interned("STDIN", sizeof("STDIN") - 1, 0);
	zend_register_constant(&ic);

	Z_CONSTANT_FLAGS(oc.value) = 0;
	oc.name = zend_string_init_interned("STDOUT", sizeof("STDOUT") - 1, 0);
	zend_register_constant(&oc);

	Z_CONSTANT_FLAGS(ec.value) = 0;
	ec.name = zend_string_init_interned("STDERR", sizeof("STDERR") - 1, 0);
	zend_register_constant(&ec);
}

/* The executable's real path, symlinks resolved; NULL when the OS will not say. */
static char *venusian_executable(void)
{
	char path[PATH_MAX];
	char real[PATH_MAX];

#ifdef __APPLE__
	uint32_t size = sizeof(path);
	if (_NSGetExecutablePath(path, &size) != 0) {
		return NULL;
	}
#else
	ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
	if (n < 0) {
		return NULL;
	}
	path[n] = '\0';
#endif

	if (realpath(path, real) == NULL) {
		return NULL;
	}

	return strdup(real);
}

/* $VENUSIAN_PHAR, <dir>/<name>.phar, <dir>/../Resources/<name>.phar: the first that is readable. */
static char *venusian_find_phar(const char *executable, char candidates[3][PATH_MAX], int *count)
{
	const char *env = getenv("VENUSIAN_PHAR");
	const char *slash = strrchr(executable, '/');
	size_t dir_len = slash ? (size_t) (slash - executable) : 0;
	const char *name = slash ? slash + 1 : executable;
	int n = 0;

	if (env && *env) {
		snprintf(candidates[n++], PATH_MAX, "%s", env);
	}
	snprintf(candidates[n++], PATH_MAX, "%.*s/%s.phar", (int) dir_len, executable, name);
	snprintf(candidates[n++], PATH_MAX, "%.*s/../Resources/%s.phar", (int) dir_len, executable, name);
	*count = n;

	for (int i = 0; i < n; i++) {
		if (access(candidates[i], R_OK) == 0) {
			char real[PATH_MAX];
			return strdup(realpath(candidates[i], real) ? real : candidates[i]);
		}
	}

	return NULL;
}

int main(int argc, char **argv)
{
	int exit_status = 0;
	char *executable;
	char candidates[3][PATH_MAX];
	int candidate_count = 0;
	char **php_argv;
	zend_file_handle file_handle;

	if (argc == 2 && strcmp(argv[1], "--venusian-version") == 0) {
		printf("Venusian SAPI %s, PHP %s%s\n", VENUSIAN_SAPI_VERSION, PHP_VERSION,
#ifdef ZTS
			" ZTS"
#else
			" NTS"
#endif
		);
		return 0;
	}

	executable = venusian_executable();
	if (executable == NULL) {
		fprintf(stderr, "venusian: cannot resolve the executable's own path: %s\n", strerror(errno));
		return 1;
	}

	venusian_phar = venusian_find_phar(executable, candidates, &candidate_count);
	if (venusian_phar == NULL) {
		fprintf(stderr, "venusian: no app to run. Looked for:\n");
		for (int i = 0; i < candidate_count; i++) {
			fprintf(stderr, "  %s\n", candidates[i]);
		}
		free(executable);
		return 1;
	}

	/* $argv as the phar's stub expects it: the phar, then the program's arguments. */
	php_argv = calloc((size_t) argc + 1, sizeof(char *));
	php_argv[0] = venusian_phar;
	for (int i = 1; i < argc; i++) {
		php_argv[i] = argv[i];
	}

#if defined(SIGPIPE) && defined(SIG_IGN)
	signal(SIGPIPE, SIG_IGN);
#endif

#ifdef ZTS
	php_tsrm_startup();
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	zend_signal_startup();

	sapi_startup(&venusian_sapi_module);
	venusian_sapi_module.ini_defaults = venusian_ini_defaults;
	venusian_sapi_module.ini_entries = HARDCODED_INI;
	venusian_sapi_module.php_ini_ignore = 1;      /* a packaged app reads no php.ini from the host */
	venusian_sapi_module.php_ini_ignore_cwd = 1;
	venusian_sapi_module.phpinfo_as_text = 1;
	venusian_sapi_module.executable_location = executable;  /* becomes PHP_BINARY */

	if (venusian_sapi_module.startup(&venusian_sapi_module) == FAILURE) {
		fprintf(stderr, "venusian: PHP failed to start\n");
		exit_status = 1;
		goto done;
	}

	SG(options) |= SAPI_OPTION_NO_CHDIR;
	SG(request_info).argc = argc;
	SG(request_info).argv = php_argv;
	SG(request_info).path_translated = venusian_phar;

	zend_first_try {
		if (php_request_startup() == FAILURE) {
			exit_status = 1;
		} else {
			zend_is_auto_global(ZSTR_KNOWN(ZEND_STR_AUTOGLOBAL_SERVER));
			PG(during_request_startup) = 0;

			venusian_register_file_handles();

			zend_stream_init_filename(&file_handle, venusian_phar);
			file_handle.primary_script = 1;
			php_execute_script(&file_handle);
			zend_destroy_file_handle(&file_handle);

			exit_status = EG(exit_status);
			php_request_shutdown(NULL);
		}
	} zend_end_try();

	php_module_shutdown();

done:
	sapi_shutdown();
#ifdef ZTS
	tsrm_shutdown();
#endif
	free(php_argv);
	free(venusian_phar);
	free(executable);

	return exit_status;
}
