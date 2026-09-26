# Protocol+ Auto Clicker

A simple native Windows auto-clicker using CPS (clicks per second).

## Features
- CPS control from 1–1000
- Left, right, or middle mouse button
- Global toggle hotkey
- Default hotkey: F6
- Click **Change** to bind another keyboard key
- Ctrl / Alt / Shift / Win modifiers are captured when held with the new key
- Blue Protocol+ interface
- Protocol+ application icon
- Native C++ Win32 application
- Windows installer with Start Menu and optional Desktop shortcut
- No Python required

## Build
GitHub Actions sets up MSVC, compiles the app and icon resource, builds the Inno Setup installer, and uploads the installer artifact.

## Hotkey
Click **Change** beside Toggle hotkey, then press the key you want. F6 is the default. If the selected combination is already registered, the previous hotkey is restored.

## License
MIT
