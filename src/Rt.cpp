#include "../include/marscalib/Rt.hpp"

namespace marscalib {

Rt::Rt() {}
Rt::~Rt() {}

marscalib::ReprojectionCost::~ReprojectionCost() {}


bool Rt::run(int argc, char**argv) {
    using namespace boost::program_options;
    options_description description("camera");
    description.add_options()
            ("help", "produce help message")
            ("input_path", value<std::string>(), "path of preprocess folder")
            ;
    positional_options_description p;
    p.add("input_path", 1);

    variables_map vm;
    store(command_line_parser(argc, argv).options(description).positional(p).run(), vm);
    notify(vm);

    if (vm.count("help") || !vm.count("input_path")) {
        std::cout << description << std::endl;
        return true;
    }

    std::string input_path = vm["input_path"].as<std::string>();
    if (input_path.back() != '/') {
        input_path += '/';
    }
    std::cout << "\n*--------------------------------------------------------------------*"<<std::endl;
    std::cout << "\n   *  input_path: " << input_path << std::endl;

    int folder_count = 0;  // number of scene
    for (const auto& entry : std::filesystem::directory_iterator(input_path)) {
        if (std::filesystem::is_directory(entry.path())) {
            folder_count++;
        }
    }
    std::cout << "   *  There are " << folder_count << " scenes" << std::endl;
    std::cout << "\n*--------------------------------------------------------------------*\n\n"<<std::endl;

    nlohmann::json config_camera, config_lidar, config_intrinsic, config_T;


    std::ifstream intrinsic_ifs(input_path + "intrinsic.json");
    intrinsic_ifs >> config_intrinsic;

    Eigen::Vector4d intrinsic = Eigen::Map<Eigen::Vector4d>(config_intrinsic["camera"]["intrinsic"].get<std::vector<double>>().data());
    Eigen::VectorXd distortion = Eigen::Map<Eigen::VectorXd>(config_intrinsic["camera"]["distortion"].get<std::vector<double>>().data(), 5);
    std::vector<int> error_index;

    Eigen::Isometry3d T_camera_lidar_init = Eigen::Isometry3d::Identity();
    Eigen::Isometry3d T_camera_lidar_corres = Eigen::Isometry3d::Identity();
    Eigen::Isometry3d T_lidar_camera = Eigen::Isometry3d::Identity();

    int count = 0;
    while(true) {
        std::cout << "*************      " << count + 1 << "-th try      *******************\n" << std::endl;
        std::vector<std::pair<Eigen::Vector2d, Eigen::Vector3d>> correspondences;
        std::vector<int> index;

        std::cout << "   **********   Correspondence   **********\n" << std::endl;
        for (int k = 1; k < folder_count + 1; k++) {
            std::ifstream camera_ifs(input_path + std::to_string(k) + "/2D_center.json");
            std::ifstream lidar_ifs(input_path + std::to_string(k) + "/3D_center.json");

            std::cerr << "      - Scene " << k << " :  ";

            if (std::find(error_index.begin(), error_index.end(), k) != error_index.end()) {
                std::cerr << "Wrong correspondence, so pass." << std::endl;
                continue;
            }


            if (camera_ifs.peek() == std::ifstream::traits_type::eof()) {
                std::cerr << "2D center is not found, so pass." << std::endl;
                continue;
            }

            if (lidar_ifs.peek() == std::ifstream::traits_type::eof()) {
                std::cerr << "3D center is not found, so pass." << std::endl;
                continue;
            }
            std::cout << "correspondence added." << std::endl;

            camera_ifs >> config_camera;
            lidar_ifs >> config_lidar;

            std::vector<double> center2D = config_camera["Camera"];
            std::vector<double> center3D = config_lidar["LiDAR"];

            if (config_lidar["LiDAR"].is_null()) continue;
            if (!center3D.empty() || !center2D.empty()) {
                correspondences.emplace_back(Eigen::Vector2d(center2D[0], center2D[1]),
                                             Eigen::Vector3d(center3D[0], center3D[1], center3D[2]));
            }
            index.emplace_back(k);
        }
        error_index.clear();



        T_camera_lidar_init.linear() << 0, -1, 0,
                0, 0, -1,
                1, 0, 0;


        T_camera_lidar_corres = estimate_pose_lsq(correspondences, T_camera_lidar_init, intrinsic, distortion);


        float sum_x = 0.0;
        float sum_y = 0.0;
        std::cout << "\n   ********   Reprojection error   ********\n" << std::endl;
        int i = 0;
        for (const auto &[pt_2d, pt_3d]: correspondences) {
            auto transfer_3d = T_camera_lidar_corres * pt_3d;
            auto proj_2d = world2image(transfer_3d, intrinsic, distortion);
            Eigen::Vector2d reproj_error(std::abs(pt_2d[0] - proj_2d[0]), std::abs(pt_2d[1] - proj_2d[1]));

            if (reproj_error[0] < 20 && reproj_error[1] < 20) {
                std::cout << "      - Scene " << index[i] << " :  " << reproj_error[0] << ", " << reproj_error[1]
                          << std::endl;
            } else {
                std::cout << "      - Scene " << index[i] << " :  " << reproj_error[0] << ", " << reproj_error[1] << " (*)"
                          << std::endl;
                error_index.emplace_back(index[i]);
            }
            sum_x += std::abs(pt_2d[0] - proj_2d[0]);
            sum_y += std::abs(pt_2d[1] - proj_2d[1]);
            i++;
        }
        std::cout << "       --------------------------------------    " << std::endl;

        std::cout << "      - Average :  " << sum_x / static_cast<float>(correspondences.size()) << ", "
                  << sum_y / static_cast<float>(correspondences.size()) << "\n\n" << std::endl;


        T_lidar_camera = T_camera_lidar_corres.inverse();



        std::cout << "   ***************   T_lc   ***************\n" << std::endl;
        std::cout << T_lidar_camera.matrix() << "\n\n" << std::endl;
        count++;

        if(error_index.empty()){
            break;
        }
        else {
            std::cout << "There are wrong correspondences(*) included, so calculate [R|t] again.\n\n\n" << std::endl;
        }
        if (count > 4) {
            std::cout << "   Small number of scene." << std::endl;
            break;
        }
    }
    const Eigen::Vector3d trans(T_camera_lidar_corres.translation());
    const Eigen::Quaterniond quat(T_camera_lidar_corres.linear());

    const std::vector<double> T_camera_lidar_values = {trans.x(), trans.y(), trans.z(), quat.x(), quat.y(),
                                                       quat.z(), quat.w()};


    config_T["results"]["T_lidar_camera"] = T_camera_lidar_values;
    std::ofstream ofs(input_path + "result.json");
    ofs << config_T.dump(4);
    ofs.close();

    return 0;
}




ReprojectionCost::ReprojectionCost(const Eigen::Vector3d& point_3d, const Eigen::Vector2d& point_2d, Eigen::Vector4d& intrinsic, Eigen::VectorXd& distortion) :
        point_3d(point_3d),
        point_2d(point_2d),
        intrinsic(intrinsic),
        distortion(distortion) {}

template <typename T>
bool ReprojectionCost::operator()(const T* const T_camera_lidar_params, T* residual) const {
    const Eigen::Map<Sophus::SE3<T> const> T_camera_lidar(T_camera_lidar_params);
    const Eigen::Matrix<T, 3, 1> pt_camera = T_camera_lidar * point_3d;

    const auto pt_2d = (pt_camera.template head<2>() / pt_camera.z()).eval();
    const auto& k1 = distortion[0];
    const auto& k2 = distortion[1];
    const auto& k3 = distortion[4];

    const auto& p1 = distortion[2];
    const auto& p2 = distortion[3];

    const auto x2 = pt_2d.x() * pt_2d.x();
    const auto y2 = pt_2d.y() * pt_2d.y();
    const auto xy = pt_2d.x() * pt_2d.y();

    const auto r2 = x2 + y2;
    const auto r4 = r2 * r2;
    const auto r6 = r2 * r4;

    const auto r_coeff = 1.0 + k1 * r2 + k2 * r4 + k3 * r6;
    const auto t_coeff1 = 2.0 * xy;
    const auto t_coeff2 = r2 + 2.0 * x2;
    const auto t_coeff3 = r2 + 2.0 * y2;

    const auto x = r_coeff * pt_2d.x() + p1 * t_coeff1 + p2 * t_coeff2;
    const auto y = r_coeff * pt_2d.y() + p1 * t_coeff3 + p2 * t_coeff1;

    const auto fx = (intrinsic[0]);
    const auto fy = (intrinsic[1]);
    const auto cx = (intrinsic[2]);
    const auto cy = (intrinsic[3]);

    const T projected_x = fx * x + cx;
    const T projected_y = fy * y + cy;

    residual[0] = projected_x - T(point_2d[0]);
    residual[1] = projected_y - T(point_2d[1]);

    return true;
}


Eigen::Isometry3d Rt::estimate_pose_lsq(
        const std::vector<std::pair<Eigen::Vector2d, Eigen::Vector3d>>& correspondences,
        const Eigen::Isometry3d& init_T_camera_lidar, Eigen::Vector4d& intrinsic, Eigen::VectorXd& distortion) {

    auto T_camera_lidar = Sophus::SE3d(init_T_camera_lidar.matrix());
    ceres::Problem problem;

    problem.AddParameterBlock(T_camera_lidar.data(), Sophus::SE3d::num_parameters, new Sophus::Manifold<Sophus::SE3>());

    for (const auto& [pt_2d, pt_3d] : correspondences) {
        auto reproj_error = new ReprojectionCost(pt_3d, pt_2d, intrinsic, distortion);
        // ceres::AutoDiffCostFunction  : , dimension of residual, dimension of input1 ,input2, ...)
        auto ad_cost = new ceres::AutoDiffCostFunction<ReprojectionCost, 2, Sophus::SE3d::num_parameters>(reproj_error);
        auto loss = new ceres::CauchyLoss(30.0);
        problem.AddResidualBlock(ad_cost, loss, T_camera_lidar.data());
    }

    // Solve!
    ceres::Solver::Options options;
    options.linear_solver_type = ceres::DENSE_QR;
    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);
    std::cout << "\n\n" << std::endl;
//    std::cout << summary.BriefReport() << std::endl;

    return Eigen::Isometry3d(T_camera_lidar.matrix());
}

Eigen::Vector2d Rt::world2image(const Eigen::Vector3d& pt_3d, const Eigen::Vector4d& intrinsic, const Eigen::VectorXd& distortion){
    Eigen::Vector2d pt_2d = Eigen::Vector2d::Zero();
    pt_2d = pt_2d.normalized();
    pt_2d[0] = pt_3d[0] / pt_3d[2];
    pt_2d[1] = pt_3d[1] / pt_3d[2];

    double k1 = distortion[0];
    double k2 = distortion[1];
    double k3 = distortion[4];

    double p1 = distortion[2];
    double p2 = distortion[3];

    double x2 = std::pow(pt_2d[0], 2);
    double y2 = std::pow(pt_2d[1], 2);

    double r2 = x2 + y2;
    double r4 = std::pow(r2, 2);
    double r6 = std::pow(r2, 3);

    double r_coeff = 1.0 + k1 * r2 + k2 * r4 + k3 * r6;
    double t_coeff1 = 2.0 * pt_2d[0] * pt_2d[1];
    double t_coeff2 = r2 + 2.0 * x2;
    double t_coeff3 = r2 + 2.0 * y2;

    pt_2d[0] = r_coeff * pt_2d[0] + p1 * t_coeff1 + p2 * t_coeff2;
    pt_2d[1] = r_coeff * pt_2d[1] + p1 * t_coeff3 + p2 * t_coeff1;

    const auto& fx = intrinsic[0];
    const auto& fy = intrinsic[1];
    const auto& cx = intrinsic[2];
    const auto& cy = intrinsic[3];

    return {fx * pt_2d[0] + cx, fy * pt_2d[1] + cy};
}

}



int main(int argc, char** argv) {
    marscalib::Rt rt;
    rt.run(argc, argv);

    return 0;
}