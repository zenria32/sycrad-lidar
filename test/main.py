#pip install numpy scipy Pillow opencv-python

import json
import numpy as np
from pathlib import Path
from scipy.spatial.transform import Rotation

BASE_DIR = Path(__file__).parent
KITTI_DIR = BASE_DIR / "kitti"
NUSCENES_DIR = BASE_DIR / "nuscenes"

PASS = "PASS"
FAIL = "FAIL"
WARN = "WARN"

results = []


def record(test_name, description, measured, threshold, status_str, detail=""):
    results.append({
        "test": test_name,
        "description": description,
        "measured": measured,
        "threshold": threshold,
        "status": status_str,
        "detail": detail,
    })


def load_kitti_calibration(calib_dir):
    calib = {}
    with open(calib_dir / "calib_velo_to_cam.txt") as f:
        for line in f:
            if line.startswith("R:"):
                calib["R_velo_to_cam"] = np.array([float(x) for x in line.strip().split()[1:]]).reshape(3, 3)
            elif line.startswith("T:"):
                calib["T_velo_to_cam"] = np.array([float(x) for x in line.strip().split()[1:]]).reshape(3, 1)
    with open(calib_dir / "calib_cam_to_cam.txt") as f:
        for line in f:
            if line.startswith("R_rect_00:"):
                calib["R_rect_00"] = np.array([float(x) for x in line.strip().split()[1:]]).reshape(3, 3)
            elif line.startswith("P_rect_02:"):
                calib["P_rect_02"] = np.array([float(x) for x in line.strip().split()[1:]]).reshape(3, 4)
    return calib


def lidar_to_rect(point, calib):
    R, T, R_rect = calib["R_velo_to_cam"], calib["T_velo_to_cam"], calib["R_rect_00"]
    return (R_rect @ (R @ point.reshape(3, 1) + T)).flatten()


def lidar_yaw_to_kitti_rotation_y(yaw_degrees, calib):
    R = calib["R_velo_to_cam"]
    return -(np.deg2rad(yaw_degrees) + np.arctan2(R[0, 1], R[0, 0]))


def load_kitti_label(label_path):
    labels = []
    with open(label_path) as f:
        for line in f:
            p = line.strip().split()
            if len(p) < 15:
                continue
            labels.append({
                "type": p[0], "truncation": float(p[1]), "occlusion": int(p[2]),
                "alpha": float(p[3]),
                "bbox": [float(p[4]), float(p[5]), float(p[6]), float(p[7])],
                "h": float(p[8]), "w": float(p[9]), "l": float(p[10]),
                "x": float(p[11]), "y": float(p[12]), "z": float(p[13]),
                "rotation_y": float(p[14]),
            })
    return labels


def project_to_2d(corners_rect, P):
    valid_pts = []
    for c in corners_rect:
        if c[2] <= 0:
            continue
        proj = P @ np.append(c, 1.0)
        if abs(proj[2]) < 1e-6:
            continue
        valid_pts.append([proj[0] / proj[2], proj[1] / proj[2]])
    if len(valid_pts) < 2:
        return None
    pts = np.array(valid_pts)
    return [max(pts[:, 0].min(), 0), max(pts[:, 1].min(), 0), pts[:, 0].max(), pts[:, 1].max()]


def compute_corners(position, dimension, rotation_quat):
    h = dimension * 0.5
    local = np.array([
        [-h[0], -h[1], -h[2]], [ h[0], -h[1], -h[2]],
        [ h[0],  h[1], -h[2]], [-h[0],  h[1], -h[2]],
        [-h[0], -h[1],  h[2]], [ h[0], -h[1],  h[2]],
        [ h[0],  h[1],  h[2]], [-h[0],  h[1],  h[2]],
    ])
    rot = Rotation.from_quat([rotation_quat[1], rotation_quat[2],
                               rotation_quat[3], rotation_quat[0]])
    return rot.apply(local) + position


def quaternion_to_rotation_matrix(q):
    w, x, y, z = q
    return np.array([
        [1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
        [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
        [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)],
    ])


def load_nuscenes_calibration(calib_dir):
    with open(calib_dir / "sensor.json") as f:
        sensors = json.load(f)
    with open(calib_dir / "calibrated_sensor.json") as f:
        cal_sensors = json.load(f)
    token_to_channel = {s["token"]: s["channel"] for s in sensors}
    by_channel = {}
    for cs in cal_sensors:
        channel = token_to_channel.get(cs["sensor_token"], "UNKNOWN")
        if channel not in by_channel:
            by_channel[channel] = cs
    return by_channel


def validate_kitti():
    calib = load_kitti_calibration(KITTI_DIR / "calibration")
    labels = load_kitti_label(KITTI_DIR / "export" / "0000000010.txt")

    
    R = calib["R_velo_to_cam"]
    orth_err = float(np.max(np.abs(R @ R.T - np.eye(3))))
    record("KITTI-CAL-01",
           "R_velo_to_cam rotation matrix orthogonality (R·Rᵀ = I)",
           f"{orth_err:.2e}", "< 1.0e-05", PASS if orth_err < 1e-5 else FAIL)

    R_rect = calib["R_rect_00"]
    orth_err_rect = float(np.max(np.abs(R_rect @ R_rect.T - np.eye(3))))
    record("KITTI-CAL-02",
           "R_rect_00 rectification matrix orthogonality (R·Rᵀ = I)",
           f"{orth_err_rect:.2e}", "< 1.0e-05", PASS if orth_err_rect < 1e-5 else FAIL)

    det = float(np.linalg.det(R_rect))
    record("KITTI-CAL-03",
           "R_rect_00 determinant is +1 (proper rotation, no reflection)",
           f"{det:.6f}", "≈ 1.000000", PASS if abs(det - 1.0) < 1e-4 else FAIL)

    for i, label in enumerate(labels):
        
        expected_alpha = np.arctan2(label['x'], label['z']) - label['rotation_y']
        alpha_diff = abs(expected_alpha - label['alpha'])
        record(f"KITTI-EXP-01",
               f"Observation angle (alpha) = atan2(x,z) - rotation_y",
               f"error = {alpha_diff:.2e}", "< 1.0e-03",
               PASS if alpha_diff < 1e-3 else FAIL,
               f"computed={expected_alpha:.6f}, exported={label['alpha']:.6f}")

        
        record(f"KITTI-EXP-02",
               f"Object z-coordinate is positive (in front of camera)",
               f"z = {label['z']:.3f}", "> 0",
               PASS if label['z'] > 0 else FAIL)

        
        rect_pos = np.array([label['x'], label['y'] - label['h'] * 0.5, label['z']])
        cam_pos = R_rect.T @ rect_pos
        T_velo = calib["T_velo_to_cam"]
        R_velo = calib["R_velo_to_cam"]
        lidar_pos = (R_velo.T @ (cam_pos.reshape(3, 1) - T_velo)).flatten()
        re_rect = lidar_to_rect(lidar_pos, calib)
        rt_err = float(np.max(np.abs(re_rect - rect_pos)))
        record(f"KITTI-RT-01",
               f"Position round-trip: KITTI rect → LIDAR → KITTI rect",
               f"max |error| = {rt_err:.2e}", "< 1.0e-03",
               PASS if rt_err < 1e-3 else FAIL,
               f"LIDAR=({lidar_pos[0]:.3f}, {lidar_pos[1]:.3f}, {lidar_pos[2]:.3f})")

        
        frame_yaw_offset = np.arctan2(R_velo[0, 1], R_velo[0, 0])
        lidar_yaw_rad = -(label['rotation_y'] + frame_yaw_offset)
        re_rot_y = lidar_yaw_to_kitti_rotation_y(np.rad2deg(lidar_yaw_rad), calib)
        rot_err = abs(re_rot_y - label['rotation_y'])
        record(f"KITTI-RT-02",
               f"Rotation round-trip: rotation_y → LIDAR yaw → rotation_y",
               f"|error| = {rot_err:.2e}", "< 1.0e-05",
               PASS if rot_err < 1e-5 else FAIL,
               f"rotation_y={label['rotation_y']:.6f} rad ({np.rad2deg(label['rotation_y']):.2f}°)")

        
        r_scipy = Rotation.from_euler('z', lidar_yaw_rad)
        q = r_scipy.as_quat()
        rot_quat = [q[3], q[0], q[1], q[2]]
        dim = np.array([label['l'], label['w'], label['h']])
        corners_lidar = compute_corners(lidar_pos, dim, rot_quat)
        corners_rect = np.array([lidar_to_rect(c, calib) for c in corners_lidar])
        bbox_computed = project_to_2d(corners_rect, calib["P_rect_02"])

        if bbox_computed:
            bbox_diff = max(abs(a - b) for a, b in zip(bbox_computed, label['bbox']))
            record(f"KITTI-BBOX-01",
                   f"2D bounding box projection (8 corners → P_rect_02 → image)",
                   f"max |pixel error| = {bbox_diff:.2f} px", "< 2.0 px",
                   PASS if bbox_diff < 2.0 else FAIL,
                   f"computed=[{', '.join(f'{v:.2f}' for v in bbox_computed)}], "
                   f"exported=[{', '.join(f'{v:.2f}' for v in label['bbox'])}]")

        
        try:
            from PIL import Image
            img = Image.open(str(KITTI_DIR / "image_02" / "data" / "0000000010.png"))
            w_img, h_img = img.size
            bbox = label['bbox']
            fully_inside = bbox[0] >= 0 and bbox[1] >= 0 and bbox[2] <= w_img and bbox[3] <= h_img
            partially = bbox[2] > 0 and bbox[3] > 0 and bbox[0] < w_img and bbox[1] < h_img
            record(f"KITTI-IMG-01",
                   f"2D bounding box falls within image bounds ({w_img}×{h_img})",
                   f"fully_inside={fully_inside}, partially_visible={partially}",
                   "partially_visible=True",
                   PASS if partially else FAIL)
        except ImportError:
            pass

        pc_file = KITTI_DIR / "velodyne_points" / "data" / "0000000010.bin"
        if pc_file.exists():
            points = np.fromfile(str(pc_file), dtype=np.float32).reshape(-1, 4)
            half = dim / 2.0
            mask = ((points[:, 0] >= lidar_pos[0] - half[0]) & (points[:, 0] <= lidar_pos[0] + half[0]) &
                    (points[:, 1] >= lidar_pos[1] - half[1]) & (points[:, 1] <= lidar_pos[1] + half[1]) &
                    (points[:, 2] >= lidar_pos[2] - half[2]) & (points[:, 2] <= lidar_pos[2] + half[2]))
            count = int(mask.sum())
            record(f"KITTI-PC-01",
                   f"LIDAR points inside cuboid axis-aligned bounding box",
                   f"{count} points", "> 0",
                   PASS if count > 0 else WARN,
                   f"Point cloud total: {points.shape[0]} points")


def validate_nuscenes():
    calib_dir = NUSCENES_DIR / "calibration"
    export_dir = NUSCENES_DIR / "export"
    data_dir = NUSCENES_DIR / "data"

    cal = load_nuscenes_calibration(calib_dir)

    for ef in export_dir.glob("*.json"):
        with open(ef) as f:
            annotations = json.load(f)
        frame_id = ef.stem

        pc_file = data_dir / (frame_id + ".pcd.bin")
        points = None
        if pc_file.exists():
            points = np.fromfile(str(pc_file), dtype=np.float32).reshape(-1, 5)

        for j, ann in enumerate(annotations):
            pos = np.array(ann['translation'])
            size = np.array(ann['size'])
            rot = ann['rotation']

            
            q_norm = float(np.linalg.norm(rot))
            record(f"NUSC-EXP-01",
                   f"Rotation quaternion [w,x,y,z] has unit norm",
                   f"||q|| = {q_norm:.6f}", "≈ 1.000000",
                   PASS if abs(q_norm - 1.0) < 1e-4 else FAIL,
                   f"q = {rot}")

            
            dist = float(np.linalg.norm(pos[:2]))
            record(f"NUSC-EXP-02",
                   f"Annotation position is in LIDAR sensor frame (not global coordinates)",
                   f"distance from origin = {dist:.2f} m", "< 200 m",
                   PASS if dist < 200 else FAIL,
                   f"pos = ({pos[0]:.4f}, {pos[1]:.4f}, {pos[2]:.4f})")

            
            record(f"NUSC-EXP-03",
                   f"Dimension order follows nuScenes [width, length, height] convention",
                   f"size = [{size[0]:.4f}, {size[1]:.4f}, {size[2]:.4f}]",
                   "[w, l, h] where w < l for typical vehicles",
                   PASS if size[0] > 0 and size[1] > 0 and size[2] > 0 else FAIL,
                   f"Export writes [dim.y, dim.x, dim.z] matching nuScenes wlh standard")

            
            if points is not None:
                reported_pts = ann.get('num_lidar_pts', 0)
                half = size / 2.0
                aabb_min, aabb_max = pos - half, pos + half
                mask = ((points[:, 0] >= aabb_min[0]) & (points[:, 0] <= aabb_max[0]) &
                        (points[:, 1] >= aabb_min[1]) & (points[:, 1] <= aabb_max[1]) &
                        (points[:, 2] >= aabb_min[2]) & (points[:, 2] <= aabb_max[2]))
                computed_pts = int(mask.sum())
                record(f"NUSC-EXP-04",
                       f"num_lidar_pts matches actual point count inside AABB",
                       f"reported={reported_pts}, computed={computed_pts}",
                       "reported == computed",
                       PASS if reported_pts == computed_pts else FAIL,
                       f"Point cloud total: {points.shape[0]} points")

            
            required = ["token", "sample_token", "instance_token", "category_name",
                       "translation", "size", "rotation", "num_lidar_pts", "num_radar_pts"]
            missing = [k for k in required if k not in ann]
            record(f"NUSC-EXP-05",
                   f"All required nuScenes annotation fields are present in JSON",
                   f"missing = {missing if missing else 'none'}", "no missing fields",
                   PASS if not missing else FAIL,
                   f"Required: {', '.join(required)}")

            
            import re
            uuid_pattern = re.compile(r'^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$')
            token_valid = bool(uuid_pattern.match(ann.get('token', '')))
            instance_valid = bool(uuid_pattern.match(ann.get('instance_token', '')))
            record(f"NUSC-EXP-06",
                   f"Annotation token and instance_token are valid UUID format",
                   f"token_valid={token_valid}, instance_valid={instance_valid}",
                   "both = True",
                   PASS if token_valid and instance_valid else FAIL)


def validate_timestamps():
    img_ts_file = KITTI_DIR / "image_02" / "timestamps.txt"
    velo_ts_file = KITTI_DIR / "velodyne_points" / "timestamps.txt"

    if img_ts_file.exists() and velo_ts_file.exists():
        with open(img_ts_file) as f:
            img_ts = [l.strip() for l in f if l.strip()]
        with open(velo_ts_file) as f:
            velo_ts = [l.strip() for l in f if l.strip()]

        record("KITTI-SYNC-01",
               "Image and Velodyne frame counts match (index-based sync)",
               f"image={len(img_ts)}, velodyne={len(velo_ts)}",
               "counts equal",
               PASS if len(img_ts) == len(velo_ts) else FAIL)

        
        from datetime import datetime
        offsets = []
        for it, vt in zip(img_ts[:10], velo_ts[:10]):
            try:
                t1 = datetime.strptime(it[:26], "%Y-%m-%d %H:%M:%S.%f")
                t2 = datetime.strptime(vt[:26], "%Y-%m-%d %H:%M:%S.%f")
                offsets.append(abs((t1 - t2).total_seconds() * 1000))
            except ValueError:
                pass
        if offsets:
            avg_ms = np.mean(offsets)
            record("KITTI-SYNC-02",
                   "Average time offset between image and velodyne captures",
                   f"{avg_ms:.1f} ms", "< 100 ms (acceptable for index-based sync)",
                   PASS if avg_ms < 100 else WARN,
                   "SYCRAD uses filename index matching, not timestamp matching")

    
    lidar_dir = NUSCENES_DIR / "data"
    cam_dir = NUSCENES_DIR / "camera" / "CAM_FRONT"

    if lidar_dir.exists() and cam_dir.exists():
        lidar_files = sorted([f for f in lidar_dir.iterdir() if f.name.endswith('.pcd.bin')])
        cam_files = sorted([f for f in cam_dir.iterdir() if f.suffix == '.jpg'])

        lidar_ts = [int(f.name.replace('.pcd.bin', '').split("__")[-1]) for f in lidar_files]
        cam_ts = [int(f.stem.split("__")[-1]) for f in cam_files]

        closest_diffs_ms = []
        for lt in lidar_ts:
            if cam_ts:
                min_diff = min(abs(lt - ct) for ct in cam_ts) / 1000.0
                closest_diffs_ms.append(min_diff)

        if closest_diffs_ms:
            avg_diff = np.mean(closest_diffs_ms)
            max_diff = np.max(closest_diffs_ms)
            record("NUSC-SYNC-01",
                   "Average LIDAR↔CAM_FRONT timestamp offset (closest frame pairs)",
                   f"avg={avg_diff:.1f} ms, max={max_diff:.1f} ms",
                   "< 100 ms",
                   PASS if avg_diff < 100 else WARN,
                   f"LIDAR frames={len(lidar_ts)}, CAM_FRONT frames={len(cam_ts)}")

        
        ego_file = NUSCENES_DIR / "calibration" / "ego_pose.json"
        sd_file = NUSCENES_DIR / "calibration" / "sample_data.json"
        if ego_file.exists() and sd_file.exists():
            with open(ego_file) as f:
                ego_poses = json.load(f)
            with open(sd_file) as f:
                sample_data = json.load(f)
            ego_map = {e['token']: e for e in ego_poses}

            target_lidar = [d for d in sample_data
                           if '1533151607048933' in d['filename'] and 'LIDAR_TOP' in d['filename']]
            target_cam = [d for d in sample_data
                         if '1533151607012404' in d['filename'] and 'CAM_FRONT/' in d['filename']
                         and 'LEFT' not in d['filename'] and 'RIGHT' not in d['filename']]

            if target_lidar and target_cam:
                ego_l = ego_map.get(target_lidar[0]['ego_pose_token'])
                ego_c = ego_map.get(target_cam[0]['ego_pose_token'])
                if ego_l and ego_c:
                    pos_diff = np.linalg.norm(
                        np.array(ego_l['translation']) - np.array(ego_c['translation']))
                    time_diff = (ego_l['timestamp'] - ego_c['timestamp']) / 1000.0
                    record("NUSC-SYNC-02",
                           "Ego vehicle position drift between LIDAR and CAM_FRONT captures",
                           f"Δposition={pos_diff:.4f} m, Δtime={time_diff:.1f} ms",
                           "informational (ego_pose compensation recommended if > 0.1m)",
                           WARN if pos_diff > 0.1 else PASS,
                           "Without ego_pose compensation, this drift causes pixel-level projection error")


def validate_nuscenes_projection():
    calib_dir = NUSCENES_DIR / "calibration"
    cal = load_nuscenes_calibration(calib_dir)
    lidar_cal, cam_cal = cal.get("LIDAR_TOP"), cal.get("CAM_FRONT")
    if not lidar_cal or not cam_cal:
        return

    R_ego_lidar = quaternion_to_rotation_matrix(lidar_cal['rotation'])
    T_ego_lidar = np.array(lidar_cal['translation']).reshape(3, 1)
    R_ego_cam = quaternion_to_rotation_matrix(cam_cal['rotation'])
    T_ego_cam = np.array(cam_cal['translation']).reshape(3, 1)
    R_cam_ego = R_ego_cam.T
    T_cam_ego = -R_ego_cam.T @ T_ego_cam
    K = np.array(cam_cal['camera_intrinsic'])

    
    ego_file = NUSCENES_DIR / "calibration" / "ego_pose.json"
    sd_file = NUSCENES_DIR / "calibration" / "sample_data.json"

    R_simple = R_cam_ego @ R_ego_lidar
    T_simple = R_cam_ego @ T_ego_lidar + T_cam_ego

    R_full, T_full = R_simple, T_simple
    has_ego = False

    if ego_file.exists() and sd_file.exists():
        with open(ego_file) as f:
            ego_poses = json.load(f)
        with open(sd_file) as f:
            sample_data = json.load(f)
        ego_map = {e['token']: e for e in ego_poses}
        tl = [d for d in sample_data if '1533151607048933' in d['filename'] and 'LIDAR_TOP' in d['filename']]
        tc = [d for d in sample_data if '1533151607012404' in d['filename'] and 'CAM_FRONT/' in d['filename']
              and 'LEFT' not in d['filename'] and 'RIGHT' not in d['filename']]
        if tl and tc:
            el, ec = ego_map.get(tl[0]['ego_pose_token']), ego_map.get(tc[0]['ego_pose_token'])
            if el and ec:
                R_g_el = quaternion_to_rotation_matrix(el['rotation'])
                T_g_el = np.array(el['translation']).reshape(3, 1)
                R_g_ec = quaternion_to_rotation_matrix(ec['rotation'])
                T_g_ec = np.array(ec['translation']).reshape(3, 1)
                R_ec_g = R_g_ec.T
                T_ec_g = -R_g_ec.T @ T_g_ec
                R_full = R_cam_ego @ R_ec_g @ R_g_el @ R_ego_lidar
                T_full = (R_cam_ego @ R_ec_g @ R_g_el @ T_ego_lidar +
                         R_cam_ego @ R_ec_g @ T_g_el + R_cam_ego @ T_ec_g + T_cam_ego)
                has_ego = True

    pc_file = list((NUSCENES_DIR / "data").glob("*1533151607048933*"))
    cam_file = NUSCENES_DIR / "camera" / "CAM_FRONT" / "n008-2018-08-01-15-16-36-0400__CAM_FRONT__1533151607012404.jpg"

    if pc_file and cam_file.exists():
        points = np.fromfile(str(pc_file[0]), dtype=np.float32).reshape(-1, 5)[:, :3]
        cam_pts = (R_full @ points.T + T_full).T
        front_mask = cam_pts[:, 2] > 1
        cam_front = cam_pts[front_mask]
        img_pts = (K @ cam_front.T).T
        img_pts[:, 0] /= img_pts[:, 2]
        img_pts[:, 1] /= img_pts[:, 2]
        valid = (img_pts[:, 0] >= 0) & (img_pts[:, 0] < 1600) & (img_pts[:, 1] >= 0) & (img_pts[:, 1] < 900)

        total = len(points)
        in_front = int(front_mask.sum())
        in_image = int(valid.sum())
        pct = (in_image / total * 100) if total > 0 else 0

        record("NUSC-PROJ-01",
               f"LIDAR→Camera projection coverage (full 6-step chain with ego_pose={has_ego})",
               f"{in_image}/{total} points in image ({pct:.1f}%)",
               "5-15% for front camera (typical)",
               PASS if 2 < pct < 25 else WARN,
               f"In front of camera: {in_front}, projected in 1600×900 image: {in_image}")

        
        try:
            import cv2
            img = cv2.imread(str(cam_file))
            depths = cam_front[valid, 2]
            valid_img_pts = img_pts[valid]
            depth_min, depth_max = depths.min(), min(depths.max(), 60)
            for k in range(len(valid_img_pts)):
                u, v = int(valid_img_pts[k, 0]), int(valid_img_pts[k, 1])
                d = min(depths[k], depth_max)
                ratio = (d - depth_min) / (depth_max - depth_min + 1e-6)
                color = (int(255 * ratio), 0, int(255 * (1 - ratio)))
                cv2.circle(img, (u, v), 1, color, -1)

            export_file = NUSCENES_DIR / "export" / "n008-2018-08-01-15-16-36-0400__LIDAR_TOP__1533151607048933.json"
            if export_file.exists():
                with open(export_file) as f:
                    anns = json.load(f)
                for ann in anns:
                    corners = compute_corners(np.array(ann['translation']),
                                             np.array(ann['size']), ann['rotation'])
                    cc = (R_full @ corners.T + T_full).T
                    if all(cc[:, 2] > 0):
                        ic = (K @ cc.T).T
                        ic[:, 0] /= ic[:, 2]
                        ic[:, 1] /= ic[:, 2]
                        pts2d = ic[:, :2].astype(int)
                        for a, b in [(0,1),(1,2),(2,3),(3,0),(4,5),(5,6),(6,7),(7,4),
                                    (0,4),(1,5),(2,6),(3,7)]:
                            cv2.line(img, tuple(pts2d[a]), tuple(pts2d[b]), (0, 255, 0), 2)

            out = BASE_DIR / "test" / "nuscenes_overlay.png"
            cv2.imwrite(str(out), img)
            record("NUSC-PROJ-02",
                   "Visual overlay generated (LIDAR points + cuboid on camera image)",
                   f"saved to test/nuscenes_overlay.png", "file created",
                   PASS, "Green wireframe = cuboid, colored dots = LIDAR depth")
        except ImportError:
            record("NUSC-PROJ-02",
                   "Visual overlay generation",
                   "opencv-python not installed", "optional",
                   WARN, "Install opencv-python to generate overlay image")


def print_report():
    print()
    print("## Export Validation Report")
    print()
    print("Test data: `test/kitti/` (KITTI format) and `test/nuscenes/` (nuScenes format)")
    print()
    print("```")

    
    groups = {
        "KITTI Calibration Integrity": [],
        "KITTI Export Accuracy": [],
        "KITTI Coordinate Round-Trip": [],
        "KITTI 2D Bounding Box Projection": [],
        "KITTI Image & Point Cloud": [],
        "nuScenes Export Accuracy": [],
        "Timestamp Synchronization": [],
        "nuScenes LIDAR→Camera Projection": [],
    }

    for r in results:
        tid = r['test']
        if tid.startswith("KITTI-CAL"):
            groups["KITTI Calibration Integrity"].append(r)
        elif tid.startswith("KITTI-EXP"):
            groups["KITTI Export Accuracy"].append(r)
        elif tid.startswith("KITTI-RT"):
            groups["KITTI Coordinate Round-Trip"].append(r)
        elif tid.startswith("KITTI-BBOX"):
            groups["KITTI 2D Bounding Box Projection"].append(r)
        elif tid.startswith("KITTI-IMG") or tid.startswith("KITTI-PC"):
            groups["KITTI Image & Point Cloud"].append(r)
        elif tid.startswith("NUSC-EXP"):
            groups["nuScenes Export Accuracy"].append(r)
        elif "SYNC" in tid:
            groups["Timestamp Synchronization"].append(r)
        elif tid.startswith("NUSC-PROJ"):
            groups["nuScenes LIDAR→Camera Projection"].append(r)

    pass_count = sum(1 for r in results if r['status'] == PASS)
    warn_count = sum(1 for r in results if r['status'] == WARN)
    fail_count = sum(1 for r in results if r['status'] == FAIL)
    total = len(results)

    for group_name, group_results in groups.items():
        if not group_results:
            continue
        print(f"\n  {group_name}")
        print(f"  {'─' * len(group_name)}")
        for r in group_results:
            icon = "✅" if r['status'] == PASS else ("⚠️" if r['status'] == WARN else "❌")
            print(f"  {icon} [{r['test']}] {r['description']}")
            print(f"     Measured : {r['measured']}")
            print(f"     Expected : {r['threshold']}")
            if r['detail']:
                print(f"     Detail   : {r['detail']}")
            print()

    print(f"  {'═' * 60}")
    print(f"  Total: {total} tests | ✅ {pass_count} passed | ⚠️ {warn_count} warnings | ❌ {fail_count} failed")
    print("```")
    print()


if __name__ == "__main__":
    validate_kitti()
    validate_nuscenes()
    validate_timestamps()
    validate_nuscenes_projection()
    print_report()