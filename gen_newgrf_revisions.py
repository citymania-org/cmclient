#!/usr/bin/env python3

from urllib.request import urlopen

verstr = urlopen("http://finger.openttd.org/tags.txt").read()
verstr = verstr.decode('utf-8')
verdata = verstr.strip().split()

releases = []
f = open("src/newgrf_revisions.hpp", "w")
# f.write('#include "stdafx.h"\n\n')
f.write('#include <map>\n')

f.write('const std::map<std::string, uint32> OPENTTD_NEWGRF_VERSIONS = {\n')
for rev, ver in zip(map(int, verdata[0::3]), verdata[2::3]):
    if ver.startswith('1.1'):
        break
    v = ver.split('.')
    vr = None
    if '-' in v[2]:
        v[2], vr = v[2].split('-')
    v = list(map(int, v))
    release_flag = int(not bool(vr))
    newgrf_rev = (v[0] << 28 | v[1] << 24 | v[2] << 20 |
                  release_flag << 19 | (rev & ((1 << 19) - 1)))
    f.write('    {{"{}", {}}},\n'.format(ver, newgrf_rev))

    if release_flag and v[2] == 0:
        newgrf_rev = v[0] << 28 | (v[1] + 1) << 24
        releases.append((rev, newgrf_rev))

f.write('};\n\n')

f.write('const std::map<uint32, uint32> OPENTTD_RELEASE_REVISIONS = {\n')
for rev, newgrf_rev in releases:
    f.write('    {{{}, {}}},\n'.format(rev, newgrf_rev))
f.write('};\n')
