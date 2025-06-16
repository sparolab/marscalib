#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <array>
#include <vector>
#include <random>
#include <cmath>

#include <pcl/filters/voxel_grid.h>
#include <pcl/point_types.h>
#include <pcl/features/normal_3d.h>
#include <pcl/point_cloud.h>
#include <pcl/io/pcd_io.h>
#include <pcl/common/common.h>
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/common/eigen.h>
#include <pcl/kdtree/kdtree_flann.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/segmentation/region_growing.h>
#include <pcl/segmentation/progressive_morphological_filter.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/segmentation/segment_differences.h>
#include <pcl/filters/statistical_outlier_removal.h>
#include <pcl/sample_consensus/ransac.h>
#include <pcl/sample_consensus/sac_model_plane.h>
#include <opencv2/opencv.hpp>
#include <pcl/surface/mls.h>

#include <nlohmann/json.hpp>

#include <ceres/ceres.h>
#include <ceres/problem.h>
#include <ceres/rotation.h>



int main()
{
//    std::string lidar = "os_c2";
    std::string sphere = "raw";
    Eigen::Vector4f intrinsic = Eigen::Vector4f::Ones();
    Eigen::VectorXf distortion(5);

    intrinsic <<             889.896415920388,
            890.030007109443,
            652.093117940091,
            362.081468286178;

    distortion <<            0.156039812922347,
            -0.477098431429220,
            -0.000593508151070103,
            0.000356355693691240,
            0.438968466801125;


    Eigen::Matrix4f T_camera_lidar;

//    std::ifstream ifs( "/home/jsh/JSH_ws/sphere_calib/08_31/" + lidar + "/json/" + sphere + "_calib.json");
//
//    nlohmann::json config;
//    ifs >> config;
//    const std::vector<double> values = config["results"]["T_lidar_camera"];
//    std::vector<double> init_values;
//    init_values.assign(values.begin(), values.end());
//    T_camera_lidar.block<3,1>(0,3) << static_cast<float>(init_values[0]),
//            static_cast<float>(init_values[1]),
//            static_cast<float>(init_values[2]);
//    Eigen::Matrix3f rotation_matrix = Eigen::Quaterniond(init_values[6], init_values[3],init_values[4], init_values[5]).normalized().toRotationMatrix().cast<float>();
//    T_camera_lidar.block<3,3>(0,0) = rotation_matrix;
//    T_camera_lidar(3,3) = 1;
//    T_camera_lidar.block<1,3>(3,0) << 0,0,0;
//    T_camera_lidar <<          0,   -1,   0,    0,
//            0,    0,   -1,   0,
//            1,    0,    00,  0,
//            0  ,       0   ,      0 ,   1.0000;

        //os c2
    T_camera_lidar <<
        0.06082518, -0.99489474,  0.08052637, -0.04328656,
       -0.15462523, -0.0890937 , -0.98394772,  0.14383912,
        0.98609913,  0.04739743, -0.15925475, -0.20404337,
        0,           0,           0,           1;

//        //mid c2 mine
//    T_camera_lidar <<        0.017815,   -0.999745,   -0.013824,    0.026745,
//    -0.0004321 ,   0.013749,   -0.9999,   -0.127939,
//    0.9998,    0.017878,    0.0001,   -0.217768,
//    0     ,    0    ,     0 ,   1.0000;

//        //mid c2 matlab
//    T_camera_lidar <<        0.0211,   -0.9997,   -0.0130,    0.0191,
//    -0.0002 ,   0.0130,   -0.9999,   -0.1406,
//    0.9998,    0.0211,    0.0001,   -0.1819,
//    0     ,    0    ,     0 ,   1.0000;

        //mid c1
//    T_camera_lidar <<          0.3575,   -0.9339,   -0.0101 ,   0.1014,
//                       -0.0761,   -0.0183,   -0.9969  , -0.0801,
//                        0.9308,    0.3572,   -0.0776,   -0.2351,
//                        0      ,   0  ,       0  ,  1.0000;


// sos2
//    T_camera_lidar <<        0.0181,   -0.9997,   -0.0136 ,  -0.0194,
//    0.0000,    0.0136,   -0.9999 ,  -0.0895,
//    0.9998,    0.0181,    0.0003 ,  -0.1516,
//         0       ,  0   ,      0   , 1.0000;
    //sos1
//    T_camera_lidar <<        0.359685,   -0.933073,   -0.000182 ,  0.074763,
//    -0.060393,    -0.023086,   -0.997907 ,  -0.074988,
//    0.931117,    0.358944,    -0.064655 ,  -0.225533,
//         0       ,  0   ,      0   , 1.0000;
////    std::cout<<T_camera_lidar<<std::endl;


    std::string target = "1";

    // std::string camera_path = "/home/jsh/data/yuncheon/rover_preprocess/" + target + "/image.png";
    // std::string lidar_path = "/home/jsh/data/yuncheon/rover_preprocess/" + target + "/pointcloud.pcd";
    std::string camera_path = "/home/jsh/JSH_ws/gamma_sphere_preprocess/5/image.png";
    std::string lidar_path = "/home/jsh/JSH_ws/gamma_sphere_manual/tmp/5.pcd";

    std::vector<cv::String> filenames_c;
    cv::glob(camera_path, filenames_c);
    std::vector<cv::String> filenames_l;
    cv::glob(lidar_path, filenames_l);
    std::sort(filenames_c.begin(), filenames_c.end());
    std::sort(filenames_l.begin(), filenames_l.end());
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr output_cloud(new pcl::PointCloud<pcl::PointXYZRGB>());

    for (int i = 0; i < 1; ++i) {
        std::cout<<i<<"-th iteration"<<std::endl;
        pcl::PointCloud<pcl::PointXYZRGB>::Ptr raw_cloud(new pcl::PointCloud<pcl::PointXYZRGB>());
        pcl::PointCloud<pcl::PointXYZRGB>::Ptr front_cloud(new pcl::PointCloud<pcl::PointXYZRGB>());
        pcl::PointCloud<pcl::PointXYZRGB>::Ptr color_cloud(new pcl::PointCloud<pcl::PointXYZRGB>());

        pcl::io::loadPCDFile<pcl::PointXYZRGB>(lidar_path, *raw_cloud);

//        pcl::io::loadPCDFile<pcl::PointXYZRGB>(filenames_l[i], *raw_cloud);
        cv::Mat RGB_image = cv::imread(camera_path, cv::IMREAD_UNCHANGED);

//        std::cout<<filenames_l[i]<<std::endl;
//        std::cout<<filenames_c[i]<<std::endl;


        int width = RGB_image.cols;
        int height = RGB_image.rows;
        for (auto &ele: *raw_cloud) {
            if (ele.x > 0.1 && ele.z > -1) {
                ele.r = 255;
                ele.g = 255;
                ele.b = 255;
                front_cloud->emplace_back(ele);
            }
        }


//        pcl::visualization::PCLVisualizer viewer2("RGB cloud");
//        viewer2.addPointCloud<pcl::PointXYZRGB>(front_cloud);
//        while(!viewer2.wasStopped()) {
//            viewer2.spinOnce();
//        }



        cv::Mat addedImage;
        Eigen::MatrixXf pointCloud(front_cloud->size(), 4);
        for (size_t j = 0; j < front_cloud->size(); j++) {
            pointCloud(j, 0) = front_cloud->points[j].x;
            pointCloud(j, 1) = front_cloud->points[j].y;
            pointCloud(j, 2) = front_cloud->points[j].z;
            pointCloud(j, 3) = 1.0;
        }

        Eigen::MatrixXf imagePoints(2, pointCloud.rows());
        imagePoints.setZero();

        for (int j = 0; j < pointCloud.rows(); j++) {
            Eigen::Vector4f transformedPoint = T_camera_lidar * pointCloud.row(j).transpose();


            const auto &x = transformedPoint(0) / transformedPoint(2);
            const auto &y = transformedPoint(1) / transformedPoint(2);

            const auto &k1 = distortion[0];
            const auto &k2 = distortion[1];
            const auto &k3 = distortion[4];
            const auto &p1 = distortion[2];
            const auto &p2 = distortion[3];

            const auto &fx = intrinsic[0];
            const auto &fy = intrinsic[1];
            const auto &cx = intrinsic[2];
            const auto &cy = intrinsic[3];

            const auto r2 = x * x + y * y;
            const auto r4 = r2 * r2;
            const auto r6 = r2 * r4;

            const auto r_coeff = 1.0 + k1 * r2 + k2 * r4 + k3 * r6;
            const auto t_coeff1 = 2.0 * x * y;
            const auto t_coeff2 = r2 + 2.0 * x * x;
            const auto t_coeff3 = r2 + 2.0 * y * y;

            const auto x_dist = r_coeff * x + p1 * t_coeff1 + p2 * t_coeff2;
            const auto y_dist = r_coeff * y + p1 * t_coeff3 + p2 * t_coeff1;

            imagePoints(0, j) = fx * x_dist + cx;
            imagePoints(1, j) = fy * y_dist + cy;
        }


        float min_z = std::numeric_limits<float>::max();
        float max_z = std::numeric_limits<float>::lowest();
        for (const auto &point : front_cloud->points) {
            if (point.z < min_z) min_z = point.z;
            if (point.z > max_z) max_z = point.z;
        }

// Normalize heights between 0 and 1
        auto normalize = [min_z, max_z](float z) {
            return (z - min_z) / (max_z - min_z);
        };

// Step 2: Project points and colorize based on height
        cv::Mat proj_image(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
        for (int j = 0; j < imagePoints.cols(); j++) {
            int u = imagePoints(0, j);
            int v = imagePoints(1, j);
            if (u >= 0 && u < width && v >= 0 && v < height) {
                // Normalize the height
//                float normalized_height = normalize(front_cloud->points[j].z);
//
//
//                if (normalized_height < 0.5) {
//                    // From blue to green (0 to 0.5)
//                    r = 0;
//                    g = static_cast<int>(510 * normalized_height); // Scale to 0-255
//                    b = static_cast<int>(255 * (1 - 2 * normalized_height)); // Decrease from 255 to 0
//                } else {
//                    // From green to red (0.5 to 1)
//                    r = static_cast<int>(510 * (normalized_height - 0.5)); // Scale to 0-255
//                    g = static_cast<int>(255 * (1 - 2 * (normalized_height - 0.5))); // Decrease from 255 to 0
//                    b = 0;
//                }
                cv::Vec3b color = RGB_image.at<cv::Vec3b>(v, u);  // Note: (v, u) as (row, col) in OpenCV

                int r = color[2];  // OpenCV stores colors as BGR, so index 2 is red
                int g = color[1];  // Green channel
                int b = color[0];  // Blue channel

                pcl::PointXYZRGB RGB_point;
                RGB_point.x = front_cloud->points[j].x;
                RGB_point.y = front_cloud->points[j].y;
                RGB_point.z = front_cloud->points[j].z;
                RGB_point.r = r;
                RGB_point.g = g;
                RGB_point.b = b;

                color_cloud->emplace_back(RGB_point);
                // cv::circle(RGB_image, cv::Point(u, v), 3, cv::Scalar(255, 0, 0), -1);
//
                proj_image.at<cv::Vec3b>(v, u) = cv::Vec3b(b, g, r);
            }
        }

        // cv::imshow("RGB", proj_image);
        // cv::imshow("add", proj_image + RGB_image);
        // cv::waitKey(0);
//        cv::imwrite("/home/jsh/sos_proj_no1.png", RGB_image);
//        cv::imwrite("/home/jsh/sos_proj_no2.png", proj_image);
        // cv::imwrite("/home/jsh/calib_result_image.png", RGB_image);


//        std::ostringstream pcdname;
//        pcdname << "/home/jsh/"
//                 << std::setw(3) << std::setfill('0') << i << ".pcd";

//        pcl::io::savePCDFileASCII(pcdname.str(), *color_cloud);
        *output_cloud += *color_cloud;
//        std::cout<<color_cloud->size()<<std::endl;
    }
    pcl::io::savePCDFileASCII("/home/jsh/calib_result_image-.pcd", *output_cloud);

    return 0;
}