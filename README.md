# Raybun

Raybun is a simple, distributed C-based raytracer. It renders 3D scenes defined in JSON files and supports distributed rendering across multiple machines (Master-Worker style).

![Demo Render](./data/simple_scene.png)

## How it works

The core is a tile-based path tracer. It takes a JSON scene description (check out `data/scene_schema.json` for the format) which defines the camera, materials, and objects (spheres, quads, triangles, boxes, etc.).

A small scene example:

```json
{
  "config": {
    "width": 400, "height": 300, 
    "samples_per_pixel": 10, "max_depth": 5
  },
  "camera": {
    "position": [0, 0, -5], "look_at": [0, 0, 0], "up": [0, 1, 0],
    "fov": 60, "aspect_ratio": "4/3", "defocus_angle": 0, "focus_dist": 1
  },
  "materials": [
    { "type": "lambertian", "albedo": [0.8, 0.3, 0.3] },
    { "type": "metal", "albedo": [0.8, 0.8, 0.8], "fuzz": 0.1 }
  ],
  "objects": {
    "sphere": [{ "center": [0, 0, 0], "radius": 1, "material": 0 }],
    "models": [{ "file": "data/suzanne/model.obj", "material": 1, "position": [2, 0, 0], "scale": 1 }],
    "plane": [], "triangle": [], "quad": [], "boxes": []
  }
}
```

### Distributed Rendering (UNDER WORK)
It uses a **Master-Worker** architecture over HTTP/JSON:

1.  **Master** starts up, loads the scene, and listens on a port.
2.  **Workers** connect to the master.
3.  First thing a worker does is run a quick **benchmark** to calculate a performance score.
4.  Workers send their machine info (thread count, perf score) to the Master.
5.  Master breaks the image into tiles and hands them out. Workers render tiles and POST the pixels back.

It uses `libmicrohttpd` for the server and `libcurl` for the client.

## Building & Running

You'll need `libcurl` installed.

```bash

make

# Run benchmark:
./build/raybun benchmark

# Run standalone:
./build/raybun standalone data/simple_scene.json output.jpg

# Run a master node:
./build/raybun master 3000 data/simple_scene.json output.ppm

# Run worker node(s):
./build/raybun worker http://localhost:3000 worker1_name

```
