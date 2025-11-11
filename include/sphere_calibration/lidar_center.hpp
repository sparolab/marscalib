#pragma once

#include <iostream>
#include <fstream>
#include <array>
#include <vector>
#include <cmath>
#include <eigen3/Eigen/Core>
#include <random>

#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <boost/program_options.hpp>

#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/io/pcd_io.h>
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/filters/statistical_outlier_removal.h>
#include <pcl/common/common.h>


namespace marscalib {

class LiDAR {
public:
    LiDAR();
    ~LiDAR();

    bool run(int argc, char **argv);

private:
    void dist2cent(pcl::PointCloud<pcl::PointXYZI>::Ptr &sphere_cloud, pcl::PointXYZI center, pcl::PointCloud<pcl::PointXYZI>::Ptr &incid_cloud, float radius);
    bool eval_cent(pcl::PointCloud<pcl::PointXYZI>::Ptr &sphere_cloud, pcl::PointXYZI center, float radius, float thresh);
    pcl::PointCloud<pcl::PointXYZI>::Ptr evaluate_representative_points(pcl::PointCloud<pcl::PointXYZI>::Ptr& cloud);
    std::vector<std::vector<int>> generateCombinations(const pcl::PointCloud<pcl::PointXYZI>::Ptr& cloud);
    bool check_if_plane(pcl::PointCloud<pcl::PointXYZI>::Ptr &sphere_cloud, std::vector<int> idx);
    std::map<std::string, int> voxelize_cluster(pcl::PointCloud<pcl::PointXYZI>::Ptr &cloud, float size);
    void fitSphere4p(pcl::PointCloud<pcl::PointXYZI>::Ptr &sphere_cloud, std::vector<int> idx, Eigen::Vector4f& sphere_params);
    };

} //namespace marscalib