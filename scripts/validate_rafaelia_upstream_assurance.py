#!/usr/bin/env python3
import json
from pathlib import Path

P = Path('docs/rafaelia/upstream-assurance.v1.json')

def fail(msg):
    raise SystemExit(f'FAIL: {msg}')

m = json.loads(P.read_text(encoding='utf-8'))
if m.get('schema') != 'rafaelia.large-upstream-assurance.v1': fail('schema')
if m.get('claim_allowed') is not False: fail('claim_allowed')
if m['license'].get('rafaelia_relicense_of_upstream_allowed') is not False: fail('relicense')
if not Path('COPYING').is_file(): fail('COPYING missing')
if m['provenance'].get('ownership_claimed_by_manifest') is not False: fail('ownership claimed before delta proof')
if 'TOKEN_VAZIO' not in m['provenance'].get('authorial_delta_boundary',''): fail('delta prematurely promoted')
if m['runtime'].get('source_presence_is_qemu_runtime') is not False: fail('source promoted to runtime')
if m['runtime'].get('qemu_process_is_guest_boot') is not False: fail('process promoted to boot')
if m['security'].get('security_claim_from_brand_or_upstream') is not False: fail('brand security claim')
if not any(g['urgency']=='P0' and g['state']=='TOKEN_VAZIO' for g in m['gaps']): fail('P0 gap missing')
if not m['rollback'].get('available'): fail('rollback')
print('PASS: QEMU upstream license/provenance/runtime boundaries remain fail-closed')
