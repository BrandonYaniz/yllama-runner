#!/usr/bin/env python3
"""Opt-in curated GGUF protocol-2 qualification; writes auditable JSON results."""
import argparse, hashlib, json, os, platform, struct, subprocess, time

def frame(t,p=b""): return bytes([t])+struct.pack("<I",len(p))+p
def generate(prompt, seed=1):
    p=struct.pack("<BBHI",1,0,0,32)+struct.pack("<ddidddQ",0.0,1.0,0,0.0,0.0,1.0,seed)
    b=prompt.encode(); return frame(1,p+struct.pack("<I",len(b))+b)
def read_frame(stream):
    h=stream.read(5)
    if len(h)!=5: raise RuntimeError("unexpected runner EOF")
    t,n=struct.unpack("<BI",h)
    if n>32<<20: raise RuntimeError("oversized output")
    p=stream.read(n)
    if len(p)!=n: raise RuntimeError("truncated output")
    return t,p
def sha256(path):
    h=hashlib.sha256()
    with open(path,"rb") as f:
        for block in iter(lambda:f.read(1<<20),b""): h.update(block)
    return h.hexdigest()
def request(proc,prompt):
    proc.stdin.write(generate(prompt));proc.stdin.flush(); chunks=[];completed=0;usage=None
    while True:
        t,p=read_frame(proc.stdout)
        if t==1:
            n=struct.unpack_from("<I",p)[0]; data=p[4:]
            if n!=len(data): raise RuntimeError("bad Chunk length")
            data.decode("utf-8");chunks.append(data)
        elif t==4:
            completed+=1; reason,inp,out,pu,gu=struct.unpack("<BIIQQ",p);usage=(inp,out)
            break
        elif t==3: raise RuntimeError("runner Error: "+repr(p))
        else: raise RuntimeError(f"unexpected type {t}")
    if completed!=1 or not b"".join(chunks) or not usage or min(usage)<=0: raise RuntimeError("invalid completion/output/usage")
def cancel_request(proc,prompt):
    proc.stdin.write(generate(prompt)+frame(2));proc.stdin.flush(); terminals=0
    while True:
        t,p=read_frame(proc.stdout)
        if t==1: p[4:].decode("utf-8")
        elif t==4:
            terminals+=1; reason=struct.unpack_from("<B",p)[0]
            if reason!=3 or terminals!=1: raise RuntimeError("Cancel did not complete as cancelled")
            return
        elif t==3: raise RuntimeError("Cancel produced Error")

def main():
    ap=argparse.ArgumentParser();ap.add_argument("--runner",required=True);ap.add_argument("--manifest",default=os.path.join(os.path.dirname(__file__),"models.json"));ap.add_argument("--results",required=True);a=ap.parse_args()
    manifest=json.load(open(a.manifest)); results=[]
    info=subprocess.check_output([a.runner,"--build-info"],text=True)
    revision=next(x.split(": ",1)[1] for x in info.splitlines() if x.startswith("llama.cpp-revision:"));version=next(x.split(": ",1)[1] for x in info.splitlines() if x.startswith("runner-version:"))
    for index,m in enumerate(manifest["models"]):
        path=os.environ.get(m["env"]);record={"model_family":m["family"],"artifact_filename":m["artifact"],"quantization":m["quantization"],"llama_cpp_revision":revision,"runner_version":version,"os":platform.system(),"architecture":platform.machine(),"passed":False}
        try:
            if not path: raise RuntimeError(f"unset {m['env']}")
            record["artifact_sha256"]=sha256(path)
            proc=subprocess.Popen([a.runner,"--protocol","2","--model",path,"--ctx","2048","--threads",str(os.cpu_count() or 1)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            t,_=read_frame(proc.stdout)
            if t!=0x10: raise RuntimeError("missing Ready")
            request(proc,m["prompt"]);request(proc,m["prompt"])
            if index==0:
                cancel_request(proc,m["prompt"]*64)
                request(proc,m["prompt"])
            proc.stdin.write(frame(3));proc.stdin.flush();proc.stdin.close()
            if proc.wait(timeout=30)!=0: raise RuntimeError("unclean Shutdown")
            record["passed"]=True
        except Exception as exc: record["failure_details"]=str(exc)
        results.append(record)
    with open(a.results,"w") as f: json.dump({"generated_at":time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime()),"results":results},f,indent=2)
    raise SystemExit(0 if all(x["passed"] for x in results) else 1)
if __name__=="__main__": main()
