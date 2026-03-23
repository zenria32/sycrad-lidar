
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

KITTI Calibration Integrity
  ───────────────────────────
  ✅ [KITTI-CAL-01] R_velo_to_cam rotation matrix orthogonality (R·Rᵀ = I)
     Measured : 9.00e-08
     Expected : < 1.0e-05

  ✅ [KITTI-CAL-02] R_rect_00 rectification matrix orthogonality (R·Rᵀ = I)
     Measured : 7.86e-08
     Expected : < 1.0e-05

  ✅ [KITTI-CAL-03] R_rect_00 determinant is +1 (proper rotation, no reflection)
     Measured : 1.000000
     Expected : ≈ 1.000000


  KITTI Export Accuracy
  ─────────────────────
  ✅ [KITTI-EXP-01] Observation angle (alpha) = atan2(x,z) - rotation_y
     Measured : error = 6.91e-07
     Expected : < 1.0e-03
     Detail   : computed=-2.236419, exported=-2.236420

  ✅ [KITTI-EXP-02] Object z-coordinate is positive (in front of camera)
     Measured : z = 11.427
     Expected : > 0


  KITTI Coordinate Round-Trip
  ───────────────────────────
  ✅ [KITTI-RT-01] Position round-trip: KITTI rect → LIDAR → KITTI rect
     Measured : max |error| = 4.20e-07
     Expected : < 1.0e-03
     Detail   : LIDAR=(11.706, 9.120, -0.716)

  ✅ [KITTI-RT-02] Rotation round-trip: rotation_y → LIDAR yaw → rotation_y
     Measured : |error| = 0.00e+00
     Expected : < 1.0e-05
     Detail   : rotation_y=1.563262 rad (89.57°)


  KITTI 2D Bounding Box Projection
  ────────────────────────────────
  ✅ [KITTI-BBOX-01] 2D bounding box projection (8 corners → P_rect_02 → image)
     Measured : max |pixel error| = 0.00 px
     Expected : < 2.0 px
     Detail   : computed=[0.00, 159.67, 177.19, 314.45], exported=[0.00, 159.67, 177.19, 314.45]


  KITTI Image & Point Cloud
  ─────────────────────────
  ✅ [KITTI-IMG-01] 2D bounding box falls within image bounds (1242×375)
     Measured : fully_inside=True, partially_visible=True
     Expected : partially_visible=True

  ✅ [KITTI-PC-01] LIDAR points inside cuboid axis-aligned bounding box
     Measured : 1278 points
     Expected : > 0
     Detail   : Point cloud total: 122486 points


  nuScenes Export Accuracy
  ────────────────────────
  ✅ [NUSC-EXP-01] Rotation quaternion [w,x,y,z] has unit norm
     Measured : ||q|| = 1.000000
     Expected : ≈ 1.000000
     Detail   : q = [1, 0, 0, 0]

  ✅ [NUSC-EXP-02] Annotation position is in LIDAR sensor frame (not global coordinates)
     Measured : distance from origin = 13.24 m
     Expected : < 200 m
     Detail   : pos = (-3.4437, 12.7798, -0.4540)

  ✅ [NUSC-EXP-03] Dimension order follows nuScenes [width, length, height] convention
     Measured : size = [2.1231, 4.2867, 1.6937]
     Expected : [w, l, h] where w < l for typical vehicles
     Detail   : Export writes [dim.y, dim.x, dim.z] matching nuScenes wlh standard

  ✅ [NUSC-EXP-04] num_lidar_pts matches actual point count inside AABB
     Measured : reported=123, computed=123
     Expected : reported == computed
     Detail   : Point cloud total: 34688 points

  ✅ [NUSC-EXP-05] All required nuScenes annotation fields are present in JSON
     Measured : missing = none
     Expected : no missing fields
     Detail   : Required: token, sample_token, instance_token, category_name, translation, size, rotation, num_lidar_pts, num_radar_pts

  ✅ [NUSC-EXP-06] Annotation token and instance_token are valid UUID format
     Measured : token_valid=True, instance_valid=True
     Expected : both = True


  Timestamp Synchronization
  ─────────────────────────
  ✅ [KITTI-SYNC-01] Image and Velodyne frame counts match (index-based sync)
     Measured : image=108, velodyne=108
     Expected : counts equal

  ✅ [KITTI-SYNC-02] Average time offset between image and velodyne captures
     Measured : 10.5 ms
     Expected : < 100 ms (acceptable for index-based sync)
     Detail   : SYCRAD uses filename index matching, not timestamp matching

  ✅ [NUSC-SYNC-01] Average LIDAR↔CAM_FRONT timestamp offset (closest frame pairs)
     Measured : avg=35.9 ms, max=36.7 ms
     Expected : < 100 ms
     Detail   : LIDAR frames=11, CAM_FRONT frames=11

  ⚠️ [NUSC-SYNC-02] Ego vehicle position drift between LIDAR and CAM_FRONT captures
     Measured : Δposition=0.3323 m, Δtime=36.5 ms
     Expected : informational (ego_pose compensation recommended if > 0.1m)
     Detail   : Without ego_pose compensation, this drift causes pixel-level projection error


  nuScenes LIDAR→Camera Projection
  ────────────────────────────────
  ✅ [NUSC-PROJ-01] LIDAR→Camera projection coverage (full 6-step chain with ego_pose=True)
     Measured : 3220/34688 points in image (9.3%)
     Expected : 5-15% for front camera (typical)
     Detail   : In front of camera: 11071, projected in 1600×900 image: 3220

  ✅ [NUSC-PROJ-02] Visual overlay generated (LIDAR points + cuboid on camera image)
     Measured : saved to test/nuscenes_overlay.png
     Expected : file created
     Detail   : Green wireframe = cuboid, colored dots = LIDAR depth

  ════════════════════════════════════════════════════════════
  Total: 22 tests | ✅ 21 passed | ⚠️ 1 warnings | ❌ 0 failed

## Qt Modifications

Sycrad uses a modified build of **Qt 6.12 QtGui** that adds `drawIndexedIndirect` functionality to QRhi for the **Metal**, **DX12** and **Vulkan(wip)** backends. Upstream QRhi does not expose indirect drawing commands.

This modification enables the octree-based LOD system to issue all visible node draws as a single batched indirect call, which is a meaningful part of the current rendering pipeline — the CPU-side octree traversal produces a list of `DrawIndexedIndirect` command structs that are uploaded to a GPU buffer and executed in one call, avoiding per-node draw call overhead.

The modified Qt headers are shipped in `third_party/QtBase/`. Under the terms of the **LGPL v3** (Qt's license), the source code of these modifications will be published.

## License

This project is licensed under the **GNU Affero General Public License v3.0** — see LICENSE file for the full text.

**Third-party licenses** are listed in `third_party/LICENSES/`.

Qt 6.12 is used under the **LGPL v3**. The modifications made to QtGui (adding `drawIndexedIndirect` to QRhi for Metal, DX12 and Vulkan(wip) backends ) will be published as required by the LGPL.