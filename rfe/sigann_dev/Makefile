PREFIX ?= /usr
DESTDIR ?=

APPDIR := $(DESTDIR)$(PREFIX)/lib/sigann
BINDIR := $(DESTDIR)$(PREFIX)/bin
DESKTOPDIR := $(DESTDIR)$(PREFIX)/share/applications

.PHONY: check install

check:
	python3 -m pytest -q
	python3 -m flake8 sigann sigann_core.py sigann_rfexplorer.py tests
	python3 -m py_compile sigann sigann_core.py sigann_rfexplorer.py
	desktop-file-validate sigann.desktop

install:
	install -d "$(APPDIR)" "$(BINDIR)" "$(DESKTOPDIR)"
	install -m 0755 sigann "$(APPDIR)/sigann"
	install -m 0644 sigann_core.py sigann_rfexplorer.py "$(APPDIR)/"
	install -m 0755 sigann-launcher "$(BINDIR)/sigann"
	install -m 0644 sigann.desktop "$(DESKTOPDIR)/sigann.desktop"
	@if [ -z "$(DESTDIR)" ] && command -v update-desktop-database >/dev/null; then \
		update-desktop-database "$(DESKTOPDIR)"; \
	fi
