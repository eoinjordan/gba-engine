#!/usr/bin/env python3
"""Compile Studio-exported C scene data and GBA-engine for Tang Nano 20K."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[2]
PLATFORM=Path(__file__).resolve().parent

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data',type=Path,required=True,help='Studio export root containing include/data and src/data')
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--llvm',type=Path,help='LLVM bin directory with clang and llvm-objcopy')
    args=parser.parse_args()
    suffix='.exe' if os.name=='nt' else ''
    llvm=args.llvm or (Path(shutil.which('clang')).parent if shutil.which('clang') else None)
    if llvm is None and os.name=='nt': llvm=Path('C:/Program Files/LLVM/bin')
    if llvm is None: parser.error('Install LLVM and put clang and llvm-objcopy on PATH, or set --llvm')
    compiler=llvm/('clang'+suffix)
    objcopy=llvm/('llvm-objcopy'+suffix)
    data=args.data.resolve(); out=args.out.resolve()
    if not (data/'include/data/gba_scene_data.h').is_file():
        parser.error('Missing GBA scene data; export Studio with --target gba')
    out.mkdir(parents=True,exist_ok=True)
    sources=sorted((ROOT/'src').glob('*.c'))+sorted((data/'src/data').glob('*.c'))
    sources += [PLATFORM/'memory.c',PLATFORM/'render.c',PLATFORM/'startup.S']
    elf=out/'game.elf'; binary=out/'game.tang.bin'
    command=[str(compiler),'--target=riscv32-unknown-elf','-march=rv32im','-mabi=ilp32',
             '-O2','-ffreestanding','-fno-builtin','-nostdlib','-fdata-sections','-ffunction-sections',
             '-DTANG_NANO20K','-I'+str(PLATFORM),'-I'+str(ROOT/'include'),'-I'+str(data/'include'),
             '-fuse-ld=lld','-Wl,--gc-sections','-Wl,-Map='+str(out/'game.map'),
             '-Wl,-T,'+str(PLATFORM/'link.ld'),'-o',str(elf)]+[str(s) for s in sources]
    subprocess.run(command,check=True)
    subprocess.run([str(objcopy),'-O','binary',str(elf),str(binary)],check=True)
    payload=binary.read_bytes()
    if not 4<=len(payload)<=1048576: raise RuntimeError('Firmware exceeds 1 MiB loader limit')
    report={'target':'tangnano20k-rv32im','bytes':len(payload),
            'sha256':hashlib.sha256(payload).hexdigest(),
            'compiler':subprocess.check_output([str(compiler),'--version'],text=True).splitlines()[0],
            'sources':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}}
    (out/'build.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'binary':str(binary),'bytes':len(payload),'sha256':report['sha256']}))

if __name__=='__main__': main()
