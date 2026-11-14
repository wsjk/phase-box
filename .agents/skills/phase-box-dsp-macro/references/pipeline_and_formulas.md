# Phase Box DSP Pipeline & Mathematical Specifications

## 1. Master Phase Accumulation & Tap Tempo

Phase increments every frame based on master BPM ($40.0 - 300.0$ BPM):
$$\Delta t = 16667\ \mu\text{s} \approx 0.016667\text{ s}$$
$$\Delta \theta = \frac{\text{BPM}}{60} \cdot 2\pi \cdot \Delta t$$
$$\theta_{\text{master}} = (\theta_{\text{master}} + \Delta \theta) \pmod{2\pi}$$

### Tap Tempo & Phase Reset (`include/TapTempo.hpp`)
* Measures delta time between successive Footswitch 1 presses.
* Averages last 4 intervals:
  $$\text{BPM} = \frac{60}{\bar{\Delta t}_{\text{tap}}}$$
* Resets $\theta_{\text{master}} \leftarrow 0$ immediately on each tap to phase-align to external live beats.

---

## 2. Polyrhythmic Clock Division & Multipliers

Each macro destination $d \in \{0, 1, 2\}$ tracks its own effective phase scaled by its clock ratio $M_d$:
$$\theta_d = (\theta_{\text{master}} \cdot M_d) \pmod{2\pi}$$

Supported clock multipliers:
$$M_d \in \left\{ \frac{1}{8},\ \frac{1}{4},\ \frac{1}{2},\ \frac{3}{4},\ 1,\ 1.5,\ 2,\ 3,\ 4,\ 8 \right\}$$

---

## 3. Waveform Generation & Phase Modulation

Waveform generation computes normalized amplitude $W(\theta) \in [0.0, 1.0]$:
* **Sine**:
  $$W(\theta) = 0.5 + 0.5 \cdot \sin(\theta + \text{PM} \cdot \cos(\theta))$$
* **Sawtooth**:
  $$W(\theta) = \frac{\theta}{2\pi}$$
* **Square**:
  $$W(\theta) = \begin{cases} 1.0 & \text{if } \theta < \pi \\ 0.0 & \text{otherwise} \end{cases}$$
* **Triangle**:
  $$W(\theta) = \begin{cases} \frac{\theta}{\pi} & \text{if } \theta < \pi \\ 2.0 - \frac{\theta}{\pi} & \text{otherwise} \end{cases}$$

---

## 4. Generative Turing LFSR Blending

The Turing machine outputs normalized value $T \in [0.0, 1.0]$.
When blended with `tur_mix` ($P_{11}$):
$$R_d = (1.0 - \text{tur\_mix}) \cdot W(\theta_d) + \text{tur\_mix} \cdot T$$

---

## 5. Bipolar Scaling, Offset & Expression

Given destination weight $w_d \in [-2.0, +2.0]$, offset $o_d \in [-1.0, +1.0]$, and smoothed expression pedal value $E \in [0.0, 1.0]$:

$$\text{scaled} = (R_d \cdot w_d \cdot E) + o_d$$
$$\text{CC}_d = \text{clamp}\left( \text{round}(\text{scaled} \cdot 127),\ 0,\ 127 \right)$$

---

## 6. Performance Overrides

* **Freeze Mode (`FROZEN`)**:
  Freezes $\Delta \theta \leftarrow 0$. Output holds constant at previous CC values regardless of pedal movements.
* **Bypass Mode (`BYPASS`)**:
  Forces all destinations to $\text{CC}_d \leftarrow 0$. Accumulators continue running in background. Bypass overrides Freeze.
