# PYNQ Yocto Build Environment

This directory contains the necessary scripts and configuration to build a custom PYNQ image for the PYNQ-Z2 board with ROS support.

## Build Process Overview

The build process uses the Yocto Project to create a custom Linux distribution for the PYNQ-Z2. The general workflow is as follows:

1.  **Setup Build Host:** Prepare a Linux build machine (or use Docker) with all the necessary dependencies as described in `build-server-setup.md`.

2.  **Initialize Git Repository:** The `repo` tool requires the build directory to be a git repository to use a local manifest. From within the `pynq-image-build` directory, run:
    ```bash
    git init
    git add .
    git commit -m "Initial commit"
    ```

3.  **Fetch Sources:** Use the `repo` tool to download all the required Yocto layers. This is done by the `setup_build_env.sh` script.

4.  **Configure Build:** The `build.sh` script will configure the build by setting the target machine to `pynq-z2` and adding the necessary layers and packages (including ROS) to the build configuration.

5.  **Run Build:** The `build.sh` script will then run the Yocto build process using `bitbake`. This will compile the entire Linux system from source, which can take several hours.

6.  **Deploy Image:** The output of the build will be an SD card image (`.wic` file) that can be flashed to an SD card and used to boot the PYNQ-Z2.

## File Descriptions

-   `build-server-setup.md`: Instructions for setting up a build host.
-   `setup_build_env.sh`: Script to initialize the build environment by fetching all the required source code using `repo`.
-   `manifest.xml`: A manifest file for the `repo` tool that specifies which git repositories to fetch.
-   `build.sh`: The main script that configures and starts the Yocto build.
-   `README.md`: This file.
