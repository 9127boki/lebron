# Game Assets

The game runs without external image files because it draws fallback sprites at startup.

To use your own pictures, place PNG files here with these exact names:

- `james.png`
- `basketball.png`
- `obstacle.png`
- `flying_obstacle.png`
- `reward.png`

Transparent PNGs work best. The code loads these files first and only draws fallback sprites when a file is missing.
