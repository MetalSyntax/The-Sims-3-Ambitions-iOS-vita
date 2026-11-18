#!/usr/bin/env python3
"""MainWindow.nib (NSKeyedArchiver binario de iPhone OS 2/3, o NIBArchive de iOS 5+) -> nib.txt para uikit.c.

Solo el patrón de las plantillas de Xcode de la época: File's Owner (UIApplication) -> delegate,
delegate.window -> UIWindow, delegate.<outlet> -> vista (UIClassSwapper, p. ej. EAGLView).
Salida (ux0:data/<slug>/nib.txt):
    delegate <Clase>
    window <w> <h>
    view <Clase> <w> <h>
    outlet <origen> <label> <destino>      (roles: owner delegate window view)
Uso: python3 -I nib_prep.py <MainWindow.nib> <nib.txt>
"""
import os, plistlib, re, struct, sys


def load_nibarchive(b):
    """NIBArchive (iOS 5+) -> lista $objects como la de NSKeyedArchiver (refs = plistlib.UID, NSString = str)."""
    n = struct.unpack_from('<10I', b, 10)
    nobj, oobj, nkey, okey, nval, oval, ncls, ocls = n[2:10]

    def varint(off):
        r = sh = 0
        while True:
            c = b[off]; off += 1
            r |= (c & 0x7F) << sh; sh += 7
            if c & 0x80:
                return r, off

    off, keys = okey, []
    for _ in range(nkey):
        l, off = varint(off); keys.append(b[off:off + l].decode('utf-8', 'replace')); off += l
    off, classes = ocls, []
    for _ in range(ncls):
        l, off = varint(off); extra, off = varint(off); off += 4 * extra
        classes.append(b[off:off + l].rstrip(b'\0').decode()); off += l
    off, vals = oval, []
    for _ in range(nval):
        k, off = varint(off); t = b[off]; off += 1
        if t in (0, 1, 2, 3):
            sz = (1, 2, 4, 8)[t]; v = int.from_bytes(b[off:off + sz], 'little', signed=True); off += sz
        elif t in (4, 5):
            v = t == 4
        elif t == 6:
            v = struct.unpack_from('<f', b, off)[0]; off += 4
        elif t == 7:
            v = struct.unpack_from('<d', b, off)[0]; off += 8
        elif t == 8:
            l, off = varint(off); v = b[off:off + l]; off += l
        elif t == 9:
            v = None
        elif t == 10:
            v = plistlib.UID(struct.unpack_from('<I', b, off)[0]); off += 4
        else:
            raise ValueError('NIBArchive: tipo de valor %d' % t)
        vals.append((keys[k], v))
    off, objs = oobj, []
    for _ in range(nobj):
        c, off = varint(off); vs, off = varint(off); vc, off = varint(off)
        d = {'$class': {'$classname': classes[c]}}
        for kk, v in vals[vs:vs + vc]:
            d[kk] = v
        objs.append(d)
    for i, d in enumerate(objs):  # NSString -> str (lo que espera R())
        if d['$class']['$classname'] in ('NSString', 'NSMutableString', 'NSLocalizableString'):
            objs[i] = (d.get('NS.bytes') or b'').decode('utf-8', 'replace')
    return objs


def main():
    src, out = sys.argv[1:3]
    if os.path.isdir(src):  # SDK 6+: X.nib/ con runtime.nib (y objects-8.0+.nib) dentro
        src = next(os.path.join(src, n) for n in ('runtime.nib', 'objects-8.0+.nib', 'objects.nib')
                   if os.path.isfile(os.path.join(src, n)))
    raw = open(src, 'rb').read()
    o = load_nibarchive(raw) if raw[:10] == b'NIBArchive' else plistlib.load(open(src, 'rb'))['$objects']

    def R(x):
        return o[x.data] if isinstance(x, plistlib.UID) else x

    def cls(obj):
        return R(obj['$class'])['$classname'] if isinstance(obj, dict) and '$class' in obj else None

    def size(obj):
        bnd = obj.get('UIBounds')
        if isinstance(bnd, (bytes, bytearray)) and len(bnd) >= 16:  # NIBArchive: tipo + 4 float/double
            f = struct.unpack_from('<4d', bnd, len(bnd) - 32) if len(bnd) >= 33 else struct.unpack_from('<4f', bnd, len(bnd) - 16)
            return (float(f[2]), float(f[3]))
        m = re.match(r'\{\{.*?\}, \{([\d.]+), ([\d.]+)\}\}', R(obj.get('UIBounds', '{{0, 0}, {320, 480}}')))
        return (float(m.group(1)), float(m.group(2))) if m else (320.0, 480.0)

    role = {}
    lines = []
    for i, obj in enumerate(o):
        c = cls(obj)
        if c == 'UIProxyObject' and R(obj.get('UIProxiedObjectIdentifier')) == 'IBFilesOwner':
            role[i] = 'owner'
        elif c == 'UIClassSwapper' and R(obj.get('UIOriginalClassName')) == 'UICustomObject':
            role[i] = 'delegate'
            lines.append('delegate %s' % R(obj['UIClassName']))
        elif c == 'UIWindow':
            role[i] = 'window'
            lines.append('window %g %g' % size(obj))
        elif c == 'UIClassSwapper' and R(obj.get('UIOriginalClassName')) in ('UIView', 'UIImageView'):
            role[i] = 'view'
            lines.append('view %s %g %g' % ((R(obj['UIClassName']),) + size(obj)))
    for obj in o:
        if cls(obj) == 'UIRuntimeOutletConnection':
            s, d = obj['UISource'].data, obj['UIDestination'].data
            if s in role and d in role:
                lines.append('outlet %s %s %s' % (role[s], R(obj['UILabel']), role[d]))
    # el orden importa: delegate y window antes que la vista (se agrega a la ventana)
    order = {'delegate': 0, 'window': 1, 'view': 2, 'outlet': 3}
    lines.sort(key=lambda l: order[l.split()[0]])
    open(out, 'w').write('\n'.join(lines) + '\n')
    print('\n'.join(lines))


if __name__ == '__main__':
    main()
