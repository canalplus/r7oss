#!/bin/sh
set -e

make ARCH=sh CONFIG_STB7100_MB411=y clean
make ARCH=sh CONFIG_STB7100_MB411=y 
make ARCH=sh CONFIG_STB7100_MB411=y clean
make ARCH=sh CONFIG_STB7100_MB411=y CONFIG_STG_DEBUG=y
# 5202 shares the core code with 7100 so no need to test a debug build
# Currently the OS21 BSP package in the 4.2.1 toolset is missing the
# HDMI interrupt defines, so only uncomment this if you have this patched
#make ARCH=sh CONFIG_STI5202_MB602=y clean
#make ARCH=sh CONFIG_STI5202_MB602=y 
make ARCH=sh CONFIG_STI7111_MB618=y clean
make ARCH=sh CONFIG_STI7111_MB618=y
make ARCH=sh CONFIG_STI7111_MB618=y clean
make ARCH=sh CONFIG_STI7111_MB618=y CONFIG_STG_DEBUG=y
# 7141 shares the core code with 7111 so no need to test a debug build
make ARCH=sh CONFIG_STI7141_MB628=y clean
make ARCH=sh CONFIG_STI7141_MB628=y
make ARCH=sh CONFIG_STI7200_MB671=y clean
make ARCH=sh CONFIG_STI7200_MB671=y
make ARCH=sh CONFIG_STI7200_MB671=y clean
make ARCH=sh CONFIG_STI7200_MB671=y CONFIG_STG_DEBUG=y
make ARCH=sh CONFIG_STI7105_MB680=y clean
make ARCH=sh CONFIG_STI7105_MB680=y
make ARCH=sh CONFIG_STI7105_MB680=y clean
make ARCH=sh CONFIG_STI7105_MB680=y CONFIG_STG_DEBUG=y
make ARCH=sh CONFIG_STI7106_MB680=y clean
make ARCH=sh CONFIG_STI7106_MB680=y
make ARCH=sh CONFIG_STI7106_MB680=y clean
make ARCH=sh CONFIG_STI7106_MB680=y CONFIG_STG_DEBUG=y
make ARCH=sh CONFIG_STI5289_MB796=y clean
make ARCH=sh CONFIG_STI5289_MB796=y
make ARCH=sh CONFIG_STI5289_MB796=y clean
make ARCH=sh CONFIG_STI5289_MB796=y CONFIG_STG_DEBUG=y
make ARCH=sh CONFIG_STI7108_MB837=y clean
make ARCH=sh CONFIG_STI7108_MB837=y
make ARCH=sh CONFIG_STI7108_MB837=y clean
make ARCH=sh CONFIG_STI7108_MB837=y CONFIG_STG_DEBUG=y

