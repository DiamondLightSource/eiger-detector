# eiger-detector

Data acquisition framework for the Eiger detector using [odin-data] consisting of:

- eigerfan: a fan-out of the Eiger zeromq push/pull stream
- EigerMetaWriter: A plugin for the odin-data meta_writer application
- EigerProcessPlugin: A plugin for the odin-data FrameProcessor
- EigerFrameDecoder: A plugin for the odin-data FrameReceiver

## Development

A VSCode [devcontainer] is provided for simpler local development. `odin-data` is
pre-installed in `/odin` and there are vscode settings to build `eiger-detector` against
this. A virtual environment is available at `/venv` and will be the default environment.
Follow the steps below to get started.

Commands starting with `>` are vscode commands and can be run with `Ctrl+Shift+P`,
while commands starting with `$` are bash commands to be run in a terminal (inside the
vscode devcontainer). Within the devcontainer `cmake` will be available on the PATH, so
make sure vscode user settings have `cmake.cmakePath` set to `cmake` (the default).

1. Open the project in VSCode and re-open in devcontainer when prompted, or
   open manually with `> Dev Containers: Reopen in Container`
2. Install the recommended extensions for the workspace
    (`> Extensions: Show Recommended Extensions`)
3. Build eiger-detector

    i. `> CMake: Delete Cache and Reconfigure` (select gcc version in dropdown)

    ii. `> CMake: Install`

4. Install the eiger-detector python package with dev dependencies and tickit simulator

    ii. `$ pip install -e './python[dev,sim]'`

5. Run the dev deployment from the `deploy` directory with `$ zellij -l layout.kdl`

It is then possible to edit the python applications and restart, or edit the C++
applications, rebuild and restart.

To debug any of the applications in the full deployment, stop that process and run the
VSCode [launch config](launch-config) to run an equivalent process in the debugger. For
example the Odin Control launch config uses the same config file as the instance in the
full deployment.

## Related Projects

- [fastcs-odin]: An EPICS driver to control the odin-data / eiger-detector applications
- [fastcs-eiger]: An EPICS driver to control Eiger detectors
- [tickit-devices]: A set of simulated devices, including Eiger

[odin-data]: https://github.com/odin-detector/odin-data
[fastcs-odin]: https://github.com/DiamondLightSource/fastcs-odin
[fastcs-eiger]: https://github.com/DiamondLightSource/fastcs-eiger
[tickit-devices]: https://github.com/DiamondLightSource/tickit-devices
[devcontainer]: https://code.visualstudio.com/docs/devcontainers/containers
[launch-config]: https://code.visualstudio.com/docs/editor/debugging
