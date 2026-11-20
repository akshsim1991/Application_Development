#!/usr/bin/env python3
"""Embeds ui/index.html into build/ui_blob.cpp (usage: embed.py in.html out.cpp)."""
import sys
data = open(sys.argv[1], 'rb').read()
with open(sys.argv[2], 'w') as f:
    f.write('extern const unsigned char kUiHtml[];\nextern const unsigned long kUiHtmlSize;\n')
    f.write('const unsigned char kUiHtml[] = {\n')
    for i in range(0, len(data), 24):
        f.write(','.join(str(b) for b in data[i:i+24]) + ',\n')
    f.write('0};\nconst unsigned long kUiHtmlSize = %d;\n' % len(data))
