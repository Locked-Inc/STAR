#!/bin/sh
# Generate en_US.UTF-8 locale if not present
if ! locale -a 2>/dev/null | grep -q "en_US.utf8"; then
    mkdir -p /usr/lib/locale
    localedef -i en_US -f UTF-8 en_US.UTF-8
fi
