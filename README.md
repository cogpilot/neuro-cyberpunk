# <img src="Assets/icon.png" width="29" style="vertical-align:middle;"> Cyberpunk Neuro Integration

RED4ext plugin that exposes Neuro-sama bindings inside Cyberpunk 2077.

## Requirements

------------

- Windows x64, Cyberpunk 2077 installed.
- Visual Studio 2022 build tools (MSVC + Windows 10/11 SDK).
- [xmake](https://xmake.io/) installed and on your PATH.
- Git submodules fetched: `git submodule update --init --recursive`.
- `CP2077_PATH` environment variable pointing to your Cyberpunk 2077 install (e.g. `C:\Games\Cyberpunk 2077`).
- Neuro Agent running locally (e.g. [neuro-api-tony](https://github.com/Pasu4/neuro-api-tony)).
- Environment variable `NEURO_SDK_WS_URL` pointing to the Neuro Agent (example value `ws://localhost:8000`).

## Build

------------

```
xmake
```

This builds `NeuroInteractions.dll` in `build/`. Be sure to run `xmake package` to prepare the packaged mod folders (`packaging/` and `packaging_pdb/`).

## Install / Run

------------

- Copy manually: take the contents of `packaging/` and drop them into `CP2077_PATH`.
- Or let xmake install for you (uses `CP2077_PATH`):  

  ```
  xmake install
  ```

*note that xmake install does not copy across scripts currently, you will need to manually copy those over yourself*

- Launch the game (or `xmake run` if `CP2077_PATH` is set) and enable the plugin under RED4ext.

## Packaging for sharing

------------

`xmake package` refreshes `packaging/` (release drop-in) and `packaging_pdb/` (with PDBs for debugging).
