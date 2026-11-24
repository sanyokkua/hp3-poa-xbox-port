#!/usr/bin/env python3
"""Check that a default.xbe is the Harry Potter and the Prisoner of Azkaban USA build this port is made for.

Every section's raw size and the SHA-1 digest stored in its header must match the reference. Region and
media patches (extract-xiso applies one by default) change the certificate and a few .text bytes but leave the stored digests alone, so they still pass; the
script says whether .text is pristine (its data matches its stored digest) or patched (this disc's image is pristine).
The other sections' data must match their stored digests too.
Exit 0 = supported, 5 = Harry Potter, another build or region, 4 = another game, 2 = not an XBE.
    python3 -I mac/xbe_check.py <default.xbe>     (also imported by mac/import_disc.py: check(data))"""
import hashlib, struct, sys

TITLE_ID = 0x4541004C
REF = [  # (name, raw size, stored digest), from the USA default.xbe (SHA-1 51b09e01..., title 0x4541004C, XDK 5849)
    (".text", 2440292, "f76df48bd188016af7b680c4670fb57bb7a91025"),
    ("D3D", 66216, "395bdf9d4ab0f257a2412ab9590700883d53254a"),
    ("D3DX", 117312, "606f7bfe268dbe763eae38a0a232bbf82426f9fd"),
    ("XGRPH", 132564, "29117ca106948b305fbda79c34432d108bb80c1b"),
    ("DSOUND", 36812, "e721f55dc3f5bec7c9a024b9d6c9e919c3288f61"),
    ("XPP", 32392, "538c3a92f9fec05201780601ab01d968f7bbd560"),
    (".rdata", 202516, "8559ae1bb0dcfe2d9f9acbf20d17b37ea8b848cd"),
    (".data", 86364, "a20e617cb50dc01d89439f203f8e750eeb42f019"),
    ("DOLBY", 29036, "c8c9142dccbd31bdca9a2d94e0acc47c2048d615"),
    ("$$XTINFO", 598, "0e87602c0cb073f348728addcd4254615379213c"),
    ("$$XTIMAGE", 10240, "685fb8f95e63c8cb8ee7462d89fb4e2a815bcd50"),
    ("$$XSIMAGE", 4096, "40b0b8b3f4a3716f67b88668de860a8f5c05b466"),
    (".XTLID", 1304, "046db41cee1ae83be488fc4323b60eb17adce3ae"),
]

def check(b):
    """(exit code, message) for the bytes of a default.xbe."""
    if b[:4] != b"XBEH":
        return 2, "not an XBE"
    try:
        base, = struct.unpack_from("<I", b, 0x104)
        cert, = struct.unpack_from("<I", b, 0x118)
        count, at = struct.unpack_from("<II", b, 0x11C)
        tid, = struct.unpack_from("<I", b, cert - base + 8)
    except struct.error:
        return 2, "not an XBE"
    if tid != TITLE_ID:
        return 4, f"title {tid:#x}: not Harry Potter Prisoner of Azkaban"
    if count != len(REF):
        return 5, f"Harry Potter Prisoner of Azkaban with {count} sections: another build or region (only the USA disc is supported; PAL is not yet)"
    text_pristine = True
    for i, (name, size, digest) in enumerate(REF):
        o = at - base + i * 56
        raw, rsz = struct.unpack_from("<II", b, o + 12)
        stored = b[o + 36:o + 56]
        if rsz != size or stored.hex() != digest:
            return 5, f"section {name} differs: another build or region (only the USA disc is supported; PAL is not yet)"
        same = hashlib.sha1(struct.pack("<I", rsz) + b[raw:raw + rsz]).digest() == stored
        if name == ".text":
            text_pristine = same
        elif not same:
            return 5, f"section {name}'s data does not match its digest: a modified executable"
    return 0, f"Harry Potter Prisoner of Azkaban USA ({'pristine' if text_pristine else 'media/region-patched'} .text)"


def main(path):
    code, msg = check(open(path, "rb").read())
    print(f"xbe_check: {'OK, ' if code == 0 else ''}{msg}")
    return code

if __name__ == "__main__":
    if len(sys.argv) != 2: sys.exit(__doc__)
    sys.exit(main(sys.argv[1]))
