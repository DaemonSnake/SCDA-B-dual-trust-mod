"""Inventory constant AddGoal/ModifyTrust/SetGoalStatus calls in the user's Xbox LINs.

This is a bounded static scanner, not a complete bytecode decoder. It does not
prove reachability. See knowledge/TRUST_REFERENCE.md for evidence and coverage.
Native IDs are pinned to the XBE build documented there. No game files are edited.
"""
from pathlib import Path
import json, re, struct, hashlib, zlib

import argparse

parser = argparse.ArgumentParser(description='Read-only SCDA trust inventory. Extracted game data stays outside the project.')
parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
parser.add_argument('--output', type=Path, default=Path.home() / 'scda-trust-analysis')
options = parser.parse_args()
ROOT = options.root.resolve()
OUT = options.output.resolve()
if OUT == ROOT or ROOT in OUT.parents:
    raise SystemExit('Output must be outside the project: it contains extracted game data.')
(OUT / 'decoded').mkdir(parents=True, exist_ok=True)
MAPROOT = ROOT / 'assets/Default/DataXb/LMaps'

def constant(b, p):
    token = b[p]; p += 1
    if token == 0x1f:
        end = b.index(0, p, p + 1000)
        return b[p:end].decode('latin1'), end + 1
    if token in (0x24, 0x2c): return b[p], p + 1
    if token == 0x1d: return struct.unpack_from('<i', b, p)[0], p + 4
    if token == 0x1e: return struct.unpack_from('<f', b, p)[0], p + 4
    if token in (0x25, 0x26): return token - 0x25, p
    if token in (0x27, 0x28): return token == 0x27, p
    if token == 0x0b: return {'omitted': True}, p
    raise ValueError(f'nonconstant token {token:02x} at {p-1:x}')

def decode(p):
    target = OUT / 'decoded' / (p.stem + '.decoded')
    b = p.read_bytes(); pos = 0; data = bytearray()
    while pos < len(b):
        n, c = struct.unpack_from('<II', b, pos)
        block = zlib.decompress(b[pos+8:pos+8+c])
        assert len(block) == n
        data += block; pos += 8 + c
    assert pos == len(b)
    target.write_bytes(data)
    return bytes(data), hashlib.sha256(b).hexdigest()

def localization():
    # Keep the first language occurrence: English. Do not merge translations.
    raw = (ROOT / 'assets/Default/dynamicxbox.umd').read_bytes()
    text = raw.decode('latin1').replace('\r', '')
    sections = {}; current = None
    for line in text.splitlines():
        match = re.search(r'\[([^\]]+)\]$', line)
        if match and '=' not in line:
            name = match[1].lower()
            current = {} if name not in sections else None
            if current is not None: sections[name] = current
        elif current is not None and '=' in line:
            key, value = line.split('=', 1)
            current.setdefault(key.lower(), value)
    return sections

def main():
    xbe_sha = hashlib.sha256((ROOT / 'assets/default.xbe').read_bytes()).hexdigest()
    if xbe_sha != 'c742d61c9b8d8c1c4f886a4a4a69ffb65ab3961e0283dcad33bfcfa963885143':
        raise SystemExit('Unsupported XBE: native opcode numbers must be checked first.')
    loc = localization(); objectives = []; direct = []; changes = []; diagnostics = []
    files = []
    paths = [MAPROOT/'common.lin', MAPROOT/'Menu/Menu.lin']
    paths += sorted(p for p in MAPROOT.glob('*/*.lin') if 'menu' not in p.stem.lower())
    for path in paths:
        b, sha = decode(path)
        files.append({'file': str(path.relative_to(ROOT)), 'sha256': sha, 'decoded_size': len(b)})
        count = 0
        for match in re.finditer(re.escape(b'\x6d\x05\x1fLocalization\\'), b):
            start = match.start(); pos = start + 2
            try:
                args = []
                provided_args = 0
                for _ in range(13):
                    if b[pos] == 0x16:
                        args.append(0)
                    else:
                        val, pos = constant(b, pos)
                        # EX_Nothing explicitly omits an optional parameter.
                        # All optional numeric/boolean AddGoal defaults read by
                        # this build are zero (verified in sub_0009B19F).
                        if isinstance(val, dict) and val.get('omitted'):
                            val = 0
                        args.append(val); provided_args += 1
                assert b[pos] == 0x16, f'end token {b[pos]:02x}'
                assert all(isinstance(x, str) for x in args[:4])
                assert isinstance(args[12], int) and not isinstance(args[12], bool)
                row = {'map': path.stem, 'offset': hex(start), 'end': hex(pos+1), 'id': args[3],
                       'section': args[1], 'title_key': args[2], 'args': args, 'provided_args': provided_args, 'amount': args[12],
                       'jba_direction': bool(args[11]), 'original_delta': -args[12] if args[11] else args[12]}
                section = loc.get(args[1].lower(), {})
                row['title'] = section.get(args[2].lower(), '')
                row['description'] = section.get(str(args[9]).lower(), '')
                objectives.append(row); count += 1
            except Exception as e:
                diagnostics.append({'map': path.stem, 'offset': hex(start), 'kind': 'AddGoal', 'error': str(e)})
        # Only accept constant native calls with both parameters and an end token.
        # Raw byte matches outside script bodies remain candidates until context review.
        for match in re.finditer(re.escape(b'\x6f\xc0'), b):
            start = match.start(); pos = start + 2
            try:
                amount, pos = constant(b, pos); faction, pos = constant(b, pos)
                if b[pos] == 0x16 and isinstance(amount, int) and isinstance(faction, bool):
                    direct.append({'map': path.stem, 'offset': hex(start), 'amount': amount,
                                   'jba_direction': faction, 'original_delta': -amount if faction else amount})
            except (ValueError, IndexError, struct.error): pass
        for pattern, kind in [(b'\x68\xae\x1f', 'EPattern.SetGoalStatus'), (b'\x63\xeb\x1f', 'EGameStats.SetGoalStatus')]:
            for match in re.finditer(re.escape(pattern), b):
                start = match.start()
                try:
                    ident, pos = constant(b, start+2); state, pos = constant(b, pos)
                    if b[pos] == 0x16 and isinstance(ident, str) and isinstance(state, (bool, int)):
                        changes.append({'map': path.stem, 'offset': hex(start), 'kind': kind, 'id': ident, 'state': state})
                except (ValueError, IndexError, struct.error): pass
        print(path.stem, 'goals', count)
    result = {'files': files, 'objectives': objectives, 'direct_constant_calls': direct,
              'status_calls': changes, 'diagnostics': diagnostics, 'xbe_sha256': xbe_sha,
              'limits': ['Constant-call scanner, not a complete UnrealScript decompiler.',
                         'Nonconstant arguments, virtual calls and unreachable code require separate audit.',
                         'Byte matches are not proof of reachability.']}
    (OUT/'inventory.json').write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding='utf8')
    print('TOTAL', len(objectives), 'direct', len(direct), 'statuses', len(changes), 'errors', diagnostics)

if __name__ == '__main__': main()
