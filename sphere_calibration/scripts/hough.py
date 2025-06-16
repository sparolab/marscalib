#!/usr/bin/env python3


import cv2
import numpy as np
import matplotlib.pyplot as plt
import open3d as o3d
import os 
import argparse



def process_point_cloud(data_path, k, points):
    # Filter based on x > 0.8 and Euclidean distance <= 5
    distances = np.linalg.norm(points, axis=1)
    mask = (points[:, 0] > 0.8) & (distances <= 5)
    filtered_points = points[mask]
    flag = True
    
    
    filtered_pcd = o3d.geometry.PointCloud()
    filtered_pcd.points = o3d.utility.Vector3dVector(filtered_points)

    # Voxelization
    voxel_size = 0.03
    voxel_pcd = filtered_pcd.voxel_down_sample(voxel_size=voxel_size)

    # Statistical Outlier Removal
    cl, ind = voxel_pcd.remove_statistical_outlier(nb_neighbors=50, std_ratio=0.05)
    filtered_pcd = voxel_pcd.select_by_index(ind)

    # Plane segmentation (RANSAC)
    plane_model, inliers = filtered_pcd.segment_plane(distance_threshold=0.1, ransac_n=3, num_iterations=1000)
    non_ground_pcd = filtered_pcd.select_by_index(inliers, invert=True)

    # print(f"   - Ground points             : {len(ground_pcd.points)}")
    # print(f"   - Non-ground(Object) points : {len(non_ground_pcd.points)}")
    print("   - Ground points excluded.")
    
    # Euclidean Cluster Extraction
    labels = np.array(non_ground_pcd.cluster_dbscan(eps=0.2, min_points=200, print_progress=False))
    max_label = labels.max()
    if max_label == -1:
        flag = False
        print(f"   - No clusters detected for {k}-th scene. Maybe the target is located too far. Skipping...\n")
    
    else :
        print(f"   - Detected {max_label + 1} object from the pointcloud.")
        print(f"   - Green :  ground,  Red :  object\n   - Press 'q' to continue.")

        final_cloud = o3d.geometry.PointCloud()
        for i in range(max_label + 1):
            cluster_pcd = non_ground_pcd.select_by_index(np.where(labels == i)[0])
            final_cloud += cluster_pcd

        o3d.io.write_point_cloud(os.path.join(data_path, str(k), "object.pcd"), final_cloud)

        filtered_pcd.paint_uniform_color([0, 1, 0]) 
        final_cloud.paint_uniform_color([1, 0, 0])
        o3d.visualization.draw_geometries([filtered_pcd, final_cloud])
        
    return flag, filtered_points
        
    
        
def make_range_image(data_path, k, raw_points, fov):
    final_cloud = o3d.io.read_point_cloud(os.path.join(data_path, str(k), "object.pcd"))
    final_points = np.asarray(final_cloud.points)

    h_fov = 360.0 * np.pi / 180.0
    v_fov_up = fov[0] * np.pi / 180.0
    v_fov_down = fov[1] * np.pi / 180.0

    w = 720
    h = int(w * (v_fov_up - v_fov_down) / h_fov)

    # Create binary image matrix
    binary_image = np.zeros((h, w), dtype=np.uint8)
    index_3d = [[[] for _ in range(w)] for _ in range(h)]

    # Process final cloud points
    for idx, point in enumerate(final_points):
        x, y, z = point
        phi = np.arctan2(y, x)
        distance = np.linalg.norm([x, y, z])
        if distance == 0:
            continue  # Skip zero distance points
        theta = np.arcsin(z / distance)

        u = int((phi + np.pi) / (2 * np.pi) * w)
        v = int((theta - v_fov_down) / (v_fov_up - v_fov_down) * h)

        if 0 <= u < w and 0 <= v < h:
            for du in range(-1, 2):
                for dv in range(-1, 2):
                    uu = u + du
                    vv = v + dv
                    if 0 <= uu < w and 0 <= vv < h:
                        binary_image[vv, uu] = 255
    
    for idx, point in enumerate(raw_points):
        x, y, z = point
        phi = np.arctan2(y, x)
        distance = np.linalg.norm([x, y, z])
        if distance == 0:
            continue  # Skip zero distance points
        theta = np.arcsin(z / distance)

        u = int((phi + np.pi) / (2 * np.pi) * w)
        v = int((theta - v_fov_down) / (v_fov_up - v_fov_down) * h)

        if 0 <= u < w and 0 <= v < h:
            # Update a 1-pixel radius neighborhood
            for du in range(-1, 2):
                for dv in range(-1, 2):
                    uu = u + du
                    vv = v + dv
                    if 0 <= uu < w and 0 <= vv < h:
                        index_3d[vv][uu].append(idx)
                        # print(idx)
                        
    for i in range(h):
        for j in range(w):
            index_3d[i][j] = list(set(index_3d[i][j])) 

    
    return binary_image, index_3d
    
    

def black_region_ratio(circle_center, radius, gray_image, threshold):
    height, width = gray_image.shape
    y, x = np.ogrid[:height, :width]
    
    distance_from_center = np.sqrt((x - circle_center[0]) ** 2 + (y - circle_center[1]) ** 2)
    
    mask = distance_from_center <= radius
    
    total_pixels_inside_circle = np.sum(mask)  
    num_black_pixels = np.sum(gray_image[mask] < threshold)  
    # print(f"Black pixel ratio :  {num_black_pixels / total_pixels_inside_circle}")
    # print(f"Radius            :  {radius}")
    if num_black_pixels / total_pixels_inside_circle < threshold :
        return True
    return False


def outer_region(circle_center, radius, gray_image, threshold):
    height, width = gray_image.shape
    y, x = np.ogrid[:height, :width]
    outer_circle_mask = (np.sqrt((x - circle_center[0]) ** 2 + (y - circle_center[1]) ** 2) <= radius + 5)
    inner_circle_mask = (np.sqrt((x - circle_center[0]) ** 2 + (y - circle_center[1]) ** 2) <= radius)
    region_a_mask = outer_circle_mask & ~inner_circle_mask
    total_pixels_inside_circle = np.sum(region_a_mask)
    num_black_pixels_region_a = np.sum(gray_image[region_a_mask] < threshold) 
    # print(f"Surronding black pixel ratio : {num_black_pixels_region_a / total_pixels_inside_circle}")

    if num_black_pixels_region_a / total_pixels_inside_circle > threshold:
        return True
    return False


def process_circles(circle, index_3d, input_points):
    center_x, center_y, radius = int(circle[0][0]), int(circle[0][1]), int(circle[0][2])
    sphere_cloud = o3d.geometry.PointCloud()
    points = []
    pindex = []
    
    height, width = len(index_3d), len(index_3d[0])
    for u in range(max(0, center_x - radius), min(width, center_x + radius)):
        for v in range(max(0, center_y - radius), min(height, center_y + radius)):
            point_indices = index_3d[v][u]
            # print(point_indices)
            if point_indices:
                # Append all points corresponding to the indices at this pixel
                for point_index in point_indices:
                    pindex.append(point_index)

    pindex = list(set(pindex))
    
    for index in pindex:
        points.append(input_points[index])



    # Create and return the PointCloud with extracted points
    sphere_cloud.points = o3d.utility.Vector3dVector(np.array(points))
    print("   - Number of points that consists sphere:", len(sphere_cloud.points), "\n\n")

    # Visualization for verification (optional)
    o3d.visualization.draw_geometries([sphere_cloud])
    
    return sphere_cloud





def main():
    
    parser = argparse.ArgumentParser(description='Range image generation & Hough circle detection', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('data_path', help='Input data path')
    parser.add_argument('LiDAR_type', help='LiDAR type : o, m ,s')
    args = parser.parse_args()
    data_path = args.data_path 
    lidar_type = args.LiDAR_type
    
    folder_count = len([folder for folder in os.listdir(data_path) 
                if os.path.isdir(os.path.join(data_path, folder))])
    
    fov = []  # vertical fov
    if lidar_type == 'o':
        fov = [22.5, -22.5]
    elif lidar_type == 'm':
        fov = [52, -7]
    elif lidar_type == 's':
        fov = [17.5, -17.5]
    else :
        print("Wrong LiDAR type")

        
    print(f"There are {folder_count} scenes.\n\n") 
    for k in range(1, folder_count + 1):

        print(f"{k}-th scene")

        
        pcd = o3d.io.read_point_cloud(os.path.join(data_path, str(k), "pointcloud.pcd"))
        points = np.asarray(pcd.points)

        flag, filtered_points = process_point_cloud(data_path, k, points)
        if flag is False:
            continue
        range_image, index = make_range_image(data_path, k, filtered_points, fov)


        inlier_circle = []

        print("   - Range image is generated. Finding circles using Hough transform")


        gray_blurred = cv2.GaussianBlur(range_image, (9, 9), 2)
        circles = cv2.HoughCircles(
            gray_blurred, 
            cv2.HOUGH_GRADIENT, 
            dp=1,              # Inverse ratio of resolution
            minDist=30,        # Minimum distance between detected centers
            param1=100,        # Upper threshold for Canny edge detector
            param2=10,          # Threshold for center detection
            minRadius=4,       # Minimum circle radius
            maxRadius=10       # Maximum circle radius
        )
        
        range_image_rgb = cv2.cvtColor(range_image, cv2.COLOR_GRAY2BGR)
        
        if circles is not None:
            circles = np.uint16(np.around(circles))  
            for i in circles[0, :]:
                center = (i[0], i[1])
                radius = i[2]
                
                if black_region_ratio(center, radius, range_image, 0.15) and outer_region(center, radius, range_image, 0.6):
                    inlier_circle.append(i)
                    cv2.circle(range_image_rgb, center, radius, (0, 255, 0), 2)
                else: 
                    cv2.circle(range_image_rgb, center, radius, (255, 0, 0), 2)
        
                
        print(f"   - {len(inlier_circle)} circle is detected from hough transform.\n\n") 
        cv2.imshow("image", range_image_rgb)
        cv2.waitKey(0)
        cv2.destroyAllWindows()
        cv2.imwrite(os.path.join(data_path, str(k), "circle_detection.png"), range_image_rgb)
        if len(inlier_circle) == 1:
            sphere_cloud = process_circles(inlier_circle, index, filtered_points)
            o3d.io.write_point_cloud(os.path.join(data_path, str(k), "sphere.pcd"), sphere_cloud)
        

if __name__ == '__main__':
    
    main()
    
