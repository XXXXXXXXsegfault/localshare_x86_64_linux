#!/bin/sh -ev
mkdir -p tmp
bin/scpp src/main.c tmp/cc.i
bin/scc tmp/cc.i tmp/cc.asm
bin/asm tmp/cc.asm localshare
