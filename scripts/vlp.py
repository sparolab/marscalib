import os
import json
import random
import numpy as np
import open3d as o3d

def load_pcd_files_from_folder(folder_path):
    pcd_files = []
    for file in os.listdir(folder_path):
        if file.endswith('.pcd'):
            pcd_files.append(os.path.join(folder_path, file))
    return pcd_files

def fit_sphere_to_points(p1, p2, p3, p4):
    """4개 점으로 구 방정식 추정"""
    A = np.array([
        [p1[0], p1[1], p1[2], 1],
        [p2[0], p2[1], p2[2], 1],
        [p3[0], p3[1], p3[2], 1],
        [p4[0], p4[1], p4[2], 1]
    ])

    B = np.array([
        -(p1[0]**2 + p1[1]**2 + p1[2]**2),
        -(p2[0]**2 + p2[1]**2 + p2[2]**2),
        -(p3[0]**2 + p3[1]**2 + p3[2]**2),
        -(p4[0]**2 + p4[1]**2 + p4[2]**2)
    ])

    try:
        X = np.linalg.solve(A, B)
        center = -0.5 * X[:3]
        radius = np.sqrt(np.sum(center**2) - X[3])
        return center, radius
    except np.linalg.LinAlgError:
        return None, None

def ransac_sphere(pcd_points, threshold=0.01, max_iterations=1000):
    best_inliers = []
    best_center = None

    points = np.asarray(pcd_points)

    for _ in range(max_iterations):
        idx = np.random.choice(len(points), 4, replace=False)
        sample_points = points[idx]

        center, radius = fit_sphere_to_points(*sample_points)
        if center is None:
            continue

        # 거리 계산
        dists = np.linalg.norm(points - center, axis=1)
        inliers = np.where(np.abs(dists - radius) < threshold)[0]

        if len(inliers) > len(best_inliers):
            best_inliers = inliers
            best_center = center

    return best_center

def save_center_as_pcd(center, output_pcd_path):
    """구 중심을 point cloud로 저장"""
    center_point = o3d.geometry.PointCloud()
    center_np = np.array(center).reshape(1, 3)
    center_point.points = o3d.utility.Vector3dVector(center_np)
    o3d.io.write_point_cloud(output_pcd_path, center_point)

def main(folder_path, output_folder_path):
    os.makedirs(output_folder_path, exist_ok=True)
    pcd_files = load_pcd_files_from_folder(folder_path)

    for pcd_file in pcd_files:
        print(f"Processing: {pcd_file}")
        pcd = o3d.io.read_point_cloud(pcd_file)

        center = ransac_sphere(pcd.points)
        
        filename_without_ext = os.path.splitext(os.path.basename(pcd_file))[0]
        output_json_path = os.path.join(output_folder_path, filename_without_ext + ".json")
        output_center_pcd_path = os.path.join(output_folder_path, filename_without_ext + "_center.pcd")

        if center is not None:
            center_data = {
                'LiDAR': [float(center[0]), float(center[1]), float(center[2])]
            }
            # Save JSON
            with open(output_json_path, 'w') as f:
                json.dump(center_data, f, indent=4)
            print(f"Center saved to {output_json_path}")

            # Save PCD
            save_center_as_pcd(center, output_center_pcd_path)
            print(f"Center PCD saved to {output_center_pcd_path}")
        else:
            print(f"Sphere not found in {pcd_file}")

if __name__ == "__main__":
    folder_path = "/home/jsh/JSH_ws/gamma_sphere_manual/"         # PCD 파일들이 있는 폴더
    output_folder_path = "/home/jsh/JSH_ws/gamma_sphere_manual/"  # JSON 및 중심 PCD 저장 폴더
    main(folder_path, output_folder_path)
