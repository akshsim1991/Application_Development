#!/usr/bin/env python3
"""Issues a signed license key: BASE64URL(payload).BASE64URL(ECDSA-P256-SHA256 signature r||s).
Usage: license_issue.py --key PRIVATE.pem --name "Customer" [--edition pro] [--devices 1] [--expires YYYY-MM-DD] [--id ORDER-ID]"""
import argparse, json, base64, datetime
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature
from cryptography.hazmat.primitives import hashes, serialization
ap = argparse.ArgumentParser(); ap.add_argument('--key', required=True); ap.add_argument('--name', required=True); ap.add_argument('--edition', default='pro'); ap.add_argument('--devices', type=int, default=1); ap.add_argument('--expires', default=''); ap.add_argument('--id', default=''); a = ap.parse_args()
payload = json.dumps({"product": "FeatherSystemUtilities", "edition": a.edition, "name": a.name, "devices": a.devices, "issued": datetime.date.today().isoformat(), "expires": a.expires, "id": a.id}, separators=(',', ':')).encode()
k = serialization.load_pem_private_key(open(a.key, 'rb').read(), None); r, s = decode_dss_signature(k.sign(payload, ec.ECDSA(hashes.SHA256())))
b = lambda x: base64.urlsafe_b64encode(x).rstrip(b'=').decode(); print(b(payload) + '.' + b(r.to_bytes(32, 'big') + s.to_bytes(32, 'big')))
