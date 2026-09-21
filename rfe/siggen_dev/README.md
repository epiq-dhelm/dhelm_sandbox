# Siggen

Siggen is a Tk control panel for an RF Explorer signal generator. It supports
continuous-wave output and stepped frequency sweeps.

The application automatically examines attached RF Explorer USB devices and
connects only to a signal generator. An attached spectrum analyzer is skipped.
If the generator disconnects or receives a different /dev/ttyUSB number,
Siggen waits for it and reconnects.

## Development

Run all fake-hardware tests and static checks with:

    make check

The tests never energize RF hardware.

## Installation

    sudo make install

This installs the application under /usr/lib/siggen, a stable launcher at
/usr/bin/siggen, and the app-bar entry under /usr/share/applications.

## Structure

- siggen: Tk interface.
- siggen_core.py: settings validation and asynchronous controller.
- siggen_rfexplorer.py: RF Explorer and USB adapter.
- tests/: fake generator and adapter tests.
