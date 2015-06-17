#!/usr/bin/env bash

if [[ "$1" == "check" ]]; then
    #checking changes made by Astyle (shall be provided in log file $2)
    [[ "$2" == "" ]] && exit -1;
    [[ `grep formatted $2 | cut -f2 -d" " | grep -v 0` != "" ]] && echo "NOK: Astyle formatted some files" && exit -1
    [[ `grep "not found" $2 | cut -f2 -d" " | grep -v 0` != "" ]] && echo "NOK: Astyle version not found" && exit -1
    echo "OK: Astyle did not format any files"
    [[ `git status -s -uno` != "" ]] && echo "NOK: some files not committed" && exit -1
    exit 0
fi

astyleversion="2.04"
echo "StreamingEngine with Artistic style $astyleversion"

astyle=`astyle --version 2>&1 | grep $astyleversion`
[[ "$astyle" = "" ]] && echo "Failed: astyle $astyleversion not found; please install it and retry; check http://astyle.sourceforge.net/" && exit -1

#########
##common style options (C/C++ user and kernel)
commonstyle_options="
--min-conditional-indent=0
--max-instatement-indent=120
--pad-oper
--pad-header
--unpad-paren
--align-pointer=name
--keep-one-line-blocks
--keep-one-line-statements
--add-brackets
--break-after-logical
--mode=c
--lineend=linux
--preserve-date
"

#########
##player and components folders with all subfolders => sdk2 C/CPP-style
##for linux 'test' modules => use same style as for player
playerstyle_options="
--style=allman
--attach-extern-c
--indent=spaces=4
--convert-tabs
--max-code-length=200
$commonstyle_options
"
#following 2 options were applied once,
#but now removed as long as 'reasonable' empty line usage is done
#--break-blocks
#--delete-empty-lines

echo "######################################################################"
echo running astyle for player/ with $playerstyle_options
echo "######################################################################"
astyle -rvnQ $playerstyle_options                    \
    player/*.h player/*.c player/*.cpp

#########
##linux folder kernel drivers => sdk2 C-kernel-style
kernelstyle_options="
--style=linux
--indent=tab=8
--max-code-length=120
$commonstyle_options
"

echo "######################################################################"
echo running astyle for linux/ with $kernelstyle_options
echo "######################################################################"
astyle -vnQ $kernelstyle_options                                    \
    linux/drivers/osdev_abs/os*.h linux/drivers/osdev_abs/os*.c     \
    linux/drivers/mmelog/*.h linux/drivers/mmelog/*.c       \
    linux/drivers/strelay/*.h linux/drivers/strelay/*.c   \
    linux/drivers/ksound/*.h linux/drivers/ksound/*.c   \
    linux/drivers/player2/*.h linux/drivers/player2/*.c \
    linux/drivers/allocator/*.h linux/drivers/allocator/*.c \
    linux/drivers/h264_preprocessor/*.h \
    linux/drivers/h264_preprocessor/*.c \
    linux/drivers/hevc_preprocessor/*.h \
    linux/drivers/hevc_preprocessor/*.c \
    linux/drivers/vp8_hard_host_transformer/inc/*.h \
    linux/drivers/vp8_hard_host_transformer/*.h \
    linux/drivers/vp8_hard_host_transformer/*.c \
    linux/drivers/hevc_hard_host_transformer/*.h \
    linux/drivers/hevc_hard_host_transformer/*.c \
    linux/drivers/h264_encode_hard_host_transformer/*.h \
    linux/drivers/h264_encode_hard_host_transformer/*.c

#TODO: apply style for other modules
#linux/drivers/hevc_hard_host_transformer/CRC         (no: external import)
#linux/drivers/hevc_hard_host_transformer/DUMPER      (no: external import)
#linux/drivers/hevc_hard_host_transformer/HADES_API   (no: external import)
#linux/drivers/hevc_hard_host_transformer/MEMTEST     (no: external import)
#linux/drivers/hevc_hard_host_transformer/STHORM      (no: external import)

#########
##unittests folders with all subfolders
unittestsstyle_options="
--style=google
--attach-extern-c
--indent=spaces=4
--convert-tabs
--max-code-length=200
$commonstyle_options
"
echo "######################################################################"
echo running astyle for unittests/ with $unittestsstyle_options
echo "######################################################################"
astyle -rvnQ $unittestsstyle_options\
    unittests/*.h unittests/*.cpp unittests/*.c

#########
##enforce chmod644
echo "######################################################################"
echo enforcing chmod644 on headers and sources
echo "######################################################################"
find . -name "*\.h" | xargs chmod 644
find . -name "*\.c" | xargs chmod 644
find . -name "*\.cpp" | xargs chmod 644

true
