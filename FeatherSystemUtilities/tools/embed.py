#!/usr/bin/env python3
"""Embeds a file as a C array (usage: embed.py in out.cpp [symbol]; default symbol kUiHtml)."""
import sys
sym = sys.argv[3] if len(sys.argv) > 3 else 'kUiHtml'
data = open(sys.argv[1], 'rb').read()
with open(sys.argv[2], 'w') as f:
    f.write('extern const unsigned char %s[];\nextern const unsigned long %sSize;\n' % (sym, sym))
    f.write('const unsigned char %s[] = {\n' % sym)
    for i in range(0, len(data), 24):
        f.write(','.join(str(b) for b in data[i:i+24]) + ',\n')
    f.write('0};\nconst unsigned long %sSize = %d;\n' % (sym, len(data)))
