#!/bin/sh
set -e
FIRMWARE_HOME=/opt/STM/STLinux-2.3/devkit/sh4/target/lib/firmware

rm -rf board_tests

mkdir -p board_tests/mb618
make ARCH=sh CONFIG_STI7111_MB618=y clean
make ARCH=sh CONFIG_STI7111_MB618=y
sh4strip -o board_tests/mb618/interlaced_display.out tests/interlaced_display/interlaced_display.out
sh4strip -o board_tests/mb618/progressive_display.out tests/progressive_display/progressive_display.out
cp $FIRMWARE_HOME/component_7111_mb618.fw board_tests/mb618/component.fw

mkdir -p board_tests/mb628
make ARCH=sh CONFIG_STI7141_MB628=y clean
make ARCH=sh CONFIG_STI7141_MB628=y
sh4strip -o board_tests/mb628/interlaced_display.out tests/interlaced_display/interlaced_display.out
sh4strip -o board_tests/mb628/progressive_display.out tests/progressive_display/progressive_display.out
cp $FIRMWARE_HOME/component_7141_mb628.fw board_tests/mb628/component.fw

mkdir -p board_tests/mb671
make ARCH=sh CONFIG_STI7200_MB671=y clean
make ARCH=sh CONFIG_STI7200_MB671=y
sh4strip -o board_tests/mb671/interlaced_display.out tests/interlaced_display/interlaced_display.out
sh4strip -o board_tests/mb671/progressive_display.out tests/progressive_display/progressive_display.out
cp $FIRMWARE_HOME/component_7200_mb671.fw board_tests/mb671/component.fw

mkdir -p board_tests/mb680
make ARCH=sh CONFIG_STI7105_MB680=y clean
make ARCH=sh CONFIG_STI7105_MB680=y
sh4strip -o board_tests/mb680/interlaced_display.out tests/interlaced_display/interlaced_display.out
sh4strip -o board_tests/mb680/progressive_display.out tests/progressive_display/progressive_display.out
cp $FIRMWARE_HOME/component_7105_mb680.fw board_tests/mb680/component.fw

mkdir -p board_tests/sdk7105
make ARCH=sh CONFIG_STI7105_SDK7105=y clean
make ARCH=sh CONFIG_STI7105_SDK7105=y
sh4strip -o board_tests/sdk7105/interlaced_display.out tests/interlaced_display/interlaced_display.out
sh4strip -o board_tests/sdk7105/progressive_display.out tests/progressive_display/progressive_display.out
cp $FIRMWARE_HOME/component_7105_pdk7105.fw board_tests/sdk7105/component.fw

mkdir -p board_tests/mb796
make ARCH=sh CONFIG_STI5289_MB796=y clean
make ARCH=sh CONFIG_STI5289_MB796=y
sh4strip -o board_tests/mb796/interlaced_display.out tests/interlaced_display/interlaced_display.out
sh4strip -o board_tests/mb796/progressive_display.out tests/progressive_display/progressive_display.out
cp $FIRMWARE_HOME/component_7105_mb680.fw board_tests/mb796/component.fw


