#!/bin/sh

wc -l `find . '!' '(' -path './obj/' -or -regex '.*x86emu.*' -or -regex ".*lwip.*" -or -regex ".*trash.*" ')' -and -iregex '\(.*\.c\|.*\.h\|.*\.S\)'`
