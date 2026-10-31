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
MAX_EVENTS=64

def _valid_event_name(name):
    if not isinstance(name,str) or not 1<=len(name)<=64: return False
    return all(ch.isalnum() or ch in "_-" for ch in name)

def validate_events_reply(payload):
    """Fail-closed semantic validator for fixed checkpoint_reached event schema."""
    if not isinstance(payload,dict) or payload.get("ok") is not True:
        return False,"events reply ok is not true"
    if payload.get("protocolVersion")!=1:
        return False,"events protocolVersion mismatch"
    ev=payload.get("events")
    if not isinstance(ev,list):
        return False,"events must be array"
    if len(ev)>MAX_EVENTS:
        return False,"events exceed capacity 64"
    latest=payload.get("latestSequence")
    dropped=payload.get("dropped")
    if type(latest) is not int or latest<0:
        return False,"latestSequence invalid"
    if type(dropped) is not int or dropped<0:
        return False,"dropped invalid"
    if not ev:
        if latest!=0:
            return False,"empty events require latestSequence 0"
        return True,None
    seen=set()
    for idx,item in enumerate(ev):
        if not isinstance(item,dict):
            return False,"event entry not object"
        for key in ("sequence","type","checkpoint","actor","score"):
            if key not in item:
                return False,"event missing "+key
        seq=item["sequence"]
        if type(seq) is not int or seq!=idx+1:
            return False,"event sequence must be contiguous from 1"
        if seq in seen:
            return False,"duplicate event sequence"
        seen.add(seq)
        if item.get("type")!="checkpoint_reached":
            return False,"event type must be checkpoint_reached"
        if item.get("actor")!="Player":
            return False,"event actor must be Player"
        if not _valid_event_name(item.get("checkpoint")):
            return False,"event checkpoint identifier invalid"
        score=item.get("score")
        if type(score) is not int or score!=1:
            return False,"event score must be exactly 1"
    if latest!=ev[-1]["sequence"]:
        return False,"latestSequence must equal last event sequence"
    return True,None

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
    def request(self,verb,request_id,payload=None,*,deadline=None):
        deadline=min(deadline if deadline is not None else math.inf,time.monotonic()+self.timeout)
        q=dict(payload or {})
        q.update(protocolVersion=1,requestId=request_id,runId=self.run_id,verb=verb)
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

    def observe(self, target, request_id):
        if not isinstance(target, str) or not (1 <= len(target) <= 64):
            raise ValueError("target must be a non-empty string <= 64 chars")
        return self.request("observe", request_id, {"target": target})

    def capture(self, request_id, completion_timeout=5.0):
        if type(request_id) is not int or not 1 <= request_id < 2147483647:
            raise ValueError("request ID must leave room for completion polls")
        if not math.isfinite(completion_timeout) or not 0 < completion_timeout <= 10:
            raise ValueError("completion timeout must be in (0,10]")
        try:
            deadline = time.monotonic() + completion_timeout
            result = self.request("capture", request_id, deadline=deadline)
            if not result.get("ok"): return result
            operation_id = request_id
            while time.monotonic() < deadline:
                request_id += 1
                if request_id > 2147483647: raise ValueError("request ID exhausted")
                result = self.request("status", request_id, deadline=deadline)
                if not result.get("ok"): raise RuntimeError("capture status rejected: " + str(result))
                operation = result.get("capture", {})
                if operation.get("requestId") != operation_id: raise RuntimeError("capture operation identity mismatch")
                if operation.get("status") == "completed": return result
                if operation.get("status") == "failed": raise RuntimeError("capture failed: " + str(operation.get("error")))
                time.sleep(min(.01, max(0, deadline - time.monotonic())))
            raise TimeoutError("capture completion deadline")
        except (OSError, ValueError, RuntimeError, TimeoutError):
            self.close()
            raise

    def keyboard(self,keys,updates,request_id,completion_timeout=7.0):
        allowed={"W","A","S","D","SPACE","UP","DOWN","LEFT","RIGHT"}
        if not isinstance(keys,(list,tuple)) or not 1<=len(keys)<=9 or any(not isinstance(k,str) or k not in allowed for k in keys) or len(set(keys))!=len(keys):
            raise ValueError("keys must be unique whitelist names")
        if type(updates) is not int or not 1<=updates<=120:
            raise ValueError("updates must be an integer in [1,120]")
        if type(request_id) is not int or not 1<=request_id<2147483647:
            raise ValueError("request ID must leave room for completion polls")
        if not math.isfinite(completion_timeout) or not 0<completion_timeout<=10:
            raise ValueError("completion timeout must be in (0,10]")
        try:
            deadline=time.monotonic()+completion_timeout
            result=self.request("keyboard",request_id,{"keys":list(keys),"updates":updates},deadline=deadline)
            if not result.get("ok"): return result
            operation_id=request_id
            while time.monotonic()<deadline:
                request_id+=1
                if request_id>2147483647: raise ValueError("request ID exhausted")
                result=self.request("status",request_id,deadline=deadline)
                if not result.get("ok"): raise RuntimeError("keyboard status rejected: "+str(result))
                operation=result.get("keyboard",{})
                if operation.get("requestId")!=operation_id: raise RuntimeError("keyboard operation identity mismatch")
                if operation.get("status")=="completed": return result
                if operation.get("status")=="cancelled": raise RuntimeError("keyboard operation cancelled")
                time.sleep(min(.01,max(0,deadline-time.monotonic())))
            raise TimeoutError("keyboard completion deadline")
        except (OSError,ValueError,RuntimeError,TimeoutError):
            self.close()
            raise

    def mouse(self,dx,dy,buttons,updates,request_id,wheel=0,completion_timeout=7.0):
        if type(dx) is not int or not -600<=dx<=600:
            raise ValueError("dx must be an integer in [-600,600]")
        if type(dy) is not int or not -600<=dy<=600:
            raise ValueError("dy must be an integer in [-600,600]")
        allowed={"LEFT","RIGHT","MIDDLE"}
        if not isinstance(buttons,(list,tuple)) or len(buttons)>3 or any(not isinstance(b,str) or b not in allowed for b in buttons) or len(set(buttons))!=len(buttons):
            raise ValueError("buttons must be unique subset of LEFT,RIGHT,MIDDLE")
        if type(wheel) is not int or not -120<=wheel<=120:
            raise ValueError("wheel must be an integer in [-120,120]")
        if type(updates) is not int or not 1<=updates<=120:
            raise ValueError("updates must be an integer in [1,120]")
        if type(request_id) is not int or not 1<=request_id<2147483647:
            raise ValueError("request ID must leave room for completion polls")
        if not math.isfinite(completion_timeout) or not 0<completion_timeout<=10:
            raise ValueError("completion timeout must be in (0,10]")
        try:
            deadline=time.monotonic()+completion_timeout
            payload={"dx":dx,"dy":dy,"buttons":list(buttons),"wheel":wheel,"updates":updates}
            result=self.request("mouse",request_id,payload,deadline=deadline)
            if not result.get("ok"): return result
            operation_id=request_id
            while time.monotonic()<deadline:
                request_id+=1
                if request_id>2147483647: raise ValueError("request ID exhausted")
                result=self.request("status",request_id,deadline=deadline)
                if not result.get("ok"): raise RuntimeError("mouse status rejected: "+str(result))
                operation=result.get("mouse",{})
                if operation.get("requestId")!=operation_id: raise RuntimeError("mouse operation identity mismatch")
                if operation.get("status")=="completed": return result
                if operation.get("status")=="cancelled": raise RuntimeError("mouse operation cancelled")
                time.sleep(min(.01,max(0,deadline-time.monotonic())))
            raise TimeoutError("mouse completion deadline")
        except (OSError,ValueError,RuntimeError,TimeoutError):
            self.close()
            raise

    def events(self,request_id):
        if type(request_id) is not int or not 1<=request_id<=2147483647:
            raise ValueError("request ID must be an integer in [1,2147483647]")
        result=self.request("events",request_id)
        if not result.get("ok"): return result
        if result.get("protocolVersion")!=1 or result.get("runId")!=self.run_id or result.get("requestId")!=request_id:
            raise RuntimeError("events response identity mismatch")
        ok,err=validate_events_reply(result)
        if not ok:
            raise RuntimeError(err or "events body invalid")
        return result

    def controller_probe(self,request_id):
        if type(request_id) is not int or not 1<=request_id<=2147483647:
            raise ValueError("request ID must be an integer in [1,2147483647]")
        result=self.request("controller_probe",request_id)
        if not result.get("ok"): return result
        if result.get("protocolVersion")!=1 or result.get("runId")!=self.run_id or result.get("requestId")!=request_id:
            raise RuntimeError("controller_probe response identity mismatch")
        from runtime_bridge_controller_probe import validate_probe_reply
        ok,err=validate_probe_reply(result)
        if not ok:
            raise RuntimeError(err or "controller_probe body invalid")
        return result

    def controller(self,lx,ly,updates,request_id,completion_timeout=7.0,rx=None,ry=None,lt=None,rt=None,buttons=None):
        from runtime_bridge_controller_axes import build_controller_payload
        payload=build_controller_payload(lx,ly,updates,rx=rx,ry=ry,lt=lt,rt=rt,buttons=buttons)
        if type(request_id) is not int or not 1<=request_id<2147483647:
            raise ValueError("request ID must leave room for completion polls")
        if isinstance(completion_timeout,bool) or not isinstance(completion_timeout,(int,float)) or not math.isfinite(completion_timeout) or not 0<completion_timeout<=10:
            raise ValueError("completion timeout must be a finite number in (0,10]")
        try:
            deadline=time.monotonic()+completion_timeout
            result=self.request("controller",request_id,payload,deadline=deadline)
            if not result.get("ok"): return result
            operation_id=request_id
            while time.monotonic()<deadline:
                request_id+=1
                if request_id>2147483647: raise ValueError("request ID exhausted")
                result=self.request("status",request_id,deadline=deadline)
                if not result.get("ok"): raise RuntimeError("controller status rejected: "+str(result))
                operation=result.get("controller",{})
                if operation.get("requestId")!=operation_id: raise RuntimeError("controller operation identity mismatch")
                if operation.get("status")=="completed": return result
                if operation.get("status")=="cancelled": raise RuntimeError("controller operation cancelled")
                time.sleep(min(.01,max(0,deadline-time.monotonic())))
            raise TimeoutError("controller completion deadline")
        except (OSError,ValueError,RuntimeError,TimeoutError):
            self.close()
            raise

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--pid",type=int,required=True)
    p.add_argument("--run-id",required=True)
    p.add_argument("--request-id",type=int,required=True,help="strictly increasing across the run")
    p.add_argument("--timeout",type=float,default=2)
    p.add_argument("verb",choices=("hello","status","quit","keyboard","observe","capture","mouse","controller","events","controller_probe"))
    p.add_argument("--keys",nargs="+")
    p.add_argument("--updates",type=int)
    p.add_argument("--completion-timeout",type=float,default=7)
    p.add_argument("--target",type=str)
    p.add_argument("--dx",type=int,default=0)
    p.add_argument("--dy",type=int,default=0)
    p.add_argument("--buttons",nargs="*",default=[])
    p.add_argument("--wheel",type=int,default=0)
    p.add_argument("--lx",type=float,default=None)
    p.add_argument("--ly",type=float,default=None)
    p.add_argument("--rx",type=float,default=None)
    p.add_argument("--ry",type=float,default=None)
    p.add_argument("--lt",type=float,default=None)
    p.add_argument("--rt",type=float,default=None)
    a=p.parse_args()
    if a.verb=="keyboard" and (a.keys is None or a.updates is None): p.error("keyboard requires --keys and --updates")
    if a.verb!="keyboard" and (a.keys is not None): p.error("keys payload requires keyboard verb")
    if a.verb=="mouse" and a.updates is None: p.error("mouse requires --updates")
    if a.verb=="controller" and (a.lx is None or a.ly is None or a.updates is None):
        p.error("controller requires --lx, --ly and --updates")
    if a.verb!="controller" and (a.lx is not None or a.ly is not None or a.rx is not None or a.ry is not None or a.lt is not None or a.rt is not None):
        p.error("lx/ly/rx/ry/lt/rt payload requires controller verb")
    if a.verb=="observe" and not a.target: p.error("observe requires --target")
    try:
        with Client(a.pid,a.run_id,a.timeout) as c:
            if a.verb=="keyboard":
                r=c.keyboard(a.keys,a.updates,a.request_id,a.completion_timeout)
            elif a.verb=="mouse":
                r=c.mouse(a.dx,a.dy,a.buttons,a.updates,a.request_id,a.wheel,a.completion_timeout)
            elif a.verb=="controller":
                r=c.controller(a.lx,a.ly,a.updates,a.request_id,a.completion_timeout,rx=a.rx,ry=a.ry,lt=a.lt,rt=a.rt,buttons=a.buttons or None)
            elif a.verb=="observe":
                r=c.observe(a.target,a.request_id)
            elif a.verb=="capture":
                r=c.capture(a.request_id,a.completion_timeout)
            elif a.verb=="events":
                r=c.events(a.request_id)
            elif a.verb=="controller_probe":
                r=c.controller_probe(a.request_id)
            else:
                r=c.request(a.verb,a.request_id)
        print(json.dumps(r,ensure_ascii=False))
        return 0 if r.get("ok") is True else 1
    except (OSError,ValueError,RuntimeError,TimeoutError) as e:
        p.exit(2,str(e)+"\n")
if __name__=="__main__": raise SystemExit(main())
