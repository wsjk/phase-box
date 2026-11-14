# Phase Box Preset Binary Format Reference (Version 3)

## Header & Magic Bytes

| Offset (Bytes) | Type | Field | Valid Range / Value |
| :--- | :--- | :--- | :--- |
| `0..3` | `uint32_t` | Magic Identifier | `0x31584250` (ASCII `"PBX1"`, little-endian) |
| `4..5` | `uint16_t` | Format Version | `3` (v3 introduced `turing_mix`) |

## Global Parameter Payload

| Field | Type | Domain / Constraints |
| :--- | :--- | :--- |
| `shape` | `int32_t` | $0 \le \text{shape} \le 3$ (Sine, Saw, Square, Triangle) |
| `bpm` | `float` (IEEE 754) | $20.0 \le \text{bpm} \le 300.0$ |
| `phase_offset` | `float` | Finite |
| `pm_depth` | `float` | Finite |
| `midi_channel` | `int32_t` | $1 \le \text{channel} \le 16$ |
| `midi_cc_num` | `int32_t` | $0 \le \text{cc} \le 127$ |
| `mutation_prob`| `float` | $0.0 \le \text{prob} \le 1.0$ |
| `turing_mix` | `float` | $0.0 \le \text{mix} \le 1.0$ |
| `macro_count` | `uint32_t` | $0 < \text{count} \le 16$ (Current hardware = 3) |

## Per-Destination Struct (Repeated `macro_count` times)

| Field | Type | Domain / Constraints |
| :--- | :--- | :--- |
| `cc_number` | `int32_t` | $0 \le \text{cc} \le 127$ |
| `midi_channel` | `int32_t` | $1 \le \text{channel} \le 16$ |
| `scale` | `float` | Finite (typically $-2.0$ to $+2.0$) |
| `offset` | `float` | Finite (typically $-1.0$ to $+1.0$) |
| `clock_mult` | `float` | Finite positive |

## Trailer Checksum

| Field | Type | Description |
| :--- | :--- | :--- |
| `crc32` | `uint32_t` | Standard IEEE 802.3 32-bit Cyclic Redundancy Check evaluated across all preceding bytes (`0` to `end - 4`). |
