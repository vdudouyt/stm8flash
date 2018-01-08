#!/bin/sh

autoreconf --install
aclocal
automake --add-missing --copy >/dev/null 2>&1
