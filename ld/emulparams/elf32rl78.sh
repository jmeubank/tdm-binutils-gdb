MACHINE=
SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-rl78"
# See also `include/elf/rl78.h'
TEXT_START_ADDR=0x00000
ARCH=rl78
ENTRY=_start
EMBEDDED=yes
TEMPLATE_NAME=elf
ELFSIZE=32
# EXTRA_EM_FILE=needrelax
MAXPAGESIZE=256

STACK_ADDR="${CREATE_SHLIB-(DEFINED(__stack) ? __stack : 0xffedc)}"
STACK_SENTINEL="LONG(0xdead)"
OTHER_SYMBOLS="${CREATE_SHLIB-PROVIDE (__rl78_abs__ = 0);}"
