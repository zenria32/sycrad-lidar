
## Overview

**Native 3D LiDAR Annotation & Visualization Tool**

*Built for autonomous vehicles, robotics, and large-scale mapping — entirely on your machine.*

Sycrad is a native desktop application for visualizing and annotating 3D LiDAR point cloud data. It is built in **C++20** using a **custom rendering pipeline** on top of Qt 6's QRhi abstraction layer.

Most annotation tools in the autonomous driving space are either browser-based or cloud-dependent, which introduces latency, peformance bottleneck and data-privacy concerns. Sycrad takes a different approach: everything runs **locally**, with **native GPU acceleration**, allowing large-scale point clouds to be loaded and annotated on a single workstation without uploading data anywhere.

> **Status:** Alpha — The annotation workflow is functional and export/import pipelines for KITTI and nuScenes are working. See the roadmap for planned additions.

## Features

### Rendering Engine
- **QRhi-based renderer** — Runs on Metal (macOS), Direct3D 12 (Windows) and Vulkan(wip) (Linux) through Qt's hardware abstraction layer. No OpenGL dependency.
- **Octree-accelerated LOD** — Point clouds are spatially indexed into an octree at load time. Screen-space error metric determines the appropriate level of detail per node each frame.
- **Frustum culling** — Nodes outside the camera frustum are discarded before any draw call is issued.
- **Indirect indexed drawing** — Visible octree nodes are batched into a single GPU-side `drawIndexedIndirect` call, minimizing per-frame CPU-to-GPU overhead. This required adding `drawIndexedIndirect` support to QRhi for Metal, DX12 and Vulkan(wip).
- **Event-driven rendering** — Frames are only rendered when the scene state changes (camera movement, data load, annotation edit), keeping idle GPU/CPU usage near zero.

### Annotation
- **3D cuboid annotations** — Create, select, move, rotate, and scale bounding boxes directly in the 3D viewport using a gizmo system.
- **Undo/Redo** — Full undo stack for all annotation operations.
- **Auto-save** — Annotations are periodically saved to prevent data loss.
- **Orthographic views** — Top, front, and side orthographic viewports show a clipped region around the selected cuboid for precise adjustments.
- **Camera overlay** — When calibration and camera image directories are provided, the corresponding camera frame is displayed alongside the LiDAR viewport, with automatic channel selection based on view direction.

### Data Pipeline
- **KITTI** — calibration and annotation support
- **NuScenes** — calibration and annotation support
- **LAS / LAZ** — missing point formats are under work
- **Waymo** — .tfrecord parser is under work
- **PCD** — ASCII, binary, and binary-compressed supported

### UI
- Project-based workflow with dataset format selection and path configuration.
- File explorer with frame-by-frame navigation.
- Property editor for cuboid attributes (class, position, rotation, dimensions).
- Real-time statistics overlay: FPS, frame time, render resolution, point count, VRAM/RAM usage.
- Dark theme.

## Demo Videos

| Platform | Dataset  | Video           |
| -------- | -------- | --------------- |
| macOS    | KITTI    | [LinkedIn →](https://www.linkedin.com/posts/activity-7441656711133736960-PapQ?utm_source=share&utm_medium=member_desktop&rcm=ACoAAFNUg7wBsnUqVb-VQr0vrlDtuaLRkEvJ__Q) |
| macOS    | nuScenes | [LinkedIn →](https://www.linkedin.com/posts/activity-7441657432948346880-DOaQ?utm_source=share&utm_medium=member_desktop&rcm=ACoAAFNUg7wBsnUqVb-VQr0vrlDtuaLRkEvJ__Q) |
| Windows  | LAS/LAZ  | [LinkedIn →](https://www.linkedin.com/posts/activity-7441660780569112576-GdvP?utm_source=share&utm_medium=member_desktop&rcm=ACoAAFNUg7wBsnUqVb-VQr0vrlDtuaLRkEvJ__Q) |

## Basic Performance Benchmark

| Scenario                                             | Result                   |
| ---------------------------------------------------- | ------------------------ |
| 218M-point `.laz` file (LAS/LAZ)                     | **5.13 GB total memory** |
| Event-driven rendering (viewport interaction)        | **~240 FPS**             |
| Test GPU                                             | RTX 4070 Super           |
| Performance and quality depends on LOD agressiveness | Balanced                 |

## Roadmap

### Near-term (High Priority)

- **SLAM integration** — Register multiple sequential frames into a unified coordinate system. Once this is in, a single operator can load 100+ frames of NuScenes or KITTI sequence as a merged high density point cloud and annotate it in one pass.

- **ONNX model deployment (AWF BEVFusion)** — Local AI-assisted annotation. The model runs entirely on the client machine; no data leaves the environment. The inference pipeline will feed a localized voxel volume to the model rather than the full scene, then back-project detections into all registered frames via the rigid-body transform from the SLAM step. Annotate once, propagate across the sequence.

### Mid-term

- **LiDAR-to-camera annotation projection** — Propagate 3D cuboid labels onto camera images using calibration transforms.

- **Waymo `.tfrecord` parser** — Embedded python script for Waymo .tfrecord format.

- **Linux support** — Follows from the Vulkan QRhi backend work described below. Once Vulkan is wired up, Linux becomes a first-class target.

- **SAM3 road / sidewalk segmentation** — Automatic semantic segmentation of ground-plane elements, combined with weather filters (rain and snow point removal) to clean up noisy captures before annotation.

### Long-term

- **Database / data streaming** — Pull frames directly from a connected store rather than local files.

- **LAN server & multi-user collaboration** — Currently Sycrad is single-user only. A local network server will allow teams to share annotation state without sending data outside the network. This matters for organizations operating in air-gapped environments.

- **Gaussian Splatting / Neural Rendering** — Generate photorealistic representations from registered LiDAR and camera data, for both visual inspection and synthetic training data generation. Still in the research phase; concrete details will be published when the design is settled.

## Building

### Requirements

| Dependency   | Version | Notes                               |
| ------------ | ------- | ----------------------------------- |
| C++ compiler | C++20   | MSVC (Windows), Apple Clang (macOS) |
| CMake        | ≥ 3.16  |                                     |
| Ninja        |         |                                     |
| Third Party  |         | All bundled in                      |

### macOS

```bash
git clone https://github.com/zenria32/sycrad-lidar.git
cd sycrad-lidar
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Windows (Use x64 Native Tools Command Prompt for Visual Studio)

```bash
git clone https://github.com/zenria32/sycrad-lidar.git
cd sycrad-lidar
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

On macOS, the output is `Sycrad.app` with all Qt frameworks, plugins, and dependencies bundled into the app bundle automatically.

On Windows, the executable and required DLLs are placed together in the build output directory.

> **Linux** is not yet supported. Vulkan backend support in QRhi is planned, which will enable Linux builds.

Shader compilation is handled automatically by CMake via the bundled `qsb` (Qt Shader Bake) tool. No separate shader build step is required.

## Export Validation

The export pipeline is verified with an automated test suite covering calibration integrity, coordinate transforms, and format compliance. **22 tests total — 21 passed, 1 warning, 0 failed.**

### KITTI

#### Calibration Integrity

| Test | Description | Measured | Threshold |
| ---- | ----------- | -------- | --------- |
| KITTI-CAL-01 | `R_velo_to_cam` rotation matrix orthogonality (R·Rᵀ = I) | 9.00e-08 | < 1.0e-05 |
| KITTI-CAL-02 | `R_rect_00` rectification matrix orthogonality (R·Rᵀ = I) | 7.86e-08 | < 1.0e-05 |
| KITTI-CAL-03 | `R_rect_00` determinant is +1 (proper rotation) | 1.000000 | ≈ 1.000000 |

#### Export Accuracy

| Test | Description | Measured | Threshold |
| ---- | ----------- | -------- | --------- |
| KITTI-EXP-01 | Observation angle: `alpha = atan2(x,z) - rotation_y` | error = 6.91e-07 | < 1.0e-03 |
| KITTI-EXP-02 | Object z-coordinate is positive (in front of camera) | z = 11.427 | > 0 |

#### Coordinate Round-Trip

| Test | Description | Measured | Threshold |
| ---- | ----------- | -------- | --------- |
| KITTI-RT-01 | Position round-trip: KITTI rect → LIDAR → KITTI rect | max &#124;error&#124; = 4.20e-07 | < 1.0e-03 |
| KITTI-RT-02 | Rotation round-trip: `rotation_y` → LIDAR yaw → `rotation_y` | &#124;error&#124; = 0.00e+00 | < 1.0e-05 |

#### 2D Bounding Box Projection

| Test | Description | Measured | Threshold |
| ---- | ----------- | -------- | --------- |
| KITTI-BBOX-01 | 8-corner projection via `P_rect_02` | max pixel error = 0.00 px | < 2.0 px |

#### Image & Point Cloud

| Test | Description | Measured | Threshold |
| ---- | ----------- | -------- | --------- |
| KITTI-IMG-01 | 2D bbox within image bounds (1242×375) | fully inside | partially visible |
| KITTI-PC-01 | LIDAR points inside cuboid AABB | 1278 / 122486 points | > 0 |

### nuScenes

#### Export Accuracy

| Test | Description | Measured | Threshold |
| ---- | ----------- | -------- | --------- |
| NUSC-EXP-01 | Rotation quaternion unit norm | ‖q‖ = 1.000000 | ≈ 1.000000 |
| NUSC-EXP-02 | Position in LIDAR sensor frame | 13.24 m from origin | < 200 m |
| NUSC-EXP-03 | Dimension order `[w, l, h]` convention | `[2.12, 4.29, 1.69]` | w < l for vehicles |
| NUSC-EXP-04 | `num_lidar_pts` matches AABB point count | 123 = 123 | exact match |
| NUSC-EXP-05 | All required annotation fields present | none missing | no missing fields |
| NUSC-EXP-06 | Token and instance token are valid UUIDs | valid | valid |

#### LIDAR → Camera Projection

| Test | Description | Measured | Threshold |
| ---- | ----------- | -------- | --------- |
| NUSC-PROJ-01 | Projection coverage (6-step chain, ego-pose corrected) | 3220 / 34688 (9.3%) | 5–15% typical |
| NUSC-PROJ-02 | Visual overlay generated | `test/nuscenes_overlay.png` | file created |

### Timestamp Synchronization

| Test | Description | Measured | Threshold | Status |
| ---- | ----------- | -------- | --------- | ------ |
| KITTI-SYNC-01 | Image and Velodyne frame counts match | 108 = 108 | equal | Pass |
| KITTI-SYNC-02 | Average image↔velodyne time offset | 10.5 ms | < 100 ms | Pass |
| NUSC-SYNC-01 | Average LIDAR↔CAM_FRONT timestamp offset | avg 35.9 ms, max 36.7 ms | < 100 ms | Pass |
| NUSC-SYNC-02 | Ego position drift between LIDAR and CAM_FRONT | 0.33 m / 36.5 ms | informational | Warn |

> **Note:** NUSC-SYNC-02 will be fixed.

## Qt Modifications

Sycrad uses a modified build of **Qt 6.12 QtGui** that adds `drawIndexedIndirect` functionality to QRhi for the **Metal**, **DX12** and **Vulkan(wip)** backends. Upstream QRhi does not expose indirect drawing commands.

This modification enables the octree-based LOD system to issue all visible node draws as a single batched indirect call, which is a meaningful part of the current rendering pipeline — the CPU-side octree traversal produces a list of `DrawIndexedIndirect` command structs that are uploaded to a GPU buffer and executed in one call, avoiding per-node draw call overhead.

The modified Qt headers are shipped in `third_party/QtBase/`. Under the terms of the **LGPL v3** (Qt's license), the source code of these modifications will be published.

## License

This project is licensed under the **GNU Affero General Public License v3.0** — see LICENSE file for the full text.

**Third-party licenses** are listed in `third_party/LICENSES/`.

Qt 6.12 is used under the **LGPL v3**. The modifications made to QtGui (adding `drawIndexedIndirect` to QRhi for Metal, DX12 and Vulkan(wip) backends ) will be published as required by the LGPL.