#!/usr/bin/env python3
"""Build an unsigned iPhone diagnostic IPA on macOS/Xcode.

No ROM, Lekak assets, developer credentials, or executable memory patching.
The compiler check can fail without preventing the diagnostic app build.
"""
import json,os,plistlib,shutil,subprocess,sys,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def run(args):
    return subprocess.run(args,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,check=False)
def build():
    if sys.platform!='darwin':
        raise SystemExit('This build runs on the GitHub macOS runner, not Windows or Linux.')
    sdk=run(['xcrun','--sdk','iphoneos','--show-sdk-path'])
    if sdk.returncode:raise SystemExit(sdk.stdout)
    compiler=run(['xcrun','--find','clang'])
    if compiler.returncode:raise SystemExit(compiler.stdout)
    sdk=sdk.stdout.strip();cc=compiler.stdout.strip()
    pointer_cc=os.environ.get('LEKAK_POINTER_CC',cc)
    version=run([pointer_cc,'--version'])
    if version.returncode: raise SystemExit(version.stdout)
    destination=ROOT/'build';destination.mkdir(exist_ok=True)
    flags=['-target','arm64-apple-ios15.0','-isysroot',sdk]
    check=run([pointer_cc,*flags,'-fms-extensions','-O2','-c',str(ROOT/'App/ptr32_check.c'),'-o',str(destination/'ptr32.o')])
    result={'supported':check.returncode==0,'exit_code':check.returncode,
            'compiler':version.stdout.strip(),'log':check.stdout,
            'test':'Eight-byte record with two G32 pointers, native-to-G32 stores, and G32 callback invocation.'}
    host_tests=run([sys.executable,str(ROOT/'tools/test_engine_host.py')])
    result['engine_host_tests']={'passed':host_tests.returncode==0,'log':host_tests.stdout}
    if host_tests.returncode:
        (destination/'build-log.txt').write_text(host_tests.stdout)
        raise SystemExit('Engine host tests failed:\n'+host_tests.stdout)
    result['app_compiler']=run([cc,'--version']).stdout.strip()
    result['engine_header']='Original src/port_ptr.h; stored pointer layout, field offset, indexed store, byte load/store and callback lowering checked.'
    if check.returncode==0:
        ir=run([pointer_cc,*flags,'-fms-extensions','-O2','-S','-emit-llvm',str(ROOT/'App/ptr32_check.c'),'-o',str(destination/'ptr32-ios.ll')])
        result['ir_exit_code']=ir.returncode
        result['ir_log']=ir.stdout
    linux=run([pointer_cc,'-target','aarch64-unknown-linux-gnu','-fms-extensions','-O2','-ffreestanding','-c',str(ROOT/'App/ptr32_check.c'),'-o',str(destination/'ptr32-linux.o')])
    result['linux_control']={'supported':linux.returncode==0,'log':linux.stdout}
    (destination/'compiler-check.json').write_text(json.dumps(result,indent=2))
    app=destination/'Payload/LekakProbe.app'
    app.mkdir(parents=True,exist_ok=True)
    info=plistlib.loads((ROOT/'App/Info.plist').read_bytes())
    info['CFBundleSupportedPlatforms']=['iPhoneOS']
    info['DTPlatformName']='iphoneos'
    (app/'Info.plist').write_bytes(plistlib.dumps(info))
    shutil.copy2(destination/'compiler-check.json',app/'compiler-check.json')
    sources=[str(ROOT/p) for p in [
        'App/engine_bridge.c','App/engine_callbacks.c','App/optional_textures.c','Engine/src/pc/memory.c',
        'Engine/src/pc/rng.c','Engine/src/pc/compat/libgs_ot.c','Engine/src/pc/compat/gte.c',
        'Engine/src/pc/compat/pgxp.c','Engine/src/pc/render/packets.c','Engine/src/pc/render/soft_gpu.c']]
    objects=[]
    for index,source in enumerate(sources):
        obj=destination/f'engine-{index}.o'
        compiled=run([cc,*flags,'-std=c11','-O2','-Wall','-I'+str(ROOT/'Engine/src'),'-c',source,'-o',str(obj)])
        if compiled.returncode:
            (destination/'build-log.txt').write_text(compiled.stdout)
            raise SystemExit('Engine component compile failed: '+source+'\n'+compiled.stdout)
        objects.append(str(obj))
    command=[cc,*flags,'-fobjc-arc','-fblocks','-O2','-Wall','-Wextra',str(ROOT/'App/main.m'),*objects,
             '-framework','UIKit','-framework','Foundation','-framework','CoreGraphics','-o',str(app/'LekakProbe')]
    built=run(command)
    (destination/'build-log.txt').write_text(built.stdout)
    if built.returncode:raise SystemExit('The diagnostic app did not compile:\n'+built.stdout)
    os.chmod(app/'LekakProbe',0o755)
    with zipfile.ZipFile(destination/'Lekak_iPhone_Diagnostic.ipa','w',zipfile.ZIP_DEFLATED) as archive:
        for p in sorted((destination/'Payload').rglob('*')):
            if p.is_file():archive.write(p,str(p.relative_to(destination)))
    print('Diagnostic IPA created. The game is NOT included. Stored-pointer compiler check:',result['supported'])
if __name__=='__main__':build()
