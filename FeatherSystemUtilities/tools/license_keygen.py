#!/usr/bin/env python3
"""Creates the license signing key pair. KEEP THE PRIVATE KEY SECRET and OUT OF GIT.
Usage: license_keygen.py PRIVATE_KEY.pem   -> writes the private key and updates data/license_pub.json (the public half embedded in the app)."""
import sys, json, base64, os
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import serialization
if len(sys.argv) != 2: sys.exit(__doc__)
k = ec.generate_private_key(ec.SECP256R1()); n = k.public_key().public_numbers(); b = lambda i: base64.urlsafe_b64encode(i.to_bytes(32, 'big')).rstrip(b'=').decode()
fd = os.open(sys.argv[1], os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
with os.fdopen(fd, 'wb') as f: f.write(k.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
pub = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'data', 'license_pub.json')
json.dump({"alg": "ES256", "x": b(n.x), "y": b(n.y)}, open(pub, 'w'), indent=1); print('Private key written to', sys.argv[1], '- public key updated in', pub, '- rebuild the app to embed it.')
