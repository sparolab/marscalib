#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <array>
#include <vector>
#include <random>
#include <cmath>

#include <pcl/point_types.h>
#include <pcl/features/normal_3d.h>
#include <pcl/point_cloud.h>
#include <pcl/io/pcd_io.h>
#include <pcl/common/common.h>
#include <pcl/visualization/pcl_visualizer.h>
#include <nlohmann/json.hpp>
#include <boost/program_options.hpp>

#include <ceres/ceres.h>
#include <ceres/problem.h>
#include <ceres/rotation.h>
#include <sophus/se3.hpp>
#include <sophus/ceres_manifold.hpp>

namespace marscalib {

class Rt {
public:
    Rt();
    ~Rt();

    bool run(int argc, char **argv);

private:
    Eigen::Isometry3d estimate_pose_lsq(const std::vector<std::pair<Eigen::Vector2d, Eigen::Vector3d>>& correspondences,
                                        const Eigen::Isometry3d& init_T_camera_lidar, Eigen::Vector4d& intrinsic, Eigen::VectorXd& distortion);
    Eigen::Vector2d world2image(const Eigen::Vector3d& pt_3d, const Eigen::Vector4d& intrinsic, const Eigen::VectorXd& distortion);
};

class ReprojectionCost {
public:
    ReprojectionCost(const Eigen::Vector3d& point_3d, const Eigen::Vector2d& point_2d, Eigen::Vector4d& intrinsic, Eigen::VectorXd& distortion);
    ~ReprojectionCost();

    template <typename T>
    bool operator()(const T* const T_camera_lidar_params, T* residual) const;

private:
    const Eigen::Vector3d point_3d;
    const Eigen::Vector2d point_2d;
    const Eigen::Vector4d intrinsic;
    const Eigen::VectorXd distortion;
};
} //marscalib