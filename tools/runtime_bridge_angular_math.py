"""Pure math and validators for runtime-bridge angular acceptance.

No Windows named-pipe / ctypes dependencies — safe to import on Linux offline checks.
"""
from __future__ import annotations

import math

PHYSICS_DT = 1.0 / 60.0

# Tolerances (radians / relative / absolute as documented per check).
TOL_AXIS_DOT = 0.98
TOL_ANG_VEL_ABS = 0.08
TOL_ANG_VEL_REL = 0.08
TOL_LIN_VEL_ABS = 0.08
TOL_QUAT_ANGLE_DEG = 3.0
TOL_QUAT_NORM = 1e-3
TOL_ROLL_SLIP_SHRINK = 0.55  # final slip must be < this * initial slip
TOL_TIP_UP_DOT = 0.92
TOL_RESET_ABS = 1e-3
TOL_RESET_REL = 1e-3

SPHERE_RADIUS = 0.5

KNOWN_LIMITATION_SCENARIOS = (
    {
        "id": "box_friction_rolling",
        "status": "expected-skip",
        "reason": (
            "Box friction torque is disabled in ResolveContact "
            "(linear friction only); sphere/ground pairs remain the supported "
            "rolling path."
        ),
    },
)


def vec3(x, y=None, z=None):
    if y is None and z is None:
        if isinstance(x, (list, tuple)):
            return (float(x[0]), float(x[1]), float(x[2]))
        if isinstance(x, dict):
            return (float(x.get("x", 0.0)), float(x.get("y", 0.0)), float(x.get("z", 0.0)))
        raise TypeError("vec3 expects triple or mapping")
    return (float(x), float(y), float(z))


def quat4(q):
    if isinstance(q, dict):
        return (float(q.get("x", 0.0)), float(q.get("y", 0.0)), float(q.get("z", 0.0)),
                float(q.get("w", 1.0)))
    return (float(q[0]), float(q[1]), float(q[2]), float(q[3]))


def v_add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def v_sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def v_scale(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def v_dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def v_cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def v_len(a):
    return math.sqrt(max(0.0, v_dot(a, a)))


def v_norm(a):
    n = v_len(a)
    if n < 1e-12:
        return (0.0, 0.0, 0.0)
    return v_scale(a, 1.0 / n)


def nearly_equal(a, b, abs_tol, rel_tol=0.0):
    return abs(a - b) <= abs_tol + rel_tol * max(abs(a), abs(b))


def vec_nearly_equal(a, b, abs_tol, rel_tol=0.0):
    return all(nearly_equal(a[i], b[i], abs_tol, rel_tol) for i in range(3))


def angular_drag_factor(angular_drag, dt=PHYSICS_DT):
    """Match Rigidbody::Integrate: 1 / (1 + angularDrag * dt)."""
    drag = max(0.0, float(angular_drag))
    return 1.0 / (1.0 + drag * float(dt))


def expected_ang_vel_after_steps(w0, angular_drag, steps, dt=PHYSICS_DT):
    factor = angular_drag_factor(angular_drag, dt)
    scale = factor ** int(steps)
    return v_scale(w0, scale)


def apply_inv_inertia_local(inv_inertia, torque_world):
    """Identity-orientation I^-1 * L (principal axes aligned with world)."""
    inv = vec3(inv_inertia)
    t = vec3(torque_world)
    return (inv[0] * t[0], inv[1] * t[1], inv[2] * t[2])


def quat_mul(q, p):
    """Hamilton product q * p with (x,y,z,w)."""
    x1, y1, z1, w1 = q
    x2, y2, z2, w2 = p
    return (
        w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2,
        w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2,
        w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2,
        w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2,
    )


def quat_add(q, p):
    return (q[0] + p[0], q[1] + p[1], q[2] + p[2], q[3] + p[3])


def quat_scale(q, s):
    return (q[0] * s, q[1] * s, q[2] * s, q[3] * s)


def quat_norm(q):
    n = math.sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3])
    if n < 1e-12:
        return (0.0, 0.0, 0.0, 1.0)
    return (q[0] / n, q[1] / n, q[2] / n, q[3] / n)


def quat_len(q):
    return math.sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3])


def integrate_quat_step(q, omega, dt=PHYSICS_DT):
    """Match Rigidbody::Integrate: q' = normalize(q + 0.5*dt*(omega_quat * q))."""
    omega_q = (omega[0], omega[1], omega[2], 0.0)
    dq = quat_scale(quat_mul(omega_q, q), 0.5 * dt)
    return quat_norm(quat_add(q, dq))


def integrate_quat_n(q0, samples_omega, dt=PHYSICS_DT):
    """Integrate quaternion over N steps using per-step omega samples (length N)."""
    q = quat_norm(q0)
    for omega in samples_omega:
        q = integrate_quat_step(q, omega, dt)
    return q


def quat_angle_deg(q_a, q_b):
    """Smallest angle between two unit quaternions in degrees."""
    a = quat_norm(q_a)
    b = quat_norm(q_b)
    dot = abs(a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3])
    dot = min(1.0, max(0.0, dot))
    return math.degrees(2.0 * math.acos(dot))


def body_up_axis_from_quat(q):
    """World-space +Y axis of the body (column 1 of rotation matrix from quat)."""
    x, y, z, w = quat_norm(q)
    # R * (0,1,0)
    return (
        2.0 * (x * y - z * w),
        1.0 - 2.0 * (x * x + z * z),
        2.0 * (y * z + x * w),
    )


def rolling_slip(vx, wz, radius):
    """|v − ω×r| slip speed for planar +X rolling on Y-up ground.

    Engine physics uses a right-handed cross product (Rigidbody ``Cross3``, same
    as DirectXMath ``XMVector3Cross`` / scenario-2 ``r × J``). Contact lever from
    COM to ground is ``r = (0, −R, 0)``. With ``ω = (0, 0, wz)``:

        ω × r = (wz * R, 0, 0)

    No-slip requires ``v_contact = v + ω×r = 0`` ⇒ ``vx = −wz·R`` ⇒ ``wz = −vx/R``.
    So for ``+vx``, expected ``wz`` is **negative**. Slip magnitude is ``|vx + wz·R|``.
    """
    return abs(float(vx) + float(wz) * float(radius))


def expected_rolling_wz_sign(vx):
    """Sign of wz for rolling without slip under engine RH Y-up convention.

    Returns -1 when +vx expects wz < 0, +1 when −vx expects wz > 0, else 0.
    """
    if vx > 1e-6:
        return -1
    if vx < -1e-6:
        return 1
    return 0


def pick_rolling_axis_sample(samples, min_abs_wz=0.05):
    """First sample with meaningful |wz| while still translating (not post-stop)."""
    for sample in samples:
        if abs(float(sample.get("wz", 0.0))) >= min_abs_wz and abs(float(sample.get("vx", 0.0))) >= 0.05:
            return sample
    for sample in samples:
        if abs(float(sample.get("wz", 0.0))) >= min_abs_wz:
            return sample
    return samples[0]


def validate_free_spin(obs_after_impulse, torque, steps, obs_after_steps):
    """Assert ω ≈ I^-1 L (axis/sign/magnitude) and damping-only decay over steps."""
    rb = obs_after_impulse.get("rigidbody") or {}
    inv = rb.get("localInverseInertia")
    if not inv:
        return {"pass": False, "error": "missing localInverseInertia"}
    ang_drag = float(rb.get("angularDrag", 0.0))
    w0 = vec3(obs_after_impulse.get("angularVelocity"))
    expected0 = apply_inv_inertia_local(inv, torque)
    axis_ok = v_dot(v_norm(w0), v_norm(expected0)) >= TOL_AXIS_DOT
    mag_ok = nearly_equal(v_len(w0), v_len(expected0), TOL_ANG_VEL_ABS, TOL_ANG_VEL_REL)
    w_n = vec3(obs_after_steps.get("angularVelocity"))
    expected_n = expected_ang_vel_after_steps(w0, ang_drag, steps)
    decay_ok = vec_nearly_equal(w_n, expected_n, TOL_ANG_VEL_ABS, TOL_ANG_VEL_REL)
    return {
        "pass": bool(axis_ok and mag_ok and decay_ok),
        "axisOk": axis_ok,
        "magOk": mag_ok,
        "decayOk": decay_ok,
        "w0": w0,
        "expected0": expected0,
        "wN": w_n,
        "expectedN": expected_n,
        "angularDrag": ang_drag,
        "steps": steps,
    }


def validate_off_center_impulse(obs_before, obs_after, impulse, point_world, mass, inv_inertia):
    """Linear Δv = J/m; angular axis matches r×J; |ω| ≈ |I^-1 (r×J)| at identity."""
    v0 = vec3(obs_before.get("velocity"))
    w0 = vec3(obs_before.get("angularVelocity"))
    v1 = vec3(obs_after.get("velocity"))
    w1 = vec3(obs_after.get("angularVelocity"))
    pos = vec3(obs_before.get("position"))
    r = v_sub(vec3(point_world), pos)
    j = vec3(impulse)
    expected_dv = v_scale(j, 1.0 / float(mass))
    dv = v_sub(v1, v0)
    lin_ok = vec_nearly_equal(dv, expected_dv, TOL_LIN_VEL_ABS, TOL_ANG_VEL_REL)
    tau = v_cross(r, j)
    expected_dw = apply_inv_inertia_local(inv_inertia, tau)
    dw = v_sub(w1, w0)
    axis_ok = v_len(tau) < 1e-8 or v_dot(v_norm(dw), v_norm(expected_dw)) >= TOL_AXIS_DOT
    mag_ok = nearly_equal(v_len(dw), v_len(expected_dw), TOL_ANG_VEL_ABS, TOL_ANG_VEL_REL)
    both_changed = v_len(dv) > 1e-4 and v_len(dw) > 1e-4
    return {
        "pass": bool(lin_ok and axis_ok and mag_ok and both_changed),
        "linOk": lin_ok,
        "axisOk": axis_ok,
        "magOk": mag_ok,
        "bothChanged": both_changed,
        "r": r,
        "tau": tau,
        "dv": dv,
        "expectedDv": expected_dv,
        "dw": dw,
        "expectedDw": expected_dw,
    }


def validate_rotation_integration(q0, omega_samples, q_final, dt=PHYSICS_DT):
    predicted = integrate_quat_n(q0, omega_samples, dt)
    observed = quat_norm(q_final)
    angle = quat_angle_deg(predicted, observed)
    norm_ok = abs(quat_len(q_final) - 1.0) <= TOL_QUAT_NORM
    angle_ok = angle <= TOL_QUAT_ANGLE_DEG
    return {
        "pass": bool(angle_ok and norm_ok),
        "angleDeg": angle,
        "norm": quat_len(q_final),
        "normOk": norm_ok,
        "angleOk": angle_ok,
        "predicted": predicted,
        "observed": observed,
    }


def validate_rolling(samples, radius=SPHERE_RADIUS):
    """Assert ω develops with engine RH Y-up rolling sign and slip shrinks.

    Axis check uses the first sample with meaningful |wz| (not a mid/late sample
    after the sphere has already decelerated to rest — Terry Windows run at
    e55467d failed that way while early samples already had the correct ``wz < 0``
    for ``+vx``). Convention is documented on ``rolling_slip`` / matched by
    scenario-2 ``r × J`` (right-handed ``Cross3``).
    """
    if len(samples) < 2:
        return {"pass": False, "error": "need >=2 samples"}
    first = samples[0]
    last = samples[-1]
    axis_sample = pick_rolling_axis_sample(samples)
    vx = float(axis_sample.get("vx", 0.0))
    wz = float(axis_sample.get("wz", 0.0))
    # Prefer sign from the axis sample's own vx; fall back to the first sample.
    sign_ref_vx = vx if abs(vx) >= 0.05 else float(first.get("vx", 0.0))
    want = expected_rolling_wz_sign(sign_ref_vx)
    if want < 0:
        axis_ok = wz < -0.05
    elif want > 0:
        axis_ok = wz > 0.05
    else:
        axis_ok = abs(wz) > 0.05
    slip0 = rolling_slip(first["vx"], first["wz"], radius)
    slip1 = rolling_slip(last["vx"], last["wz"], radius)
    # If already nearly rolling at start, require slip stays small.
    if slip0 < 0.15:
        slip_ok = slip1 <= slip0 + 0.05
    else:
        slip_ok = slip1 < slip0 * TOL_ROLL_SLIP_SHRINK
    return {
        "pass": bool(axis_ok and slip_ok),
        "axisOk": axis_ok,
        "slipOk": slip_ok,
        "slip0": slip0,
        "slipN": slip1,
        "axisSample": axis_sample,
        "expectedWzSign": want,
        "convention": "RH_Yup_wz_eq_minus_vx_over_R",
        "samples": samples,
    }


def validate_tipping(obs_final, min_up_dot=TOL_TIP_UP_DOT):
    q = quat4(obs_final.get("rotationQuaternion") or {})
    up = body_up_axis_from_quat(q)
    # Settled flat: body +Y near world +Y, or -Y if upside-down is somehow stable
    # (we expect upright from a mild tilt).
    dot = v_dot(v_norm(up), (0.0, 1.0, 0.0))
    return {
        "pass": bool(dot >= min_up_dot),
        "upDot": dot,
        "up": up,
        "quat": q,
        "position": obs_final.get("position"),
        "angularVelocity": obs_final.get("angularVelocity"),
    }


def validate_reset_determinism(a, b, abs_tol=TOL_RESET_ABS, rel_tol=TOL_RESET_REL):
    wa = vec3(a.get("angularVelocity"))
    wb = vec3(b.get("angularVelocity"))
    va = vec3(a.get("velocity"))
    vb = vec3(b.get("velocity"))
    ang_ok = vec_nearly_equal(wa, wb, abs_tol, rel_tol)
    lin_ok = vec_nearly_equal(va, vb, abs_tol, rel_tol)
    return {
        "pass": bool(ang_ok and lin_ok),
        "angOk": ang_ok,
        "linOk": lin_ok,
        "a": {"velocity": va, "angularVelocity": wa},
        "b": {"velocity": vb, "angularVelocity": wb},
    }


