#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
import glob

PREFIX='''#ifndef _GENERATED_H_
#define _GENERATED_H_

// #ifdef __cplusplus
// extern "C" {
// #endif

'''

SUFIX='''
    }

    return tbl;
}

// #ifdef __cplusplus
// }
// #endif

#endif /* _GENERATED_H_ */

'''

MIDDLE='''

hash_table* collect_handlers()
{
    hash_table* tbl = hash_table_create(64, NULL);
    if (tbl)
    {
'''

if __name__ == '__main__':
    print("Scanning protocol handlers...\n")

    fd = open('protocols.h', 'w')
    defs  = []
    stats = []

    for fn in glob.glob('protocols/*'):
        protocol = os.path.basename(fn)
        print("\tAdding %s...\n"%protocol)

        defs.append('extern mget_err process_%s_request(dinfo*, dp_callback, bool*, mget_option*, void*);'%protocol)
        stats.append('        hash_table_insert(tbl, "%s", process_%s_request, sizeof(void*));'%(protocol, protocol))

    fd.write(PREFIX)
    fd.write('\n'.join(defs))
    fd.write(MIDDLE)
    fd.write('\n'.join(stats))
    fd.write(SUFIX)
    fd.close()

    print("Done...\n")
