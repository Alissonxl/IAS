"""Validate the input profile against installed PE files, without loading either DLL."""
import configparser
import hashlib
from pathlib import Path
import struct
import sys

class Image:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        pe = self.u32(0x3c)
        self.base = self.u32(pe + 52)
        self.sections = []
        for index in range(struct.unpack_from('<H', self.data, pe + 6)[0]):
            entry = pe + 24 + struct.unpack_from('<H', self.data, pe + 20)[0] + index * 40
            size, rva, raw_size, raw = struct.unpack_from('<IIII', self.data, entry + 8)
            self.sections.append((rva, raw_size, raw))
    def u32(self, offset):
        return struct.unpack_from('<I', self.data, offset)[0]
    def offset(self, rva):
        for start, size, raw in self.sections:
            if start <= rva < start + size:
                return raw + rva - start
        raise ValueError(f'RVA outside raw sections: {rva:#x}')

profile = configparser.ConfigParser()
profile.read(sys.argv[1], encoding='ascii')
config = profile['input']
root = Path(sys.argv[2])
client = Image(root / 'mods/deathmatch/client.dll')
core = Image(root / 'MTA/core.dll')
checks = 0

def check(value, message):
    global checks
    if not value:
        raise AssertionError(message)
    checks += 1
    print('PASS', message)

def number(key):
    return int(config[key], 0)

for image, name in ((client, 'client'), (core, 'core')):
    check(hashlib.sha256(image.data).hexdigest().upper() == config[name + '_sha256'].upper(), name + ' file matches fingerprint')
for name in ('visible', 'controls', 'force', 'chat', 'menu', 'settings'):
    entry = core.offset(number('core_vtable_rva') + number(name + '_index') * 4)
    check(core.u32(entry) == core.base + number(name + '_rva'), name + ' slot points to expected implementation')
slot = struct.pack('<I', client.base + number('core_pointer_rva'))
check(b'\x89\x35' + slot in client.data, 'client initialization stores core interface at configured slot')
force = core.data[core.offset(number('force_rva')):][:32]
check(b'\xc2\x08\x00' in force and b'\xff\x75\x08' in force, 'force method consumes two x86 stack arguments')
check(bytes((0x8b, 0x49, number('local_gui_offset'))) in force, 'force method forwards to configured local GUI field')
print(f'{checks} offline binary checks passed; no MTA process exercised.')

