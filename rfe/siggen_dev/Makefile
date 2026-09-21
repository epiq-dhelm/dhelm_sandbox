PREFIX ?= /usr
LIBDIR ?= $(PREFIX)/lib/siggen
BINDIR ?= $(PREFIX)/bin
APPLICATIONSDIR ?= $(PREFIX)/share/applications
DESTDIR ?=

.PHONY: check install

check:
	python3 -m pytest -q
	python3 -m flake8 siggen siggen_core.py siggen_rfexplorer.py tests
	python3 -m py_compile siggen siggen_core.py siggen_rfexplorer.py
	desktop-file-validate siggen.desktop

install:
	install -d "$(DESTDIR)$(LIBDIR)" "$(DESTDIR)$(BINDIR)" \
		"$(DESTDIR)$(APPLICATIONSDIR)"
	install -m 0755 siggen "$(DESTDIR)$(LIBDIR)/siggen"
	install -m 0644 siggen_core.py siggen_rfexplorer.py "$(DESTDIR)$(LIBDIR)/"
	install -m 0755 siggen-launcher "$(DESTDIR)$(BINDIR)/siggen"
	install -m 0644 siggen.desktop "$(DESTDIR)$(APPLICATIONSDIR)/siggen.desktop"
	@if [ -z "$(DESTDIR)" ] && command -v update-desktop-database >/dev/null; then \
		update-desktop-database "$(APPLICATIONSDIR)"; \
	fi
