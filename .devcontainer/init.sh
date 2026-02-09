#!/bin/sh
# Initialize configuration directories for Unix Dev Container
mkdir -p "$HOME/.ssh" "$HOME/.gemini" "$HOME/.claude" "$HOME/.config/gh" "$HOME/.config/opencode" "$HOME/.config/openai"
touch "$HOME/.gitconfig"
# Don't touch .claude.json - let Claude manage it inside .claude directory
