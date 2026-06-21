# PythonCaller

A lightweight Windows launcher that runs a Python application from a native `.exe`. It loads `python3.dll` and invokes `Py_Main`, so your app ships as a familiar executable while the logic stays in Python.

## How It Works

1. On startup, the launcher looks for a config file next to itself: `<YourApp>.exe.conf`.
2. It reads the working directory and entry script from that file, then changes the process working directory.
3. It loads `python3.dll` from the working directory and calls `Py_Main` with arguments equivalent to:

   ```
   python.exe <entry_script> [command-line args...]
   ```

Command-line arguments passed to the `.exe` are forwarded to the Python script (the launcher inserts the script path as `argv[1]`). `argv[0]` passed to `Py_Main` is always the launcher's full path from `GetModuleFileNameW`, not the value from the command line — otherwise running `HttpRequester\HttpRequester.exe` from a parent directory would give Python a relative `argv[0]` and break path resolution.

## Building

Open `PythonCaller.sln` in Visual Studio 2022 and build the desired configuration (Win32/x64, Debug/Release). The output executable is written to `lib_win_<platform>_<configuration>/`.

Rename the built executable to match your application (e.g. `HttpRequester.exe`).

## `exe.conf` Format

Place a text file named `<YourApp>.exe.conf` in the same directory as the executable. The file has two lines:

| Line | Description |
|------|-------------|
| 1 | Working directory, relative to the **executable's directory** (not the process current working directory; where `python3.dll` lives) |
| 2 | Entry script path, relative to that working directory |

Example (`HttpRequester.exe.conf`):

```
RuntimeX64
Lib\http-requester\main.pyc
```

This tells the launcher to:

- Set the working directory to `<exe_dir>\RuntimeX64`
- Run `Lib\http-requester\main.pyc` as the Python entry point

Use forward slashes or backslashes in paths. Lines may end with `\r\n` or `\n`.

## Deployment Layout

A typical install directory looks like this:

```
YourApp/
├── YourApp.exe              # PythonCaller, renamed
├── YourApp.exe.conf         # Launcher config
├── RuntimeX64/              # Embedded Python runtime (from python.org embeddable package)
│   ├── python3.dll
│   ├── python311.dll
│   ├── python311.zip
│   └── Lib/
│       ├── site-packages/   # Third-party packages
│       └── your_package/
│           └── main.py      # (or main.pyc)
├── config/                  # App data (optional)
└── logs/                    # App logs (optional)
```

Requirements:

- `python3.dll` must be present in the working directory (line 1 of the conf file).
- The entry script and dependencies must be reachable from that working directory.

## Real-World Example: [HTTP Requester](https://github.com/yinkaisheng/http-requester)

[HTTP Requester](https://github.com/yinkaisheng/http-requester) is a lightweight desktop HTTP client (Python + PyQt5) whose Windows portable release is packaged with this launcher. Each release ships as a ~18 MB 7z archive with a self-contained Python 3.11 runtime—extract and run `HttpRequester.exe` with no system Python or `pip` install required.

```
HttpRequester/
├── HttpRequester.exe          # PythonCaller, renamed
├── HttpRequester.exe.conf       → RuntimeX64 + Lib\http-requester\main.pyc
├── RuntimeX64/                  # Python 3.11 embeddable runtime
├── config/                      # Session, history, and settings
└── logs/
```

The conf file matches the example above. Running `HttpRequester.exe` (with optional CLI args) starts the Python app through `Py_Main` without a system-wide Python installation.

## Notes

- The launcher exits with `-1` if the `.conf` file is missing.
- The launcher can be started from any working directory (e.g. `D:\> HttpRequester\HttpRequester.exe`); conf paths are resolved against the executable's directory, and `Py_Main` receives the absolute launcher path as `argv[0]`.
- `python3.dll` is loaded by full path from the configured working directory.
- Build platform (x86/x64) must match the embedded Python runtime.
