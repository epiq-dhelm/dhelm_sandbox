# Sigann

Sigann is a desktop spectrum display for an RF Explorer signal analyzer.

## Install

From this directory, run:

```sh
sudo make install
```

The installation has one public command and one desktop application:

- `/usr/bin/sigann` is the command and app-bar launcher target.
- `/usr/lib/sigann/` contains the application and its internal Python modules.
- `/usr/share/applications/sigann.desktop` is the app-bar entry.

The internal modules are installed together; users continue to run `sigann` or
select **Sigann** from the app bar.

## Test

```sh
make check
```

The automated suite uses a fake analyzer, so it does not require or modify RF
hardware. It covers validation, startup retry, USB port renumbering,
reconfiguration, sweep delivery, connection failure, and shutdown behavior.

## Structure

- `sigann`: Tk and Matplotlib user interface.
- `sigann_core.py`: settings, sweep models, and asynchronous controller.
- `sigann_rfexplorer.py`: RF Explorer and serial-port adapter.
- `tests/`: fake analyzer and automated tests.
