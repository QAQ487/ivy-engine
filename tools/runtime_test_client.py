"""Bounded local-only Windows named-pipe client (stdlib; no process launcher)."""
import argparse
import ctypes as C
import json
import math
import re
import sys
import time

# Pipe transport is Windows-only; pure helpers below stay importable on Linux
# for offline validation unit tests.
K=None
W=None
INVALID=None
if sys.platform == "win32":
    from ctypes import wintypes as W
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
else:
    class OVERLAPPED(C.Structure):
        _fields_=[]
MAX_EVENTS=64

MAX_MOUSE_DURATION_MS=30000

def build_mouse_payload(dx, dy, buttons, updates=None, wheel=0, x=None, y=None, duration_ms=None):
    """Validate mouse fields and return the JSON payload (no I/O).

    Exactly one of updates (frame hold) or duration_ms (wall-clock hold) is required.
    """
    if type(dx) is not int or not -600<=dx<=600:
        raise ValueError("dx must be an integer in [-600,600]")
    if type(dy) is not int or not -600<=dy<=600:
        raise ValueError("dy must be an integer in [-600,600]")
    allowed={"LEFT","RIGHT","MIDDLE"}
    if not isinstance(buttons,(list,tuple)) or len(buttons)>3 or any(not isinstance(b,str) or b not in allowed for b in buttons) or len(set(buttons))!=len(buttons):
        raise ValueError("buttons must be unique subset of LEFT,RIGHT,MIDDLE")
    if type(wheel) is not int or not -120<=wheel<=120:
        raise ValueError("wheel must be an integer in [-120,120]")
    has_updates=updates is not None
    has_duration=duration_ms is not None
    if has_updates==has_duration:
        raise ValueError("mouse requires exactly one of updates or duration_ms")
    if has_updates:
        if type(updates) is not int or not 1<=updates<=120:
            raise ValueError("updates must be an integer in [1,120]")
    else:
        if type(duration_ms) is not int or not 1<=duration_ms<=MAX_MOUSE_DURATION_MS:
            raise ValueError("duration_ms must be an integer in [1,%d]" % MAX_MOUSE_DURATION_MS)
    if (x is None) != (y is None):
        raise ValueError("absolute cursor requires both x and y (backbuffer pixels)")
    payload={"dx":dx,"dy":dy,"buttons":list(buttons),"wheel":wheel}
    if has_updates:
        payload["updates"]=updates
    else:
        payload["durationMs"]=duration_ms
    if x is not None:
        if type(x) is not int or type(y) is not int or x < 0 or y < 0:
            raise ValueError("x,y must be non-negative integers (backbuffer pixels; server enforces size)")
        payload["x"]=x
        payload["y"]=y
    return payload

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
        # Bound must accept a full 256-frame controller_probe reply and still
        # reject anything larger. Size is derived in runtime_bridge_controller_probe.
        from runtime_bridge_controller_probe import RESPONSE_BYTE_LIMIT, accumulate_pipe_response
        deadline=deadline if deadline is not None else time.monotonic()+self.timeout
        def chunks():
            while True:
                # Chunked reads keep large probe replies inside the bridge WriteFile timeout.
                yield self._io(size=4096,deadline=deadline)
        return accumulate_pipe_response(chunks(), max_bytes=RESPONSE_BYTE_LIMIT)
    def request(self,verb,request_id,payload=None,*,deadline=None):
        deadline=min(deadline if deadline is not None else math.inf,time.monotonic()+self.timeout)
        q=dict(payload or {})
        q.update(protocolVersion=1,requestId=request_id,runId=self.run_id,verb=verb)
        self.send_raw(json.dumps(q,separators=(",",":")).encode()+b"\n",deadline)
        r=self.read_response(deadline)
        if r.get("protocolVersion")!=1 or r.get("runId")!=self.run_id or r.get("requestId")!=request_id:
            raise RuntimeError("response identity mismatch")
        # Matching identity + ok:false is a bridge rejection; return it so callers
        # can read error without raw send/read. Genuine identity mismatches stay above.
        if r.get("ok") is False:
            return r
        if verb in ("hello","status") and r.get("pid")!=self.pid:
            raise RuntimeError("response PID mismatch")
        if verb=="quit":
            if r.get("requiresReceipt") is not True: raise RuntimeError("missing quit receipt contract")
            self.send_raw(b"\n",deadline)
        return r

    def observe(self, target, request_id):
        if not isinstance(target, str) or not (1 <= len(target) <= 64):
            raise ValueError("target must be a non-empty string <= 64 chars")
        return self.request("observe", request_id, {"target": target})

    @staticmethod
    def _valid_object_name(name):
        if not isinstance(name, str) or not (1 <= len(name) <= 64):
            return False
        return all(ch.isalnum() or ch in "_-" for ch in name)

    @staticmethod
    def _valid_vec(values, length):
        if not isinstance(values, (list, tuple)) or len(values) != length:
            return False
        for v in values:
            if isinstance(v, bool) or not isinstance(v, (int, float)) or not math.isfinite(v):
                return False
        return True

    def _await_world_op(self, request_id, verb, payload, completion_timeout=5.0):
        if type(request_id) is not int or not 1 <= request_id < 2147483647:
            raise ValueError("request ID must leave room for completion polls")
        if not math.isfinite(completion_timeout) or not 0 < completion_timeout <= 10:
            raise ValueError("completion timeout must be in (0,10]")
        try:
            deadline = time.monotonic() + completion_timeout
            result = self.request(verb, request_id, payload, deadline=deadline)
            if not result.get("ok"):
                return result
            operation_id = request_id
            while time.monotonic() < deadline:
                request_id += 1
                if request_id > 2147483647:
                    raise ValueError("request ID exhausted")
                result = self.request("status", request_id, deadline=deadline)
                if not result.get("ok"):
                    raise RuntimeError("worldOp status rejected: " + str(result))
                operation = result.get("worldOp", {})
                if operation.get("requestId") != operation_id:
                    raise RuntimeError("worldOp operation identity mismatch")
                if operation.get("status") == "completed":
                    return result
                if operation.get("status") == "failed":
                    raise RuntimeError("worldOp failed: " + str(operation.get("error")))
                time.sleep(min(.01, max(0, deadline - time.monotonic())))
            raise TimeoutError("worldOp completion deadline")
        except (OSError, ValueError, RuntimeError, TimeoutError):
            self.close()
            raise

    def reset_scene(self, request_id, scene=None, completion_timeout=8.0):
        payload = {}
        if scene is not None:
            if not isinstance(scene, str) or not scene.startswith("rom/") or ".." in scene.split("/") or not scene.lower().endswith(".json"):
                raise ValueError("scene must be a rom/...json path without '..'")
            payload["scene"] = scene
        return self._await_world_op(request_id, "reset_scene", payload or None, completion_timeout)

    def set_transform(self, object_name, request_id, position=None, rotation=None, scale=None, zero_velocity=True, completion_timeout=5.0):
        if not self._valid_object_name(object_name):
            raise ValueError("object must be a 1..64 alnum/_/- name")
        if not isinstance(zero_velocity, bool):
            raise ValueError("zero_velocity must be bool")
        payload = {"object": object_name, "zeroVelocity": zero_velocity}
        if position is not None:
            if not self._valid_vec(position, 3):
                raise ValueError("position must be 3 finite numbers")
            payload["position"] = [float(position[0]), float(position[1]), float(position[2])]
        if rotation is not None:
            if self._valid_vec(rotation, 3):
                payload["rotation"] = [float(rotation[0]), float(rotation[1]), float(rotation[2])]
            elif self._valid_vec(rotation, 4):
                if abs(rotation[0]) + abs(rotation[1]) + abs(rotation[2]) + abs(rotation[3]) <= 1e-12:
                    raise ValueError("rotation quaternion must be non-zero")
                payload["rotation"] = [float(rotation[0]), float(rotation[1]), float(rotation[2]), float(rotation[3])]
            else:
                raise ValueError("rotation must be 3 (euler deg) or 4 (quaternion) finite numbers")
        if scale is not None:
            if not self._valid_vec(scale, 3):
                raise ValueError("scale must be 3 finite numbers")
            payload["scale"] = [float(scale[0]), float(scale[1]), float(scale[2])]
        if "position" not in payload and "rotation" not in payload and "scale" not in payload:
            raise ValueError("set_transform requires position, rotation, and/or scale")
        return self._await_world_op(request_id, "set_transform", payload, completion_timeout)

    def set_velocity(self, object_name, request_id, linear=None, angular=None, completion_timeout=5.0):
        if not self._valid_object_name(object_name):
            raise ValueError("object must be a 1..64 alnum/_/- name")
        payload = {"object": object_name}
        if linear is not None:
            if not self._valid_vec(linear, 3):
                raise ValueError("linear must be 3 finite numbers")
            payload["linear"] = [float(linear[0]), float(linear[1]), float(linear[2])]
        if angular is not None:
            if not self._valid_vec(angular, 3):
                raise ValueError("angular must be 3 finite numbers")
            payload["angular"] = [float(angular[0]), float(angular[1]), float(angular[2])]
        if "linear" not in payload and "angular" not in payload:
            raise ValueError("set_velocity requires linear and/or angular")
        return self._await_world_op(request_id, "set_velocity", payload, completion_timeout)

    def apply_impulse(self, object_name, impulse, request_id, point=None, completion_timeout=5.0):
        if not self._valid_object_name(object_name):
            raise ValueError("object must be a 1..64 alnum/_/- name")
        if not self._valid_vec(impulse, 3):
            raise ValueError("impulse must be 3 finite numbers")
        payload = {"object": object_name, "impulse": [float(impulse[0]), float(impulse[1]), float(impulse[2])]}
        if point is not None:
            if not self._valid_vec(point, 3):
                raise ValueError("point must be 3 finite numbers")
            payload["point"] = [float(point[0]), float(point[1]), float(point[2])]
        return self._await_world_op(request_id, "apply_impulse", payload, completion_timeout)

    def apply_torque_impulse(self, object_name, torque_impulse, request_id, completion_timeout=5.0):
        if not self._valid_object_name(object_name):
            raise ValueError("object must be a 1..64 alnum/_/- name")
        if not self._valid_vec(torque_impulse, 3):
            raise ValueError("torque_impulse must be 3 finite numbers")
        payload = {"object": object_name, "torqueImpulse": [float(torque_impulse[0]), float(torque_impulse[1]), float(torque_impulse[2])]}
        return self._await_world_op(request_id, "apply_torque_impulse", payload, completion_timeout)

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
        from runtime_bridge_keyboard_keys import validate_keyboard_keys
        validate_keyboard_keys(keys)
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

    def mouse(self,dx,dy,buttons,updates,request_id,wheel=0,completion_timeout=None,x=None,y=None,duration_ms=None):
        payload=build_mouse_payload(dx,dy,buttons,updates,wheel=wheel,x=x,y=y,duration_ms=duration_ms)
        if type(request_id) is not int or not 1<=request_id<2147483647:
            raise ValueError("request ID must leave room for completion polls")
        if completion_timeout is None:
            completion_timeout=7.0 if duration_ms is None else (duration_ms/1000.0)+7.0
        max_completion=10.0 if duration_ms is None else 60.0
        if not math.isfinite(completion_timeout) or not 0<completion_timeout<=max_completion:
            raise ValueError("completion timeout must be in (0,%g]" % max_completion)
        try:
            deadline=time.monotonic()+completion_timeout
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
    p.add_argument("verb",choices=("hello","status","quit","keyboard","observe","capture","mouse","controller","events","controller_probe","list_objects","pause","resume","step","perf","reset_scene","set_transform","set_velocity","apply_impulse","apply_torque_impulse"))
    p.add_argument("--keys",nargs="+")
    p.add_argument("--updates",type=int)
    p.add_argument("--duration-ms",type=int,default=None,dest="duration_ms",
                   help="mouse wall-clock hold ms (exclusive with --updates; 1..30000)")
    p.add_argument("--completion-timeout",type=float,default=7)
    p.add_argument("--target",type=str)
    p.add_argument("--object",type=str,dest="object_name")
    p.add_argument("--scene",type=str,default=None)
    p.add_argument("--position",nargs=3,type=float,default=None)
    p.add_argument("--rotation",nargs="+",type=float,default=None)
    p.add_argument("--scale",nargs=3,type=float,default=None)
    p.add_argument("--linear",nargs=3,type=float,default=None)
    p.add_argument("--angular",nargs=3,type=float,default=None)
    p.add_argument("--impulse",nargs=3,type=float,default=None)
    p.add_argument("--point",nargs=3,type=float,default=None)
    p.add_argument("--torque-impulse",nargs=3,type=float,default=None,dest="torque_impulse")
    p.add_argument("--zero-velocity",dest="zero_velocity",action="store_true",default=None)
    p.add_argument("--no-zero-velocity",dest="zero_velocity",action="store_false")
    p.add_argument("--dx",type=int,default=0)
    p.add_argument("--dy",type=int,default=0)
    p.add_argument("--x",type=int,default=None,help="optional absolute backbuffer X (requires --y)")
    p.add_argument("--y",type=int,default=None,help="optional absolute backbuffer Y (requires --x)")
    p.add_argument("--buttons",nargs="*",default=[])
    p.add_argument("--wheel",type=int,default=0)
    p.add_argument("--lx",type=float,default=None)
    p.add_argument("--ly",type=float,default=None)
    p.add_argument("--rx",type=float,default=None)
    p.add_argument("--ry",type=float,default=None)
    p.add_argument("--lt",type=float,default=None)
    p.add_argument("--rt",type=float,default=None)
    p.add_argument("--offset",type=int,default=None)
    p.add_argument("--limit",type=int,default=None)
    p.add_argument("--frames",type=int,default=None)
    a=p.parse_args()
    if a.verb=="keyboard" and (a.keys is None or a.updates is None): p.error("keyboard requires --keys and --updates")
    if a.verb!="keyboard" and (a.keys is not None): p.error("keys payload requires keyboard verb")
    if a.verb=="mouse":
        if (a.updates is None) == (a.duration_ms is None):
            p.error("mouse requires exactly one of --updates or --duration-ms")
    elif a.duration_ms is not None:
        p.error("--duration-ms requires mouse verb")
    if a.verb=="controller" and (a.lx is None or a.ly is None or a.updates is None):
        p.error("controller requires --lx, --ly and --updates")
    if a.verb!="controller" and (a.lx is not None or a.ly is not None or a.rx is not None or a.ry is not None or a.lt is not None or a.rt is not None):
        p.error("lx/ly/rx/ry/lt/rt payload requires controller verb")
    if a.verb=="observe" and not a.target: p.error("observe requires --target")
    if a.verb=="step" and a.frames is None: p.error("step requires --frames")
    if a.verb=="perf" and a.frames is None: p.error("perf requires --frames")
    if a.verb!="list_objects" and (a.offset is not None or a.limit is not None):
        p.error("offset/limit require list_objects")
    if a.verb not in ("step","perf") and a.frames is not None:
        p.error("frames requires step or perf")
    if a.verb in ("set_transform","set_velocity","apply_impulse","apply_torque_impulse") and not a.object_name:
        p.error("%s requires --object" % a.verb)
    if a.verb=="apply_impulse" and a.impulse is None: p.error("apply_impulse requires --impulse")
    if a.verb=="apply_torque_impulse" and a.torque_impulse is None: p.error("apply_torque_impulse requires --torque-impulse")
    if a.verb=="set_transform" and a.rotation is not None and len(a.rotation) not in (3, 4):
        p.error("--rotation requires 3 (euler) or 4 (quaternion) values")
    try:
        with Client(a.pid,a.run_id,a.timeout) as c:
            if a.verb=="keyboard":
                r=c.keyboard(a.keys,a.updates,a.request_id,a.completion_timeout)
            elif a.verb=="mouse":
                # Default 7s completion timeout is too short for long durationMs holds;
                # pass None so Client.mouse derives duration_ms/1000+7.
                mouse_ct=None if (a.duration_ms is not None and a.completion_timeout==7) else a.completion_timeout
                r=c.mouse(a.dx,a.dy,a.buttons,a.updates,a.request_id,a.wheel,mouse_ct,x=a.x,y=a.y,duration_ms=a.duration_ms)
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
            elif a.verb=="list_objects":
                payload={}
                if a.offset is not None: payload["offset"]=a.offset
                if a.limit is not None: payload["limit"]=a.limit
                r=c.request(a.verb,a.request_id,payload or None)
            elif a.verb in ("step","perf"):
                r=c.request(a.verb,a.request_id,{"frames":a.frames})
            elif a.verb=="reset_scene":
                r=c.reset_scene(a.request_id,scene=a.scene,completion_timeout=a.completion_timeout)
            elif a.verb=="set_transform":
                zv=True if a.zero_velocity is None else a.zero_velocity
                r=c.set_transform(a.object_name,a.request_id,position=a.position,rotation=a.rotation,scale=a.scale,zero_velocity=zv,completion_timeout=a.completion_timeout)
            elif a.verb=="set_velocity":
                r=c.set_velocity(a.object_name,a.request_id,linear=a.linear,angular=a.angular,completion_timeout=a.completion_timeout)
            elif a.verb=="apply_impulse":
                r=c.apply_impulse(a.object_name,a.impulse,a.request_id,point=a.point,completion_timeout=a.completion_timeout)
            elif a.verb=="apply_torque_impulse":
                r=c.apply_torque_impulse(a.object_name,a.torque_impulse,a.request_id,completion_timeout=a.completion_timeout)
            else:
                r=c.request(a.verb,a.request_id)
        print(json.dumps(r,ensure_ascii=False))
        return 0 if r.get("ok") is True else 1
    except (OSError,ValueError,RuntimeError,TimeoutError) as e:
        p.exit(2,str(e)+"\n")
if __name__=="__main__": raise SystemExit(main())
