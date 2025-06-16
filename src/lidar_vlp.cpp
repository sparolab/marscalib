#include "../include/marscalib/lidar_center.hpp"


#include <unordered_map>

namespace marscalib {
LiDAR::LiDAR() {}
LiDAR::~LiDAR() {}


bool LiDAR::run(int argc, char** argv) {
    using namespace boost::program_options;
    options_description description("lidar");
    description.add_options()
            ("help", "produce help message")
            ("input_path", value<std::string>(), "path of preprocess folder")
            ("lidar_type", value<std::string>(),"lidar product :  o(ouster), s(sos), m(mid)")
            ("radius", value<std::string>(),"radius of the target sphere[m]")
            ("visualize,v", "if true, show every step of processing")
            ;

    positional_options_description p;
    p.add("input_path", 1);
    p.add("lidar_type", 1);
    p.add("radius", 1);

    variables_map vm;
    store(command_line_parser(argc, argv).options(description).positional(p).run(), vm);
    notify(vm);

    if (vm.count("help") || !vm.count("input_path") || !vm.count("lidar_type") || !vm.count("radius")) {
        std::cout << description << std::endl;
        return true;
    }

    std::string input_path = vm["input_path"].as<std::string>();
    const std::string lidar = vm["lidar_type"].as<std::string>();
    const std::string radius = vm["radius"].as<std::string>();

    if (input_path.back() != '/') {
        input_path += '/';
    }

    std::cout << "- input_path : " << input_path << std::endl;
    if(lidar == "m") {
        std::cout << "- LiDAR      : Mid 360" << std::endl;
    }
    else if(lidar == "o"){
        std::cout << "- LiDAR      : Ouster" << std::endl;
    }
    else if(lidar == "s"){
        std::cout << "- LiDAR      : Sos" << std::endl;
    }
    else {
        std::cout << "- LiDAR      : Wrong type of LiDAR" << std::endl;
    }
    std::cout << "- radius     : " << radius <<" [m]" << std::endl;


    int folder_count = 0;  // number of scene
    for (const auto& entry : std::filesystem::directory_iterator(input_path)) {
        if (std::filesystem::is_directory(entry.path())) {
            folder_count++;
        }
    }
    std::cout << "- There are " << folder_count << " scenes\n" << std::endl;


    std::unordered_map<std::string, std::vector<float>> lidar_thresholds = {
            {"o", {0.06f, 0.01f, 0.15f, 0.05f}},
            {"s", {0.035f, 0.0f, 0.5f, 0.05f}}
    };
    std::vector<float> thresh = lidar_thresholds.count(lidar) ? lidar_thresholds[lidar] : std::vector<float>{};

    for (int k = 1; k < folder_count + 1; k++) {
        std::cout<<"* "<<k<<"-th scene processing" <<std::endl;
        std::string target = std::to_string(k);
        std::string data_path = input_path + target + "/";

        nlohmann::json config;
        std::ofstream ofs(data_path + "/3D_center.json");


        pcl::PointCloud<pcl::PointXYZI>::Ptr raw_cloud(new pcl::PointCloud<pcl::PointXYZI>());
        pcl::PointCloud<pcl::PointXYZI>::Ptr sor_cloud(new pcl::PointCloud<pcl::PointXYZI>());
        pcl::PointCloud<pcl::PointXYZI>::Ptr voxel_cloud(new pcl::PointCloud<pcl::PointXYZI>());
        pcl::PointCloud<pcl::PointXYZI>::Ptr sphere_cloud(new pcl::PointCloud<pcl::PointXYZI>());
        pcl::PointCloud<pcl::PointXYZI>::Ptr sphere_cloud2(new pcl::PointCloud<pcl::PointXYZI>());
        pcl::PointCloud<pcl::PointXYZI>::Ptr center_cloud(new pcl::PointCloud<pcl::PointXYZI>());
        pcl::PointCloud<pcl::PointXYZI>::Ptr output_cloud(new pcl::PointCloud<pcl::PointXYZI>());
        pcl::PointCloud<pcl::PointXYZI>::Ptr output2_cloud(new pcl::PointCloud<pcl::PointXYZI>());
        pcl::PointCloud<pcl::PointXYZI>::Ptr incid_cloud(new pcl::PointCloud<pcl::PointXYZI>());
        pcl::PointCloud<pcl::PointXYZI>::Ptr cluster_cloud(new pcl::PointCloud<pcl::PointXYZI>());


        pcl::io::loadPCDFile<pcl::PointXYZI>( data_path + "sphere.pcd", *raw_cloud);
        //        std::cout<<raw_cloud->size()<<std::endl;
        if (raw_cloud->empty()){
            std::cerr << "Wrong directory. Cannot find pcd file.\n\n" << std::endl;
            continue;
        }
        std::cout << "   - Input pointcloud is found" << std::endl;
        if (vm.count("visualize")) {
            pcl::visualization::PCLVisualizer viewer1("Raw cloud");
            viewer1.addPointCloud<pcl::PointXYZI>(raw_cloud, "raw");
            viewer1.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 5.0, "raw");
            viewer1.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_COLOR, 1.0, 1.0, 1.0,
                                                     "raw");
//            viewer1.spin();

            while (!viewer1.wasStopped()) {
                viewer1.spinOnce();
            }
            viewer1.close();
        }

        // Cluster the sphere pointcloud using Kd tree
        std::cout << "   - Begin clustering the input pointcloud!" << std::endl;
        if (lidar == "o" || lidar == "s") {
            pcl::EuclideanClusterExtraction<pcl::PointXYZI> ec;
            pcl::search::KdTree<pcl::PointXYZI>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZI>);
            ec.setInputCloud(raw_cloud);
            ec.setSearchMethod(tree);
            ec.setMinClusterSize(300);
            ec.setClusterTolerance(0.003);

            std::vector<pcl::PointIndices> cluster_indices;
            ec.extract(cluster_indices);
            std::vector<pcl::PointIndices> filtered_cluster_indices;

            // Find the cluster that is longer than thresh[0]. Long cluster means large noise(variance).
            for (const auto &indices: cluster_indices) {
                float min_x = std::numeric_limits<float>::max();
                float max_x = std::numeric_limits<float>::lowest();
                pcl::PointXYZI min_pt, max_pt;

                for (const auto &idx: indices.indices) {
                    pcl::PointXYZI point = (*raw_cloud)[idx];
                    if (point.x < min_x) {
                        min_x = point.x;
                        min_pt = point;
                    }
                    if (point.x > max_x) {
                        max_x = point.x;
                        max_pt = point;
                    }
                }

                float distance = std::sqrt(std::pow(max_pt.x - min_pt.x, 2) +
                                           std::pow(max_pt.y - min_pt.y, 2) +
                                           std::pow(max_pt.z - min_pt.z, 2));
                if (distance <= thresh[0] && distance >= thresh[1]) {
                    filtered_cluster_indices.push_back(indices);
                }
            }

            std::cout << "   - There are " << filtered_cluster_indices.size() << " clusters found." << std::endl;
            if (vm.count("visualize")) {
                pcl::visualization::PCLVisualizer viewer2("Clusters viewer");
                int j = 0;
                for (const auto &indices: filtered_cluster_indices) {
                    pcl::PointCloud<pcl::PointXYZI>::Ptr cluster_color_cloud(new pcl::PointCloud<pcl::PointXYZI>);
                    for (const auto &idx: indices.indices) {
                        cluster_color_cloud->points.push_back(raw_cloud->points[idx]);
                    }
                    cluster_color_cloud->width = cluster_color_cloud->points.size();
                    cluster_color_cloud->height = 1;
                    cluster_color_cloud->is_dense = true;

                    std::string cluster_name = "cluster_" + std::to_string(j);
                    pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZI> color_handler(cluster_color_cloud,
                                                                                                   rand() % 256,
                                                                                                   rand() % 256,
                                                                                                   rand() % 256);

                    viewer2.addPointCloud<pcl::PointXYZI>(cluster_color_cloud, color_handler, cluster_name);
                    viewer2.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 5.0,
                                                             cluster_name);
                    j++;
                }
//                viewer2.spin();

                while (!viewer2.wasStopped()) {
                    viewer2.spinOnce();
                }
                viewer2.close();
            }

            // ********** Representative Points selection **********
            //
            // 1. Voxelize the points in each cluster.
            // 2. Calculate frequency of the voxels using 'std::map'.
            // 3. Calculate weighted sum of the voxels.
            //
            // *****************************************************


            std::cout << "   - Processing Representative Points selection ... " << std::endl;

            for (const auto &indices: filtered_cluster_indices) {
                pcl::PointCloud<pcl::PointXYZI>::Ptr tmp_cloud(new pcl::PointCloud<pcl::PointXYZI>);
                for (const auto &idx: indices.indices) {
                    tmp_cloud->points.push_back(raw_cloud->points[idx]);
                    *cluster_cloud += *tmp_cloud;
                }


                std::map<std::string, int> point_frequency = voxelize_cluster(tmp_cloud, 100.0);

                Eigen::Vector3f weighted_sum{0.0, 0.0, 0.0};
                int point_number = 0;

//            for (const auto &entry: point_frequency) {
//                std::cout << "Key: " << entry.first << ", freq: " << entry.second << std::endl;
//            }

                for (const auto &entry: point_frequency) {
                    // entry.first = voxel's position
                    // entry.second = voxel's frequency
                    std::istringstream key_stream(entry.first);
                    std::string x_str, y_str, z_str;
                    std::getline(key_stream, x_str, '_');
                    std::getline(key_stream, y_str, '_');
                    std::getline(key_stream, z_str, '_');

                    float x = std::stof(x_str);
                    float y = std::stof(y_str);
                    float z = std::stof(z_str);

                    if (entry.second > tmp_cloud->size() * thresh[1]) {
                        point_number += entry.second;
                        weighted_sum[0] += x * entry.second;
                        weighted_sum[1] += y * entry.second;
                        weighted_sum[2] += z * entry.second;
                    }
                }

                if (point_number == 0) {
                    continue;
                }


                pcl::PointXYZI representative_point;   // 'representative_point' represents the voxel.
                representative_point.x = weighted_sum[0] / point_number;
                representative_point.y = weighted_sum[1] / point_number;
                representative_point.z = weighted_sum[2] / point_number;

                float min_distance = std::numeric_limits<float>::max();
                pcl::PointXYZI closest_point;

                for (const auto& point : tmp_cloud->points) {
                    // Compute Euclidean distance
                    float distance = std::sqrt(
                            std::pow(point.x - representative_point.x, 2) +
                            std::pow(point.y - representative_point.y, 2) +
                            std::pow(point.z - representative_point.z, 2)
                    );

                    if (distance < min_distance) {
                        min_distance = distance;
                        closest_point = point;
                    }
                }
                closest_point.intensity = cluster_cloud->points[0].intensity;


                sphere_cloud->emplace_back(closest_point);
            }

            if (lidar == "o"){
                sphere_cloud2 = evaluate_representative_points(sphere_cloud);
            }
            else { // lidar == "s"
                float max_intensity = 0.0f;
                for(const auto& point : *sphere_cloud){
                    if(point.intensity > max_intensity){
                        max_intensity = point.intensity;
                    }
                }
                float intensity_threshold = max_intensity * 0.95f;
                sphere_cloud->erase(
                        std::remove_if(
                                sphere_cloud->begin(),
                                sphere_cloud->end(),
                                [intensity_threshold](const auto& point) {
                                    return point.intensity < intensity_threshold;
                                }
                        ),
                        sphere_cloud->end()
                );
                sphere_cloud2 = sphere_cloud;
            }



            std::cout << "   - Points that represent each cluster are found! " << std::endl;
            if (vm.count("visualize")) {
                pcl::visualization::PCLVisualizer viewer3("Representative");
                viewer3.addPointCloud<pcl::PointXYZI>(cluster_cloud, "cluster");
                viewer3.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 2.0, "cluster");
                viewer3.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_COLOR, 1.0, 1.0, 1.0,
                                                         "cluster");
                viewer3.addPointCloud<pcl::PointXYZI>(sphere_cloud, "representative");
                viewer3.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 6.0, "representative");
                viewer3.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_COLOR, 1.0, 0.0, 0.0,
                                                         "representative");
                viewer3.addPointCloud<pcl::PointXYZI>(sphere_cloud2, "real");
                viewer3.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 6.0, "real");
                viewer3.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_COLOR, 0.0, 1.0, 0.0,
                                                         "real");
                while (!viewer3.wasStopped()) {
                    viewer3.spinOnce();
                }
                viewer3.close();
            }
        }


        else if (lidar == "m"){
            pcl::StatisticalOutlierRemoval<pcl::PointXYZI> sor;
            sor.setInputCloud (raw_cloud);
            sor.setMeanK (1000);
            sor.setStddevMulThresh (0.005);
            sor.filter(*sor_cloud);

            pcl::VoxelGrid<pcl::PointXYZI> voxel;
            voxel.setInputCloud(sor_cloud);
            voxel.setLeafSize(0.03f, 0.03f, 0.03f);
            voxel.filter(*voxel_cloud);

            std::cout<<voxel_cloud->points.size()<<std::endl;
            std::vector<int> indices(voxel_cloud->size());
            std::iota(indices.begin(), indices.end(), 0);  // Fill indices with 0 to voxel_cloud->size()-1

            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(indices.begin(), indices.end(), g);

            int num_samples = std::min(40, static_cast<int>(voxel_cloud->size()));  // Ensure there are at least 50 points
            for (int i = 0; i < num_samples; ++i) {
                sphere_cloud2->points.push_back(voxel_cloud->points[indices[i]]);
            }


            // float max_intensity = 0.0f;
            // for (const auto &point: *sphere_cloud2) {
            //     if (point.intensity > max_intensity) {
            //         max_intensity = point.intensity;
            //     }
            // }
            // float intensity_threshold = max_intensity * 0.90f;
            //
            // sphere_cloud2->erase(
            //         std::remove_if(
            //                 sphere_cloud2->begin(),
            //                 sphere_cloud2->end(),
            //                 [intensity_threshold](const auto &point) {
            //                     return point.intensity < intensity_threshold;
            //                 }
            //         ),
            //         sphere_cloud2->end()
            // );
        }


        if (sphere_cloud2->size() > 20) {
            std::vector<int> indices(sphere_cloud2->size());
            std::iota(indices.begin(), indices.end(), 0); // Fill indices with sequential numbers

            // Shuffle the indices randomly
            std::random_device rd;                  // Random number seed
            std::mt19937 generator(rd());           // Random number generator
            std::shuffle(indices.begin(), indices.end(), generator);

            // Select the first 20 indices after shuffling
            indices.resize(20);

            // Create a new point cloud to store the selected points
            pcl::PointCloud<pcl::PointXYZI>::Ptr selected_cloud(new pcl::PointCloud<pcl::PointXYZI>());

            for (const auto& idx : indices) {
                selected_cloud->push_back(sphere_cloud2->at(idx));
            }


            // Now `selected_cloud` contains 40 random points
            sphere_cloud2 = selected_cloud; // Replace sphere_cloud2 with the reduced set of points if needed
        }



        // ********** Sphere fitting & Center Extraction **********
        //
        // 1. Compute combinations of representative points. (nC4)
        // 2. Combinations that form plane cannot define a sphere, so exclude.
        // 3. Fit sphere for each combination.
        // 4. Calculate weighted sum of the voxels.
        //
        // *****************************************************

        pcl::PointXYZI minPt, maxPt;
        pcl::getMinMax3D(*sphere_cloud2, minPt,maxPt);
        Eigen::Vector4f sphere_params;

        std::vector<std::vector<int>> combinations = generateCombinations(sphere_cloud2);
        std::cout << "   - There are " << combinations.size() << " combinations can be made from representative points"
                  << std::endl;

        for (const auto &combination: combinations) {
            if (check_if_plane(sphere_cloud2, combination)) {
                fitSphere4p(sphere_cloud2, combination, sphere_params);

                // if (sphere_params(3) < std::stof(radius) + 0.005 &&
                // sphere_params(3) > std::stof(radius) - 0.005  && sphere_params(0) > maxPt.x){ // check if fitted sphere's radius is valid using known radius. threshold = 5mm.
                    pcl::PointXYZI tmp_center;
                    pcl::PointCloud<pcl::PointXYZI>::Ptr tmp_cloud(new pcl::PointCloud<pcl::PointXYZI>);
                    tmp_center.x = sphere_params(0);
                    tmp_center.y = sphere_params(1);
                    tmp_center.z = sphere_params(2);
                    center_cloud->points.emplace_back(tmp_center);
                // }
            }
        }
//        if(center_cloud->empty()) continue;


        std::map<std::string, int> center_frequency = voxelize_cluster(center_cloud, 500.0);
        std::cout << "   - There are " << center_frequency.size() << " center candidates for " << center_cloud->size()
                  << " centers" << std::endl;
        std::cout << "   - Apply weighted sum to center candidates" << std::endl;
//         float sum_x = 0.0;
//         float sum_y = 0.0;
//         float sum_z = 0.0;
//         float sum_weight = 0.0;
//         for (const auto &entry: center_frequency) {
//             pcl::PointCloud<pcl::PointXYZI>::Ptr tmp_cloud(new pcl::PointCloud<pcl::PointXYZI>);
//             pcl::PointXYZI tmp_point;
//
// //            std::cout << "Key: " << entry.first << ", freq: " << entry.second <<std::endl;
//
//             std::istringstream key_stream(entry.first);
//             std::string x_str, y_str, z_str;
//             std::getline(key_stream, x_str, '_');
//             std::getline(key_stream, y_str, '_');
//             std::getline(key_stream, z_str, '_');
//
//             tmp_point.x = std::stof(x_str);
//             tmp_point.y = std::stof(y_str);
//             tmp_point.z = std::stof(z_str);
//
//
//             if (eval_cent(sphere_cloud2, tmp_point, std::stof(radius), 0.01)) {
//                 sum_weight += entry.second;
//                 sum_x += entry.second * tmp_point.x;
//                 sum_y += entry.second * tmp_point.y;
//                 sum_z += entry.second * tmp_point.z;
//                 output_cloud->emplace_back(tmp_point);
//             }
//         }
//
//         if (output_cloud->empty()){
//
//             for (const auto &entry: center_frequency) {
//                 pcl::PointCloud<pcl::PointXYZI>::Ptr tmp_cloud(new pcl::PointCloud<pcl::PointXYZI>);
//                 pcl::PointXYZI tmp_point;
//
// //            std::cout << "Key: " << entry.first << ", freq: " << entry.second <<std::endl;
//
//                 std::istringstream key_stream(entry.first);
//                 std::string x_str, y_str, z_str;
//                 std::getline(key_stream, x_str, '_');
//                 std::getline(key_stream, y_str, '_');
//                 std::getline(key_stream, z_str, '_');
//
//                 tmp_point.x = std::stof(x_str);
//                 tmp_point.y = std::stof(y_str);
//                 tmp_point.z = std::stof(z_str);
//
//
//                 if (eval_cent(sphere_cloud2, tmp_point, std::stof(radius), thresh[2])) {
//                     sum_weight += entry.second;
//                     sum_x += entry.second * tmp_point.x;
//                     sum_y += entry.second * tmp_point.y;
//                     sum_z += entry.second * tmp_point.z;
//                     output_cloud->emplace_back(tmp_point);
//                 }
//             }
//         }
//         pcl::PointXYZI final_center;
//         final_center.x = sum_x / sum_weight;
//         final_center.y = sum_y / sum_weight;
//         final_center.z = sum_z / sum_weight;
//         if (std::isnan(final_center.x) || std::isnan(final_center.y) || std::isnan(final_center.z)) {
//             std::cout << "   - Warning: Center is not detected.\n\n" << std::endl;
//             continue;
//         }
//         std::cout << "   - Estimated center :  " << final_center.x << ", " << final_center.y << ", "
//               << final_center.z <<"\n\n" << std::endl;
//
//         output2_cloud->emplace_back(final_center);
//
//
//         dist2cent(sphere_cloud2, final_center, incid_cloud, std::stof(radius));
//        std::cout << std::sqrt(std::pow(final_center.x,2) + std::pow(final_center.y,2) + std::pow(final_center.z,2)) <<std::endl;

        pcl::visualization::PCLVisualizer viewer4("Result");
        viewer4.addPointCloud<pcl::PointXYZI>(raw_cloud, "cluster");
        viewer4.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 2.0, "cluster");
        viewer4.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_COLOR, 1.0, 1.0, 1.0,
                                                 "cluster");

        viewer4.addPointCloud<pcl::PointXYZI>(center_cloud, "candidates");
        viewer4.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 4.0, "candidates");
        viewer4.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_COLOR, 1.0, 0.0, 0.0,
                                                 "candidates");

        viewer4.addPointCloud<pcl::PointXYZI>(output_cloud, "center_candidate");
        viewer4.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 4.0, "center_candidate");
        viewer4.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_COLOR, 0.0, 1.0, 0.0,
                                                 "center_candidate");

        // viewer4.addPointCloud<pcl::PointXYZI>(output2_cloud, "final");
        // viewer4.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 6.0, "final");
        // viewer4.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_COLOR, 0.0, 0.0, 1.0,
        //                                          "final");
        while (!viewer4.wasStopped()) {
            viewer4.spinOnce();
        }
        viewer4.close();

        // config["LiDAR"] = {final_center.x, final_center.y, final_center.z};
        // ofs << config.dump(2) << std::endl;
        // ofs.close();

    }
    return true;
}




void LiDAR::dist2cent(pcl::PointCloud<pcl::PointXYZI>::Ptr &sphere_cloud, pcl::PointXYZI center, pcl::PointCloud<pcl::PointXYZI>::Ptr &incid_cloud, float radius){
    for (const auto& point : *sphere_cloud){
        float distance = std::sqrt(std::pow(point.x - center.x, 2) + std::pow(point.y - center.y, 2) + std::pow(point.z - center.z, 2));
//        std::cout<<distance<<std::endl;
        if(distance < radius + 0.008 && distance > radius - 0.008){
            incid_cloud->emplace_back(point);
        }
    }
}

bool LiDAR::eval_cent(pcl::PointCloud<pcl::PointXYZI>::Ptr &sphere_cloud, pcl::PointXYZI center, float radius, float thresh){
    for (const auto& point : *sphere_cloud){
        float distance = std::sqrt(std::pow(point.x - center.x, 2) + std::pow(point.y - center.y, 2) + std::pow(point.z - center.z, 2));
//        std::cout<<distance<<std::endl;
        if(distance > radius + thresh || distance < radius - thresh){
            return false;
        }
    }
    return true;
}

pcl::PointCloud<pcl::PointXYZI>::Ptr LiDAR::evaluate_representative_points(pcl::PointCloud<pcl::PointXYZI>::Ptr& cloud){
    pcl::PointCloud<pcl::PointXYZI>::Ptr output_cloud(new pcl::PointCloud<pcl::PointXYZI>);
    pcl::PointCloud<pcl::PointXYZI>::Ptr temp_cloud(new pcl::PointCloud<pcl::PointXYZI>);

    *temp_cloud = *cloud;

    std::srand(std::time(0));
    const double threshold = 0.01;

    while (temp_cloud->size() > 3) {

        std::vector<int> plane_points;
        int random_index = std::rand() % temp_cloud->size();


        pcl::PointXYZI init_point = temp_cloud->points[random_index];
        temp_cloud->points.erase(temp_cloud->points.begin() + random_index);

        pcl::PointCloud<pcl::PointXYZI>::Ptr plane_cloud(new pcl::PointCloud<pcl::PointXYZI>);
        plane_cloud->emplace_back(init_point);
        for (int i = 0; i < temp_cloud->size(); i++) {
            if (std::abs(init_point.z - temp_cloud->points[i].z) < 0.02){
                plane_points.emplace_back(i);
                plane_cloud->emplace_back(temp_cloud->points[i]);
            }
        }

        for (int i = plane_points.size() - 1; i >= 1; i--) {
            temp_cloud->points.erase(temp_cloud->points.begin() + plane_points[i]);
        }

        if (plane_points.size() < 3) {
            continue;
        }

        Eigen::Vector3f line_dir;
        int idx1, idx2;

        do {
//            std::cout<<"******************"<<std::endl;

            idx1 = std::rand() % plane_points.size();
            idx2 = std::rand() % plane_points.size();

            line_dir = (plane_cloud->points[idx2].getVector3fMap() - plane_cloud->points[idx1].getVector3fMap()).normalized();
//            std::cout<<line_dir.transpose()<<std::endl;
//            std::cout<<std::rand() % plane_points.size()<<", "<<plane_cloud->points[idx1]<<std::endl;
//            std::cout<<std::rand() % plane_points.size()<<", "<<plane_cloud->points[idx2]<<std::endl;
//            std::cout<<plane_cloud->size()<<std::endl;
//            for(const auto& point : *plane_cloud){
//                std::cout<<point<<std::endl;
//            }
        } while (line_dir.isZero());


        bool line_fit = std::all_of(plane_cloud->points.begin(), plane_cloud->points.end(),
                                    [&](const pcl::PointXYZI& p) {
                                        float distance = line_dir.cross(p.getVector3fMap() - plane_cloud->points[idx1].getVector3fMap()).norm();
                                        return distance <= threshold;
                                    });
        // Add plane to output if it does not fit the line
//        for(const auto& point : *plane_cloud){
//            std::cout<<point<<std::endl;
//        }
        if (!line_fit) *output_cloud += *plane_cloud;

//        pcl::visualization::PCLVisualizer viewer5("Raw cloud");
//        viewer5.addPointCloud<pcl::PointXYZI>(temp_cloud, "plane");
//        viewer5.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 5.0, "plane");
//        viewer5.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_COLOR, 0.0, 1.0, 1.0,
//                                                 "plane");
//        viewer5.addPointCloud<pcl::PointXYZI>(output_cloud, "outotd");
//        viewer5.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 5.0, "outotd");
//        viewer5.setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_COLOR, 1.0, 1.0, 1.0,
//                                                 "outotd");
//        while (!viewer5.wasStopped()) {
//            viewer5.spin();
//        }
//        viewer5.close();
    }

    return output_cloud;
}

std::vector<std::vector<int>> LiDAR::generateCombinations(const pcl::PointCloud<pcl::PointXYZI>::Ptr& cloud) {
    int n = cloud->points.size();
    std::vector<int> indices(n);
    std::iota(indices.begin(), indices.end(), 0);

    std::vector<int> combination(4);
    std::vector<std::vector<int>> combinations;

    for (int i = 0; i <= n - 4; ++i) {
        combination[0] = indices[i];
        for (int j = i + 1; j <= n - 4 + 1; ++j) {
            combination[1] = indices[j];
            for (int k = j + 1; k <= n - 4 + 2; ++k) {
                combination[2] = indices[k];
                for (int l = k + 1; l <= n - 4 + 3; ++l) {
                    combination[3] = indices[l];
                    combinations.push_back(combination);
                }
            }
        }
    }
    return combinations;
}

// Determine if the 4 points form a plane.
bool LiDAR::check_if_plane(pcl::PointCloud<pcl::PointXYZI>::Ptr &sphere_cloud, std::vector<int> idx){
    pcl::PointXYZI p1 = sphere_cloud->points[idx[0]];
    pcl::PointXYZI p2 = sphere_cloud->points[idx[1]];
    pcl::PointXYZI p3 = sphere_cloud->points[idx[2]];
    pcl::PointXYZI p4 = sphere_cloud->points[idx[3]];

    Eigen::Vector3f v1(p2.x - p1.x, p2.y - p1.y, p2.z - p1.z);
    Eigen::Vector3f v2(p3.x - p1.x, p3.y - p1.y, p3.z - p1.z);
    Eigen::Vector3f v3(p4.x - p1.x, p4.y - p1.y, p4.z - p1.z);

    float volume = v1.dot(v2.cross(v3));
    if (std::abs(volume) > 1e-2){
        return false;
    }
    else {
        return true;
    }
}

std::map<std::string, int> LiDAR::voxelize_cluster(pcl::PointCloud<pcl::PointXYZI>::Ptr &cloud, float size) {

    // if 'size' = 100, cell's size = 1/100 = 0.01m
    // if 'size' = 300, cell's size = 1/300 = 0.033m
    // if 'size' = 500, cell's size = 1/500 = 0.002mm

    std::map<std::string, int> point_frequency;
    for (const auto& point : *cloud) {
        Eigen::Vector4f mod_point;

        mod_point[0] = std::trunc(point.x * size) / size;
        mod_point[1] = std::trunc(point.y * size) / size;
        mod_point[2] = std::trunc(point.z * size) / size;
        mod_point[3] = 0;

        std::string key = std::to_string(mod_point[0]) + "_" +
                          std::to_string(mod_point[1]) + "_" +
                          std::to_string(mod_point[2]);

        // Increment the count for this key
        if (point_frequency.find(key) != point_frequency.end()) {
            point_frequency[key] += 1;
        } else {
            point_frequency[key] = 1;
        }
    }
    return point_frequency;
}


// fit sphere using 4 points.
void LiDAR::fitSphere4p(pcl::PointCloud<pcl::PointXYZI>::Ptr &sphere_cloud, std::vector<int> idx, Eigen::Vector4f& sphere_params) {
    int num_points = idx.size();
    Eigen::MatrixXf A(num_points, 4);
    Eigen::VectorXf B(num_points);

    for (int i = 0; i < num_points; ++i) {
        int index = idx[i];
        float x = sphere_cloud->points[index].x;
        float y = sphere_cloud->points[index].y;
        float z = sphere_cloud->points[index].z;
        float x2 = x * x;
        float y2 = y * y;
        float z2 = z * z;

        A(i, 0) = -2 * x;
        A(i, 1) = -2 * y;
        A(i, 2) = -2 * z;
        A(i, 3) = 1;

        B(i) = -(x2 + y2 + z2);
    }

    // Solve the linear system using Eigen library
    Eigen::Vector4f params = (A.transpose() * A).ldlt().solve(A.transpose() * B);

    // Extract the sphere parameters
    float a = params(0);
    float b = params(1);
    float c = params(2);
    float D = params(3);

    // Compute radius
    float radius = std::sqrt(a * a + b * b + c * c - D);

    sphere_params << a, b, c, radius;
}

}

int main(int argc, char** argv) {
    marscalib::LiDAR lidar;
    lidar.run(argc, argv);

    return 0;
}




