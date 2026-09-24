# Portable Linux builds

Run `./build.sh` on a Linux x86_64 machine with Python 3.10 or newer, the
Python venv and Tk modules, and internet access. On Ubuntu, the build packages
are `python3-venv` and `python3-tk`. The build creates an isolated Python
environment, installs the pinned build dependencies, and produces
`dist/rfe-portable-linux-x86_64.tar.gz`.

Copy the archive to a Linux x86_64 target, then run:

    tar -xzf rfe-portable-linux-x86_64.tar.gz
    ./siggen/siggen
    ./sigann/sigann

The target does not need Python or Python packages. Run `--check-runtime` on
either executable to verify bundled imports and USB discovery without opening
the GUI or RF device. Run `--check-gui` from a desktop session to test the
window and widgets without connecting to RF hardware.

## USB permissions

The target needs a graphical desktop and permission to open its USB serial
device. If an app reports `Permission denied` for `/dev/ttyUSB0`, check the
device and your active groups:

    ls -l /dev/ttyUSB*
    id -nG

For a device shown as `crw-rw---- ... root dialout ... /dev/ttyUSB0`, add your
user to `dialout`:

    sudo usermod -aG dialout "$USER"

Log out and back in before launching from the desktop. To use the new group in
the current terminal instead, run `newgrp dialout`, then launch the app from
that shell. Confirm that `id -nG` includes `dialout` and that the device is
readable and writable:

    test -r /dev/ttyUSB0 && test -w /dev/ttyUSB0 && echo 'USB access OK'

If the device has a different group with read/write permission, use that group
name in the `usermod` and `newgrp` commands. If its group has no read/write
permission, an administrator can install this udev rule for the USB bridge IDs
used by the apps:

    printf '%s\n' 'SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", GROUP="dialout", MODE="0660"' | sudo tee /etc/udev/rules.d/70-rfexplorer.rules
    sudo udevadm control --reload-rules

Unplug and reconnect the device, then check its permissions again. These IDs
are shared by other CP210x USB serial adapters, so the rule also applies to
those adapters. Do not run the apps as root to work around a permission error.

Build separately for each CPU architecture. Linux binaries also use the
target's glibc, so build on the oldest Linux distribution you intend to
support. The supplied x86_64 archive was built on Ubuntu 22.04 with glibc 2.35.
