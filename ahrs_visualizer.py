#!/usr/bin/env python3
"""
AHRS 3D orientation visualizer.

Reads quaternion telemetry from the STM32 AHRS firmware over serial
(format: "q,w,x,y,z\n") and renders a live-rotating cube matching the
board's real-time orientation.

Usage:
    python3 ahrs_visualizer.py [serial_port] [baud]

If serial_port is omitted, the script lists available ports and asks
which one to use. Default baud is 115200 (matches Telemetry_UART_Init
in main.c).

Requires: pyserial, numpy, matplotlib
    pip install pyserial numpy matplotlib
"""

import sys
import serial
import serial.tools.list_ports
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
import mpl_toolkits.mplot3d  # noqa: F401  (registers the 3d projection)

DEFAULT_BAUD = 115200

# ---- cube geometry ----
CUBE_VERTS = np.array([
    [-1, -1, -1], [1, -1, -1], [1, 1, -1], [-1, 1, -1],
    [-1, -1, 1], [1, -1, 1], [1, 1, 1], [-1, 1, 1],
], dtype=float)

CUBE_FACES = [
    [0, 1, 2, 3],  # bottom
    [4, 5, 6, 7],  # top
    [0, 1, 5, 4],  # front
    [2, 3, 7, 6],  # back
    [1, 2, 6, 5],  # right
    [0, 3, 7, 4],  # left
]

FACE_COLORS = ['#e74c3c', '#c0392b', '#3498db', '#2980b9', '#2ecc71', '#27ae60']


def quat_to_rotmat(w, x, y, z):
    """Standard quaternion-to-rotation-matrix conversion (unit quaternion assumed)."""
    n = w * w + x * x + y * y + z * z
    if n < 1e-9:
        return np.eye(3)
    s = 2.0 / n
    wx, wy, wz = s * w * x, s * w * y, s * w * z
    xx, xy, xz = s * x * x, s * x * y, s * x * z
    yy, yz, zz = s * y * y, s * y * z, s * z * z
    return np.array([
        [1 - (yy + zz), xy - wz, xz + wy],
        [xy + wz, 1 - (xx + zz), yz - wx],
        [xz - wy, yz + wx, 1 - (xx + yy)],
    ])


def find_port():
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("No serial ports found. Is the Nucleo plugged in?")
        sys.exit(1)
    print("Available serial ports:")
    for i, p in enumerate(ports):
        print(f"  [{i}] {p.device} - {p.description}")
    idx = input(f"Select port [0-{len(ports) - 1}]: ").strip()
    return ports[int(idx)].device


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else find_port()
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_BAUD

    print(f"Connecting to {port} @ {baud} baud...")
    ser = serial.Serial(port, baud, timeout=0.5)

    fig = plt.figure(figsize=(7, 7))
    ax = fig.add_subplot(111, projection='3d')
    ax.set_xlim(-2, 2)
    ax.set_ylim(-2, 2)
    ax.set_zlim(-2, 2)
    ax.set_box_aspect([1, 1, 1])
    ax.set_title("AHRS Live Orientation")

    poly = Poly3DCollection([], facecolors=FACE_COLORS, edgecolors='black',
                             linewidths=1, alpha=0.9)
    ax.add_collection3d(poly)

    state = {'w': 1.0, 'x': 0.0, 'y': 0.0, 'z': 0.0}

    def read_latest_quat():
        latest = None
        while ser.in_waiting:
            line = ser.readline().decode('ascii', errors='ignore').strip()
            if line.startswith('q,'):
                parts = line.split(',')
                if len(parts) == 5:
                    try:
                        w, x, y, z = (float(p) for p in parts[1:5])
                        latest = (w, x, y, z)
                    except ValueError:
                        pass
        return latest

    def update(_frame):
        latest = read_latest_quat()
        if latest is not None:
            state['w'], state['x'], state['y'], state['z'] = latest

        R = quat_to_rotmat(state['w'], state['x'], state['y'], state['z'])
        rotated = CUBE_VERTS @ R.T

        faces = [[rotated[i] for i in face] for face in CUBE_FACES]
        poly.set_verts(faces)

        ax.set_title(
            f"AHRS Live Orientation  (q = {state['w']:.3f}, {state['x']:.3f}, "
            f"{state['y']:.3f}, {state['z']:.3f})"
        )
        return poly,

    ani = FuncAnimation(fig, update, interval=33, blit=False)
    plt.show()

    ser.close()


if __name__ == '__main__':
    main()
