#!/usr/bin/env python3
import json,plistlib,zipfile,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
info=plistlib.loads((root/'App/Info.plist').read_bytes())
assert info['CFBundleIdentifier']=='org.lekak.iosprobe'
assert info['CFBundleExecutable']=='LekakProbe'
assert info['MinimumOSVersion']=='15.0'
audit=json.loads((root/'engine-audit.json').read_text())
assert not audit['built_ios_app'] and not audit['tested_on_iphone']
main=(root/'App/main.m').read_text()
assert 'MAP_FIXED,' not in main and 'mprotect(' not in main
assert 'native_function_fits_32bits' in main and 'UIApplicationMain' in main
if '--built' in sys.argv:
    app=root/'build/Payload/LekakProbe.app';binary=(app/'LekakProbe').read_bytes()
    assert binary[:4]==b'\xcf\xfa\xed\xfe','Expected a 64-bit Mach-O executable'
    assert int.from_bytes(binary[4:8],'little')==0x0100000c,'Expected ARM64'
    with zipfile.ZipFile(root/'build/Lekak_iPhone_Diagnostic.ipa') as z:
        assert z.testzip() is None
        assert 'Payload/LekakProbe.app/LekakProbe' in z.namelist()
        assert 'Payload/LekakProbe.app/compiler-check.json' in z.namelist()
print('Project checks passed'+('; unsigned ARM64 IPA structure passed' if '--built' in sys.argv else '; macOS build still required'))
