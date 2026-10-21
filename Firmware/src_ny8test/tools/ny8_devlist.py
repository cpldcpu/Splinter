#!/usr/bin/env python3
"""Build the NY8A device table from Nyquest tool databases.

usage: ny8_devlist.py <Q-Writer Nyquest.Core resources dir> <NYC_NY8 app dir> <Q-Writer USB lib resources dir> <out prefix>
The resource dirs are the embedded .resx resources of Nyquest.Core.dll / Nyquest.USBClassLibrary.dll
(Q-Writer 6.60), extracted to files (otp.txt, mask.txt, NY8_*_Otp_Download_Config.txt, ICE_*.bin)."""
import collections
import json
import os
import re
import sys

C, A, RES, OUT = sys.argv[1:5]   # Q-Writer core resources, NYC_NY8 app dir, USB lib resources, output prefix

strip = lambda t: re.sub(r'<!--.*?-->', '', t, flags=re.S)

# --- otp.txt: IDs per revision --------------------------------------------------------
otp = collections.OrderedDict()
for m in re.finditer(r'<otp id="([^"]+)"([^>]*)>(.*?)</otp>', strip(open(C + '/otp.txt').read()), re.S):
    name, attrs, body = m.groups()
    a = dict(re.findall(r'(\w+)="([^"]*)"', attrs))
    e = otp.setdefault(name, {'revs': [], 'chipid1': set(), 'cmd': set(), 'rom': None, 'mtp': False})
    e['revs'].append((a.get('revision'), a.get('chipid0')))
    e['chipid1'].add(a.get('chipid1'))
    e['cmd'].add(a.get('command_id'))
    r = re.search(r'<romsize_byte>([^<]+)</romsize_byte>', body)
    if r: e['rom'] = r.group(1)
    if '<mtp/>' in body: e['mtp'] = True

# --- mask.txt: body properties -----------------------------------------------------
mask = {}
for m in re.finditer(r'<body ([^>]*?)(/>|>(.*?)</body>)', strip(open(C + '/mask.txt').read()), re.S):
    a = dict(re.findall(r'(\w+)="([^"]*)"', m.group(1)))
    if 'id' in a:
        a['bugs'] = re.findall(r'<bug>([^<]+)</bug>', m.group(3) or '')
        mask[a['id']] = a

# --- writer download configs ---------------------------------------------------------
def cfg(path):
    out = {}
    for m in re.finditer(r'<otp id="([^"]+)"[^>]*>(.*?)</otp>', strip(open(path).read()), re.S):
        out[m.group(1)] = {k: v.strip() for k, v in re.findall(r'<(\w+)>([^<]*)</\1>', m.group(2))}
    return out
fw = cfg(C + '/NY8_FW_Otp_Download_Config.txt')
hw = cfg(C + '/NY8_HW_Otp_Download_Config.txt')

# --- SDCC device file and memory models ------------------------------------------------
sdcc = {}
cur = None
for line in open(A + '/include/ny8devices.txt'):
    line = line.split('#', 1)[0].strip()
    if not line: continue
    tok = line.split()
    if tok[0] == 'processor':
        cur = {}
        for p in tok[1:]: sdcc['NY' + p.upper()] = cur
    elif cur is not None and len(tok) >= 2:
        cur.setdefault(tok[0], ' '.join(tok[1:]))
props = {m.group(1): m.group(2) for m in re.finditer(r'<ic name="([^"]+)" ms="([^"]+)"', open(A + '/lib/properties').read())}

# --- emulator FPGA images in Q-Writer ----------------------------------------------------
fpga = set()
for f in os.listdir(RES):
    m = re.match(r'ICE_(?:VERB_)?FPGA_(NY8\w+)\.bin', f)
    if m: fpga.add(m.group(1))

rows = []
for name, e in otp.items():
    if not name.startswith('NY8A'): continue
    mk, f, h, s = mask.get(name, {}), fw.get(name, {}), hw.get(name, {}), sdcc.get(name, {})
    romb = mk.get('romsize_byte') or e['rom'] or ''
    words = int(float(romb.lower().replace('k', '')) * 1024 // 2) if romb else None
    rows.append({
        'body': name,
        'rom_words': words,
        'ram_bytes': mk.get('ramsize_byte'),
        'die_id': sorted(e['chipid1']),
        'revisions': [f'{r}:{c}' for r, c in e['revs']],
        'command_id': sorted(e['cmd']),
        'mtp': e['mtp'],
        'enter_mode': f.get('WRITER_Enter_Mode'),
        'program_mode': f.get('WRITER_Program_Mode'),
        'vpp_code': f.get('Program_High_Voltage_setting'),
        'cp_type': f.get('MTP_CP_Type'),
        'trim_cmd_bit': h.get('TRIM_COMMAND_BIT'),
        'ihrc_trim': h.get('IHRC_TRIM'),
        'jump_size': s.get('jump_size'),
        'sdcc_model': props.get(name),
        'profile': mk.get('profile'),
        'series': mk.get('series'),
        'bugs': mk.get('bugs', []),
        'ice_fpga': name in fpga,
    })

json.dump(rows, open(OUT + '.json', 'w'), indent=1)

def short(v):
    return ', '.join(v) if isinstance(v, list) else ('' if v is None else str(v))

hdr = ['Body', 'ROM words', 'RAM B', 'Die ID (chipid1)', 'Revisions (rev:chipid0)', 'Cmd ID', 'Entry', 'VPP code',
       'CP type', 'Trim cmd bit', 'IHRC trim', 'LGOTO words', 'SDCC model', 'Profile', 'Notes']
with open(OUT + '.md', 'w') as o:
    o.write('| ' + ' | '.join(hdr) + ' |\n|' + '---|' * len(hdr) + '\n')
    for r in sorted(rows, key=lambda r: r['body']):
        notes = []
        if r['mtp']: notes.append('MTP')
        if r['bugs']: notes.append('bug: ' + ', '.join(r['bugs']))
        if r['ice_fpga']: notes.append('ICE image')
        o.write('| ' + ' | '.join([r['body'], short(r['rom_words']), short(r['ram_bytes']), short(r['die_id']),
            short(r['revisions']), short(r['command_id']), short(r['enter_mode']), short(r['vpp_code']),
            short(r['cp_type']), short(r['trim_cmd_bit']), short(r['ihrc_trim']), short(r['jump_size']),
            short(r['sdcc_model']), short(r['profile']), '; '.join(notes)]) + ' |\n')
print(len(rows), 'devices')
