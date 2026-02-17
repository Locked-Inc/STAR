#!/bin/sh
# Initialize configuration directories for Unix Dev Container
# Creates directories that will be bind-mounted into the container.
# This prevents Docker errors when the host directories don't exist.
mkdir -p "$HOME/.ssh" "$HOME/.config/gh" "$HOME/.config/opencode" "$HOME/.config/openai"
touch "$HOME/.gitconfig"
