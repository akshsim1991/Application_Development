import sys
def decode(buf):
    P=bytearray(buf); BS=0x10000
    for base in range(0,len(P),BS):
        size=min(BS,len(P)-base)
        if size<5: continue
        size-=4; i=0
        while i<size:
            if P[base+i] in (0xE8,0xE9):
                i+=1
                j=base+i
                if P[j+3] in (0x00,0xFF):
                    addr=(base+i+4)&0xFFFFFF
                    rel=P[j]|(P[j+1]<<8)|(P[j+2]<<16)
                    rel=(rel-addr)&0xFFFFFFFF
                    if rel&0x800000: P[j+3]^=0xFF
                    P[j]=rel&0xFF; P[j+1]=(rel>>8)&0xFF; P[j+2]=(rel>>16)&0xFF
                i+=4
            else: i+=1
    return bytes(P)
d=open(sys.argv[1],'rb').read(); open(sys.argv[2],'wb').write(decode(d))
