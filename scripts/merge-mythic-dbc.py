#!/usr/bin/env python3
from __future__ import annotations
import argparse, csv, struct
from pathlib import Path

HEADER=struct.Struct("<4s4I")
SCHEMAS={
 "Item.csv":{"strings":set()},
 "ItemDisplayInfo.csv":{"strings":{
   "ModelName_1","ModelName_2","ModelTexture_1","ModelTexture_2",
   "InventoryIcon_1","InventoryIcon_2",
   "Texture_1","Texture_2","Texture_3","Texture_4",
   "Texture_5","Texture_6","Texture_7","Texture_8"
 }},
}

def base(path, fields):
    raw=path.read_bytes()
    magic,count,fc,rs,ss=HEADER.unpack_from(raw)
    if magic!=b"WDBC" or fc!=fields or rs!=fields*4:
        raise SystemExit(f"invalid/incompatible base DBC: {path}")
    start=HEADER.size; end=start+count*rs
    if end+ss!=len(raw): raise SystemExit(f"truncated DBC: {path}")
    return [raw[start+i*rs:start+(i+1)*rs] for i in range(count)], raw[end:end+ss] or b"\0"

def allocator(initial):
    block=bytearray(initial); offsets={"":0}; pos=1
    while pos<len(block):
        end=block.find(0,pos)
        if end<0: break
        try: s=bytes(block[pos:end]).decode("utf-8")
        except UnicodeDecodeError: s=""
        if s and s not in offsets: offsets[s]=pos
        pos=end+1
    def alloc(s):
        if not s:return 0
        if s in offsets:return offsets[s]
        off=len(block); block.extend(s.encode()); block.append(0); offsets[s]=off; return off
    return block,alloc

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--csv-dir",type=Path,required=True)
    ap.add_argument("--base-dir",type=Path,required=True)
    ap.add_argument("--out-dir",type=Path,required=True)
    args=ap.parse_args()
    args.out_dir.mkdir(parents=True,exist_ok=True)
    for name,schema in SCHEMAS.items():
        cp=args.csv_dir/name
        with cp.open(encoding="utf-8-sig",newline="") as fh:
            reader=csv.DictReader(fh); fields=reader.fieldnames or []; rows=list(reader)
        if not fields or fields[0]!="ID": raise SystemExit(f"{name}: first field must be ID")
        bp=args.base_dir/(Path(name).stem+".dbc")
        recs,strings=base(bp,len(fields)); block,alloc=allocator(strings)
        order=[]; byid={}
        for rec in recs:
            rid=struct.unpack_from("<I",rec,0)[0]
            if rid not in byid: order.append(rid)
            byid[rid]=rec
        for row in rows:
            rid=int(row["ID"]); chunks=[]
            for field in fields:
                val=(row.get(field) or "").strip()
                if field in schema["strings"]:
                    chunks.append(struct.pack("<I",alloc(val)))
                else:
                    chunks.append(struct.pack("<I",int(float(val or "0")) & 0xffffffff))
            if rid not in byid: order.append(rid)
            byid[rid]=b"".join(chunks)
        final=[byid[x] for x in order]
        out=args.out_dir/(Path(name).stem+".dbc")
        out.write_bytes(HEADER.pack(b"WDBC",len(final),len(fields),len(fields)*4,len(block))+b"".join(final)+bytes(block))
        print(f"[FURY][MYTHIC][DBC] {out.name}: base={len(recs)} delta={len(rows)} final={len(final)}")

if __name__=="__main__": main()
