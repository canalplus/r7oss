# Note: A number of encodings are handled with purely algorithmic converters,
# without any mapping tables:
# US-ASCII, ISO 8859-1, UTF-7/8/16/32, SCSU

# Listed here:

# * ISO 8859-2..8,10,13,14,15,16
#   - 8859-11 table is not included. It's rather treated as a synonym of
#     Windows-874
# * Windows-125[0-8]
# * Simplified Chinese : GBK(Windows cp936), GB 18030
#   - GB2312 table was removed and 4 aliases for GB2312 were added
#     to GBK in convrtrs.txt to treat GB2312 as a synonym of GBK.
#   - GB-HZ is supported now that it uses the GBK table.
# * Traditional Chinese : Big5
# * Japanese : SJIS (shift_jis-html), EUC-JP (euc-jp-html)
# * Korean : EUC-KR per the encoding spec
# * Thai : Windows-874
#   - TIS-620 and ISO-8859-11 are treated as synonyms of Windows-874
#     although they're not the same.
# * Mac encodings : MacRoman, MacCyrillic
# * Cyrillic : KOI8-R, KOI8-U, IBM-866
#
# * Missing
#  - Armenian, Georgian  : extremly rare
#  - Mac encodings (other than Roman and Cyrillic) : extremly rare

UCM_SOURCE_FILES=


UCM_SOURCE_CORE=


# Do not build EBCDIC converters.
# ibm-37 and ibm-1047 are hardcoded in Makefile.in and
# they're removed by modifying the file. It's also hard-coded in makedata.mak for
# Winwodws, but we don't have to touch it because the data dll is generated out of
# icu*.dat file generated on Linux.
UCM_SOURCE_EBCDIC =
