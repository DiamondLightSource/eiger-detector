# eiger-detector

Data acquisition framework for the Eiger detector consisting of:

- eigerfan: a fan-out of the Eiger zeromq push/pull stream
- EigerMetaWriter: A plugin for the odin-data meta_writer application
- EigerProcessPlugin: A plugin for the odin-data FrameProcessor
- EigerFrameDecoder: A plugin for the odin-data FrameReceiver

# Development

A devcontainer is provided for simpler local development. To get started:

1. Open the project in VSCode and re-open in devcontainer when prompted, or
   open manually with `Dev Containers: Reopen in Container`
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

To run the tickit simulation, add `tickit-devices` to the workspace, pip install, and
run the `Eiger` launch config.

To run the IOC, add fastcs-eiger to the workspace, pip install and run the `Eiger IOC`
launch config, which talks to the tickit sim by default. The generated `output.bob` can
then be opened in Phoebus for GUI control.

Currently in development is fastcs-odin, which creates an EPICS IOC to control the odin
processes. To run this, add fastcs-odin to the workspace, pip install, and then run the
`Odin IOC` launch config, which by default talks to a control server at 127.0.0.1:8888.

To debug any of the applications in the full deployment, stop that process and run the
launch config to run an equivalent process in the debugger. For example the Odin Control
launch config uses the same config file as the instance in the full deployment.