# Siggen

Siggen is a Tk control panel for an RF Explorer signal generator. It supports
continuous-wave output and stepped frequency sweeps.

For a build that carries Python and its packages to another Linux machine, see
[portable builds](../portable/README.md).

The application automatically examines attached RF Explorer USB devices and
connects only to a signal generator. An attached spectrum analyzer is skipped.
It uses the connected generator's reported frequency range for continuous-wave
and sweep settings, including the 0.1 MHz minimum of Combo generators.
If the generator disconnects or receives a different /dev/ttyUSB number,
Siggen waits for it and reconnects.

## Development

Run all fake-hardware tests and static checks with:

    make check

The tests never energize RF hardware.

## Installation

`siggen` uses `/usr/bin/python3`. Install its runtime dependencies for that
interpreter before running it or installing the launcher:

    sudo apt install python3-tk
    /usr/bin/python3 -m pip install --user pyudev RFExplorer

RFExplorer installs PySerial as a dependency. The `--user` packages are
available when you run `siggen` as the same user.

On Linux, the USB serial device is usually owned by the `dialout` group. Add
your user to that group, then start a new login session before running Siggen:

    sudo usermod -aG dialout "$USER"

    sudo make install

This installs the application under /usr/lib/siggen, a stable launcher at
/usr/bin/siggen, and the app-bar entry under /usr/share/applications.

## Structure

- siggen: Tk interface.
- siggen_core.py: settings validation and asynchronous controller.
- siggen_rfexplorer.py: RF Explorer and USB adapter.
- tests/: fake generator and adapter tests.
