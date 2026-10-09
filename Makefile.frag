venusian: $(SAPI_VENUSIAN_PATH)

$(SAPI_VENUSIAN_PATH): $(PHP_GLOBAL_OBJS) $(PHP_BINARY_OBJS) $(PHP_VENUSIAN_OBJS)
	$(BUILD_VENUSIAN)

install-venusian: $(SAPI_VENUSIAN_PATH)
	@echo "Installing Venusian SAPI binary:  $(INSTALL_ROOT)$(bindir)/"
	@$(mkinstalldirs) $(INSTALL_ROOT)$(bindir)
	@$(LIBTOOL) --mode=install $(INSTALL) -m 0755 $(SAPI_VENUSIAN_PATH) $(INSTALL_ROOT)$(bindir)/venusian$(EXEEXT)
