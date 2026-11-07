# Storyboard & visual design specification

Timeline is frame-exact (30 fps, frames 0-1799). All animation values come from `blender_scripts/motion.py`.

| Scene | Frames | Time | 3-D content | Overlay |
|---|---|---|---|---|
| 1 Hook | 0-209 | 0-7 s | Close-up of gripper holding an aluminium bracket, yaw 0->100->0 deg, camera pulls back to reveal the whole 6-axis arm (DoF f/4->8) | Kinetic title, word-by-word |
| 2 Angles | 210-599 | 7-20 s | Fixed thin X/Y/Z axes at the TCP + thick rotating tool axes x'/y'/z'; three single-axis demos (X 45, Y 45, Z 60 deg) with reset to identity between them; right-hand-rule arcs | "ROTATION ABOUT X/Y/Z", live theta, right-hand-rule note, "one axis at a time - not combined", sequence disclaimer |
| 3 Matrix | 600-1139 | 20-38 s | Tool frame spins 0->360 deg about fixed Z; blue arc + arrowhead track theta; white marker v=[1,0,0]^T (fixed) and orange marker v' = Rz v on x' | Rz(theta) revealed row by row, sin/cos highlighted orange, live numeric matrix with same theta |
| 4 90 deg | 1140-1559 | 38-52 s | Gripper + bracket rotate exactly 90 deg CCW (seen from +Z) about fixed Z; orange x' arrow; bracket then descends into a fixture whose pocket is along Y | theta counter, before/after x-direction legend, Rz(90) matrix, Rz(90) x [1,0,0]^T = [0,1,0]^T |
| 5 Apps | 1560-1799 | 52-60 s | Assembly (1560), welding torch with changing work/travel angle (1620), pick-and-place with 90 deg yaw (1680), closing close-up that returns to the opening pose (identity orientation, same camera as frame 0 -> seamless loop) | Chips, closing title (fades out by the last frame), CTA |

Transitions: 12-frame (S5 internal: 8-frame) fade through black; all kinematic discontinuities (IK re-seeding, prop/tool swaps) happen on the black frame.

Palette: charcoal #0b0c0f, X #f21f1a, Y #2ecc38, Z #1f59fa, white type, orange #ff8c14 for selected math. Type: Inter (OFL) for text, STIX (matplotlib mathtext) for mathematics.
Robot: white painted metal (clear-coat), dark polymer joints, brushed-steel caps/gripper, orange accent stripe - original geometry, no manufacturer branding.
Lighting: 3x2.2 m softbox key, cool + warm rim lights, low blue/orange accent washes, overhead table light, LED strips on the studio wall.
Layout grid (1080x1920): title zone y 70-330, 3-D focus 270-1100, math panel 1136-1696, captions 1740-1850. Camera `shift_y` keeps the TCP above the panel.
