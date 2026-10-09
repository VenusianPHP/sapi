dnl The Venusian SAPI: sapi/venusian in a php-src tree; enable with --enable-venusian.

PHP_ARG_ENABLE([venusian],
  [for the Venusian SAPI],
  [AS_HELP_STRING([--enable-venusian],
    [Build the Venusian SAPI, the executable of a packaged Venusian app])],
  [no],
  [no])

if test "$PHP_VENUSIAN" != "no"; then
  PHP_ADD_MAKEFILE_FRAGMENT([$abs_srcdir/sapi/venusian/Makefile.frag])

  SAPI_VENUSIAN_PATH=sapi/venusian/venusian

  PHP_SELECT_SAPI([venusian],
    [program],
    [venusian.c],
    [-DZEND_ENABLE_STATIC_TSRMLS_CACHE=1])

  AS_CASE([$host_alias],
    [*darwin*], [
      BUILD_VENUSIAN="\$(CC) \$(CFLAGS_CLEAN) \$(EXTRA_CFLAGS) \$(EXTRA_LDFLAGS_PROGRAM) \$(LDFLAGS) \$(NATIVE_RPATHS) \$(PHP_GLOBAL_OBJS:.lo=.o) \$(PHP_BINARY_OBJS:.lo=.o) \$(PHP_VENUSIAN_OBJS:.lo=.o) \$(PHP_FRAMEWORKS) \$(EXTRA_LIBS) \$(ZEND_EXTRA_LIBS) -o \$(SAPI_VENUSIAN_PATH)"
    ], [
      BUILD_VENUSIAN="\$(LIBTOOL) --tag=CC --mode=link \$(CC) -export-dynamic \$(CFLAGS_CLEAN) \$(EXTRA_CFLAGS) \$(EXTRA_LDFLAGS_PROGRAM) \$(LDFLAGS) \$(PHP_RPATHS) \$(PHP_GLOBAL_OBJS:.lo=.o) \$(PHP_BINARY_OBJS:.lo=.o) \$(PHP_VENUSIAN_OBJS:.lo=.o) \$(EXTRA_LIBS) \$(ZEND_EXTRA_LIBS) -o \$(SAPI_VENUSIAN_PATH)"
    ])

  PHP_SUBST([SAPI_VENUSIAN_PATH])
  PHP_SUBST([BUILD_VENUSIAN])
fi
