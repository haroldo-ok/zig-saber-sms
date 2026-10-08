#!/usr/bin/env python3
"""Package the actual SMS ROM + tiny WASM emulator into a single offline HTML.
No network, CDN, external font, or separate asset is required.
"""
import base64,re
from pathlib import Path
import json
root=Path(__file__).resolve().parents[1]
symbols={m[1]:int(m[2],16) for m in re.finditer(r'DEF\s+_(\w+)\s+0x([0-9A-Fa-f]+)',(root/'zig-saber.noi').read_text())}
needed=['game_state','is_demo','paused']
html=(root/'tools/player-template.html').read_text()
html=html.replace('__ROM_BASE64__',base64.b64encode((root/'zig-saber.sms').read_bytes()).decode()).replace('__WASM_BASE64__',base64.b64encode((root/'assets/sms-player.wasm').read_bytes()).decode()).replace('__SYMBOLS__',json.dumps({k:symbols[k] for k in needed}))
(root/'PLAY.html').write_text(html)
print('PLAY.html: %d bytes, embeds the unmodified cartridge ROM and WASM.'%len(html.encode()))
