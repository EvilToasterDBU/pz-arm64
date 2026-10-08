#!/usr/bin/env python3
"""pzgen - build the JNI binding tables for the universal shim (no compiler needed).

For every `lib*.so` found in the game directory that exports `Java_*` functions, find the Java `native` methods those
functions implement (by scanning the game's own class files) and write one table per library:

    <outdir>/tables/<libname>.tbl      X86LIB<TAB><x86 file>  then  class<TAB>method<TAB>descriptor<TAB>x86 symbol
    <outdir>/libs/<libname>             a private copy of the universal shim (distinct inode: the JVM treats
                                        symlinks to one file as the same library and would not call JNI_OnLoad again)
"""
import hashlib
import os
import shutil
import struct
import sys
import zipfile

SKIP_DIRS = {"media", "mods", "Workshop", "launcher", "license", "jre", "jre64", "jre32", "natives_unused"}
# libraries the tool provides as real arm64 builds instead of forwarding them to Box64
NATIVE_ARM64 = {"libjassimp64.so"}
# the Java side loads <key>; the x86 implementation is <value> (no OpenGL dependency); GL-only functions become no-ops
ALIASES = {"libPZBullet64.so": "libPZBulletNoOpenGL64.so"}


# ------------------------------------------------------------------ class files
def native_methods(data):
    """Return [(class, method, descriptor, is_static)] for all ACC_NATIVE methods of one class file."""
    if data[:4] != b"\xca\xfe\xba\xbe":
        return []
    cp_count = struct.unpack(">H", data[8:10])[0]
    pos, cp, i = 10, [None] * cp_count, 1
    while i < cp_count:
        tag = data[pos]
        if tag == 1:
            n = struct.unpack(">H", data[pos + 1:pos + 3])[0]
            cp[i] = data[pos + 3:pos + 3 + n]
            pos += 3 + n
        elif tag in (3, 4):
            pos += 5
        elif tag in (5, 6):
            pos += 9
            i += 1
        elif tag in (7, 8, 16, 19, 20):
            cp[i] = struct.unpack(">H", data[pos + 1:pos + 3])[0]
            pos += 3
        elif tag in (9, 10, 11, 12, 17, 18):
            pos += 5
        elif tag == 15:
            pos += 4
        else:
            return []
        i += 1
    _acc, this, _sup = struct.unpack(">HHH", data[pos:pos + 6])
    pos += 6
    ni = struct.unpack(">H", data[pos:pos + 2])[0]
    pos += 2 + 2 * ni

    def skip_attrs(p):
        n = struct.unpack(">H", data[p:p + 2])[0]
        p += 2
        for _ in range(n):
            p += 6 + struct.unpack(">I", data[p + 2:p + 6])[0]
        return p

    nf = struct.unpack(">H", data[pos:pos + 2])[0]
    pos += 2
    for _ in range(nf):
        pos = skip_attrs(pos + 6)
    nm = struct.unpack(">H", data[pos:pos + 2])[0]
    pos += 2
    out = []
    cname = cp[cp[this]].decode("utf-8", "replace")
    for _ in range(nm):
        acc, name, desc = struct.unpack(">HHH", data[pos:pos + 6])
        pos = skip_attrs(pos + 6)
        if acc & 0x0100:
            out.append((cname, cp[name].decode("utf-8", "replace"), cp[desc].decode("utf-8", "replace"), bool(acc & 8)))
    return out


def iter_class_data(inst, classpath):
    for entry in classpath:
        path = os.path.normpath(os.path.join(inst, entry))
        if os.path.isdir(path):
            for root, dirs, files in os.walk(path):
                if root == path:
                    dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
                for f in files:
                    if f.endswith(".class"):
                        with open(os.path.join(root, f), "rb") as fh:
                            yield fh.read()
        elif path.endswith(".jar") and os.path.isfile(path):
            with zipfile.ZipFile(path) as z:
                for n in z.namelist():
                    if n.endswith(".class"):
                        yield z.read(n)


# ------------------------------------------------------------------ ELF
def elf_exports(path):
    """Names of the defined dynamic symbols of an ELF64 little-endian shared object."""
    with open(path, "rb") as f:
        d = f.read()
    if d[:4] != b"\x7fELF" or d[4] != 2:
        return set()
    shoff = struct.unpack("<Q", d[0x28:0x30])[0]
    shentsize, shnum = struct.unpack("<HH", d[0x3A:0x3E])
    secs = []
    for i in range(shnum):
        o = shoff + i * shentsize
        name, typ, flags, addr, off, size, link, info, align, entsize = struct.unpack("<IIQQQQIIQQ", d[o:o + 64])
        secs.append((typ, off, size, link, entsize))
    out = set()
    for typ, off, size, link, entsize in secs:
        if typ != 11:                                   # SHT_DYNSYM
            continue
        stroff = secs[link][1]
        for k in range(size // (entsize or 24)):
            st_name, st_info, _o, st_shndx, _v, _s = struct.unpack("<IBBHQQ", d[off + k * 24:off + k * 24 + 24])
            if st_shndx != 0 and (st_info >> 4) in (1, 2):
                end = d.index(b"\0", stroff + st_name)
                out.add(d[stroff + st_name:end].decode("ascii", "replace"))
    return out


def mangle(s):
    r = []
    for ch in s:
        if ch == "/":
            r.append("_")
        elif ch == "_":
            r.append("_1")
        elif ch == ";":
            r.append("_2")
        elif ch == "[":
            r.append("_3")
        elif ch.isalnum() and ord(ch) < 128:
            r.append(ch)
        else:
            r.append("_0%04x" % ord(ch))
    return "".join(r)


# ------------------------------------------------------------------ cache key
def cache_key(inst, classpath, libdirs):
    h = hashlib.sha1()
    h.update(os.path.abspath(inst).encode())
    def add(p):
        try:
            st = os.stat(p)
            h.update(f"{p}:{st.st_size}:{int(st.st_mtime)}".encode())
        except OSError:
            pass
    add(os.path.join(inst, "ProjectZomboid64.json"))
    for e in classpath:
        p = os.path.join(inst, e)
        add(p)                                           # jar, or directory mtime
        if os.path.isdir(p):
            add(os.path.join(p, "zombie"))
    for d in libdirs:
        if os.path.isdir(d):
            for f in sorted(os.listdir(d)):
                if f.startswith("lib") and f.endswith(".so"):
                    add(os.path.join(d, f))
    return h.hexdigest()[:16]


# ------------------------------------------------------------------ main entry
def build(inst, classpath, outdir, shim_so, real_libs=()):
    """Create <outdir>/{tables,libs}. real_libs: extra prebuilt arm64 libraries to copy into libs/."""
    libdirs = [inst, os.path.join(inst, "linux64"), os.path.join(inst, "natives")]
    tables, libs = os.path.join(outdir, "tables"), os.path.join(outdir, "libs")
    os.makedirs(tables, exist_ok=True)
    os.makedirs(libs, exist_ok=True)

    natives = []
    for data in iter_class_data(inst, classpath):
        natives += native_methods(data)

    x86 = {}
    for d in libdirs:
        if not os.path.isdir(d):
            continue
        for f in sorted(os.listdir(d)):
            if f.startswith("lib") and f.endswith(".so") and f.count(".so") == 1 and f not in x86:
                x86[f] = (os.path.join(d, f), {s for s in elf_exports(os.path.join(d, f)) if s.startswith("Java_")})

    summary = {}
    for lib, (path, exports) in x86.items():
        if lib in NATIVE_ARM64:
            continue
        impl_name = ALIASES.get(lib, lib)
        impl_exports = x86[impl_name][1] if impl_name in x86 else exports
        rows = []
        for cls, name, desc, _static in natives:
            short = "Java_" + mangle(cls) + "_" + mangle(name)
            longn = short + "__" + mangle(desc[1:desc.index(")")])
            sym = longn if longn in impl_exports else (short if short in impl_exports else None)
            if sym:
                rows.append((cls, name, desc, sym))
            elif impl_name != lib and (longn in exports or short in exports):
                rows.append((cls, name, desc, ""))        # exists only in the OpenGL variant: no-op
        rows.sort()
        with open(os.path.join(tables, lib + ".tbl"), "w") as t:
            t.write("X86LIB\t" + impl_name + "\n")
            for r in rows:
                t.write("\t".join(r) + "\n")
        dst = os.path.join(libs, lib)
        shutil.copy2(shim_so, dst)                       # separate inode for every name
        summary[lib] = len(rows)
    for real in real_libs:
        shutil.copy2(real, os.path.join(libs, os.path.basename(real)))
    return summary


if __name__ == "__main__":
    inst, out, shim = sys.argv[1], sys.argv[2], sys.argv[3]
    import json
    cfg = json.load(open(os.path.join(inst, "ProjectZomboid64.json")))
    s = build(inst, cfg["classpath"], out, shim, sys.argv[4:])
    for k, v in sorted(s.items()):
        print(f"{k:34s} {v:4d} natives")
