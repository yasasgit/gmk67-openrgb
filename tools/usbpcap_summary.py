import struct, sys, collections

def blocks(path):
    with open(path, 'rb') as f:
        while True:
            h = f.read(8)
            if len(h) < 8: return
            t, l = struct.unpack('<II', h)
            body = f.read(l - 8)
            yield t, body[:-4]

def packets(path):
    for t, b in blocks(path):
        if t == 6:  # EPB
            iface, th, tl, cap, orig = struct.unpack('<IIIII', b[:20])
            yield ((th << 32) | tl) / 1e6, b[20:20 + cap]

mode = sys.argv[2] if len(sys.argv) > 2 else 'summary'
stats = collections.Counter()
n = 0
for ts, p in packets(sys.argv[1]):
    n += 1
    hl, irp, st, fn, info, bus, dev, ep, xfer, dlen = struct.unpack('<HQIHBHHBBI', p[:27])
    data = p[hl:]
    out = info & 1 == 0
    stats[(bus, dev, ep & 0x7f, 'IN' if ep & 0x80 else 'OUT', xfer, 'h2d' if out else 'd2h')] += 1
    if mode == 'ctrl' and xfer == 2 and hl >= 28:
        stage = p[27]
        if stage == 0 and len(data) >= 8:
            bm, br, wv, wi, wl = struct.unpack('<BBHHH', data[:8])
            if bm & 0x60 == 0x20:  # class request
                print(f"#{n} t={ts:.3f} dev={dev} bm={bm:02x} req={br:02x} wValue={wv:04x} wIndex={wi} wLen={wl} data={data[8:].hex()}")
        elif stage in (1, 2) and data and not out:
            print(f"#{n} t={ts:.3f} dev={dev} d2h stage={stage} data={data.hex()}")
    if mode == 'int' and xfer == 1 and out and data:
        print(f"#{n} t={ts:.3f} dev={dev} ep={ep:02x} OUT {data.hex()}")

if mode == 'summary':
    print('packets', n)
    for k, v in sorted(stats.items()):
        print('bus,dev,ep,dir,xfer,irpdir', k, v)
