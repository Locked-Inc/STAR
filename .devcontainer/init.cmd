@echo off
rem Initialize configuration directories for Windows Dev Container
rem Creates directories that will be bind-mounted into the container.
rem This prevents Docker errors when the host directories don't exist.
set "BASE_DIR=%USERPROFILE%"
if defined HOME set "BASE_DIR=%HOME%"

if not exist "%BASE_DIR%\.ssh" mkdir "%BASE_DIR%\.ssh"
if not exist "%BASE_DIR%\.config\gh" mkdir "%BASE_DIR%\.config\gh"
if not exist "%BASE_DIR%\.config\opencode" mkdir "%BASE_DIR%\.config\opencode"
if not exist "%BASE_DIR%\.config\openai" mkdir "%BASE_DIR%\.config\openai"
if not exist "%BASE_DIR%\.gitconfig" type nul >> "%BASE_DIR%\.gitconfig"

exit /b 0
