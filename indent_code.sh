#!/bin/bash
#
# Author: Yang, Ying-chao@gmail.com, 2015-01-17
#

find . -name "*.h" -o -name "*.c" -exec \
     indent  -nbad -bap -bbo -nbc -br -brs -c33 -cd33 -ncdb -ce -ci4 -cli0 \
     -cp33 -cs -d0 -di1 -nfc1 -nfca -hnl -i4 -ip0 -l75 -lp -npcs  -nprs -npsl \
     -saf -sai -saw -nsc -nsob -nss -nut {} \;
