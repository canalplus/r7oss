modules modules_install help clean utils tests:
	$(MAKE) -C linux $@

.PHONY: apidoc
apidoc:
	rm -rf apidoc
	mkdir -p apidoc
	doxygen linux/kernel/include/linux/stm/Doxyfile
