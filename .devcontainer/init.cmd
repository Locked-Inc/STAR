@echo off
rem Initialize configuration directories for Windows Dev Container
set "BASE_DIR=%USERPROFILE%"
if defined HOME set "BASE_DIR=%HOME%"

if not exist "%BASE_DIR%\.ssh" mkdir "%BASE_DIR%\.ssh"
if not exist "%BASE_DIR%\.gemini" mkdir "%BASE_DIR%\.gemini"
if not exist "%BASE_DIR%\.claude" mkdir "%BASE_DIR%\.claude"
if not exist "%BASE_DIR%\.config\gh" mkdir "%BASE_DIR%\.config\gh"
if not exist "%BASE_DIR%\.config\opencode" mkdir "%BASE_DIR%\.config\opencode"
if not exist "%BASE_DIR%\.config\openai" mkdir "%BASE_DIR%\.config\openai"
if not exist "%BASE_DIR%\.gitconfig" type nul >> "%BASE_DIR%\.gitconfig"
rem Don't create .claude.json - let Claude manage it inside .claude directory

exit /b 0
