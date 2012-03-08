#!/bin/sh

wc -l `find . '!' '(' -path './target/' -or -regex '.*x86emu.*'  ')' -and -iregex '\(.*\.c\|.*\.h\|.*\.S\)'`
