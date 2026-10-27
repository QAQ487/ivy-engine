"""Bounded local-only Windows named-pipe client (stdlib; no process launcher)."""
import argparse
import ctypes as C
from ctypes import wintypes as W
import json
import math
import re
import time

K=C.WinDLL("kernel32", use_last_error=True)
class OVERLAPPED(C.Structure):
    _fields_=[("Internal",C.c_size_t),("InternalHigh",C.c_size_t),
              ("Offset",W.DWORD),("OffsetHigh",W.DWORD),("hEvent",W.HANDLE)]
K.CreateFileW.argtypes=[W.LPCWSTR,W.DWORD,W.DWORD,C.c_void_p,W.DWORD,W.DWORD,W.HANDLE]
K.CreateFileW.restype=W.HANDLE
K.CreateEventW.argtypes=[C.c_void_p,W.BOOL,W.BOOL,W.LPCWSTR]
K.CreateEventW.restype=W.HANDLE
for fn in ("ReadFile","WriteFile"):
    getattr(K,fn).argtypes=[W.HANDLE,C.c_void_p,W.DWORD,C.POINTER(W.DWORD),C.POINTER(OVERLAPPED)]
    getattr(K,fn).restype=W.BOOL
K.GetOverlappedResult.argtypes=[W.HANDLE,C.POINTER(OVERLAPPED),C.POINTER(W.DWORD),W.BOOL]
K.GetOverlappedResult.restype=W.BOOL
K.CancelIoEx.argtypes=[W.HANDLE,C.POINTER(OVERLAPPED)]
K.WaitForSingleObject.argtypes=[W.HANDLE,W.DWORD]
K.WaitForSingleObject.restype=W.DWORD
K.CloseHandle.argtypes=[W.HANDLE]
K.GetNamedPipeServerProcessId.argtypes=[W.HANDLE,C.POINTER(W.ULONG)]
INVALID=C.c_void_p(-1).value

class Client:
    def __init__(self,pid,run_id,timeout=2.0):
        if not isinstance(pid,int) or isinstance(pid,bool) or pid<=0:
            raise ValueError("positive PID required")
        if not re.fullmatch(r"[A-Za-z0-9-]{1,64}",run_id):
            raise ValueError("invalid run ID")
        if not math.isfinite(timeout) or not 0<timeout<=30:
            raise ValueError("timeout must be in (0,30]")
        self.pid,self.run_id,self.timeout=pid,run_id,timeout
        self.handle=None
        name=rf"\\.\pipe\IvyRuntimeTest-{pid}-{run_id}"
        deadline=time.monotonic()+timeout
        while time.monotonic()<deadline:
            h=K.CreateFileW(name,0xC0000000,0,None,3,0x40000000,None)
            if h!=INVALID:
                self.handle=h
                actual=W.ULONG()
                if not K.GetNamedPipeServerProcessId(h,C.byref(actual)) or actual.value!=pid:
                    self.close()
                    raise RuntimeError("named-pipe server PID mismatch")
                return
            error=C.get_last_error()
            if error not in (2,231):
                raise C.WinError(error)
            time.sleep(min(.01,max(0,deadline-time.monotonic())))
        raise TimeoutError("pipe startup deadline")
    def close(self):
        if self.handle is not None:
            K.CloseHandle(self.handle)
            self.handle=None
    def __enter__(self): return self
    def __exit__(self,*args): self.close()

    def _io(self,data=None,size=1,deadline=None):
        if self.handle is None: raise RuntimeError("closed client")
        deadline=deadline if deadline is not None else time.monotonic()+self.timeout
        remaining=deadline-time.monotonic()
        if remaining<=0: raise TimeoutError("request deadline")
        ov=OVERLAPPED()
        ov.hEvent=K.CreateEventW(None,True,False,None)
        if not ov.hEvent: raise C.WinError(C.get_last_error())
        buf=C.create_string_buffer(data) if data is not None else C.create_string_buffer(size)
        n=W.DWORD()
        count=len(data) if data is not None else size
        try:
            fn=K.WriteFile if data is not None else K.ReadFile
            if not fn(self.handle,buf,count,C.byref(n),C.byref(ov)):
                error=C.get_last_error()
                if error!=997: raise C.WinError(error)
                if K.WaitForSingleObject(ov.hEvent,max(1,int(remaining*1000)))!=0:
                    K.CancelIoEx(self.handle,C.byref(ov))
                    K.GetOverlappedResult(self.handle,C.byref(ov),C.byref(n),True)
                    raise TimeoutError("request deadline")
                if not K.GetOverlappedResult(self.handle,C.byref(ov),C.byref(n),False):
                    raise C.WinError(C.get_last_error())
            if n.value==0 or (data is not None and n.value!=len(data)):
                raise ConnectionError("short pipe transfer")
            return buf.raw[:n.value]
        finally:
            K.CloseHandle(ov.hEvent)
    def send_raw(self,data,deadline=None):
        return self._io(data=data,deadline=deadline)
    def read_response(self,deadline=None):
        deadline=deadline if deadline is not None else time.monotonic()+self.timeout
        data=bytearray()
        while len(data)<=8192:
            b=self._io(deadline=deadline)
            if b==b"\n": return json.loads(data)
            data+=b
        raise ValueError("oversize response")
    def request(self,verb,request_id):
        deadline=time.monotonic()+self.timeout
        q=dict(protocolVersion=1,requestId=request_id,runId=self.run_id,verb=verb)
        self.send_raw(json.dumps(q,separators=(",",":")).encode()+b"\n",deadline)
        r=self.read_response(deadline)
        if r.get("protocolVersion")!=1 or r.get("runId")!=self.run_id or r.get("requestId")!=request_id:
            raise RuntimeError("response identity mismatch")
        if r.get("ok") and verb in ("hello","status") and r.get("pid")!=self.pid:
            raise RuntimeError("response PID mismatch")
        if r.get("ok") and verb=="quit":
            if r.get("requiresReceipt") is not True: raise RuntimeError("missing quit receipt contract")
            self.send_raw(b"\n",deadline)
        return r

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--pid",type=int,required=True)
    p.add_argument("--run-id",required=True)
    p.add_argument("--request-id",type=int,required=True,help="strictly increasing across the run")
    p.add_argument("--timeout",type=float,default=2)
    p.add_argument("verb",choices=("hello","status","quit"))
    a=p.parse_args()
    try:
        with Client(a.pid,a.run_id,a.timeout) as c: r=c.request(a.verb,a.request_id)
        print(json.dumps(r,ensure_ascii=False))
        return 0 if r.get("ok") is True else 1
    except (OSError,ValueError,RuntimeError,TimeoutError) as e:
        p.exit(2,str(e)+"\n")
if __name__=="__main__": raise SystemExit(main())
