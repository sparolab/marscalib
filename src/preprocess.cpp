#include "../include/marscalib/preprocess.hpp"

#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>

#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>

#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/msg/compressed_image.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <pcl_conversions/pcl_conversions.h>



namespace marscalib {

Preprocess::Preprocess() {}
Preprocess::~Preprocess() {}


bool Preprocess::run(int argc, char **argv) {
    rclcpp::Logger logger = rclcpp::get_logger("rosbag2_storage");
    logger.set_level(rclcpp::Logger::Level::Warn);

    using namespace boost::program_options;
    options_description description("preprocess");
    description.add_options()
            ("bag_path", value<std::string>(), "bag directory")
            ("auto_topic,a", "automatically select topics")
            ("image_topic", value<std::string>())
            ("points_topic", value<std::string>())
            ("camera_intrinsic", value<std::string>(), "camera intrinsic parameters [fx,fy,cx,cy] (don't put spaces between values)")
            ("camera_distortion", value<std::string>(), "camera distortion parameters [k1,k2,p1,p2,k3] (don't put spaces between values)")
            ;

    positional_options_description p;
    p.add("bag_path", 1);

    variables_map vm;
    store(command_line_parser(argc, argv).options(description).positional(p).run(), vm);
    notify(vm);

    if (vm.count("help") || !vm.count("bag_path")) {
        std::cout << description << std::endl;
        return true;
    }

    std::string bag_path = vm["bag_path"].as<std::string>();


    if (bag_path.back() == '/') {
        bag_path.pop_back();
    }

    size_t last_slash_pos = bag_path.find_last_of('/');
    std::string dst_path = bag_path.substr(0, last_slash_pos + 1) + bag_path.substr(last_slash_pos + 1) + "_preprocess";
    std::cout << "\n* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n" <<std::endl;
    std::cout << "- bag_path    : " << bag_path << std::endl;
    std::cout << "- output_path : " << dst_path << std::endl;

    nlohmann::json config;
    std::vector<double> intrinsic(4, 0.0);
    std::vector<double> distortion(5, 0.0);

    if (vm.count("camera_intrinsic")) {
        std::cout << "- camera_intrinsic : ";
        std::string intrinsic_str = vm["camera_intrinsic"].as<std::string>();
        std::stringstream ss(intrinsic_str);
        for (int j = 0; j < 4; ++j) {
            std::string value;
            if (std::getline(ss, value, ',')) {
                intrinsic[j] = std::stod(value);
                std::cout << intrinsic[j];
                if (j < 3) {
                    std::cout << ", ";
                }
            }
        }
        std::cout<<std::endl;
    }

    if (vm.count("camera_distortion")) {
        std::cout << "- camera_distortion : ";

        std::string distortion_str = vm["camera_distortion"].as<std::string>();
        std::stringstream ss(distortion_str);
        for (int j = 0; j < 5; ++j) {
            std::string value;
            if (std::getline(ss, value, ',')) {
                distortion[j] = std::stod(value);
                std::cout << distortion[j];
                if (j < 4) {
                    std::cout << ", ";
                }
            }
        }
        std::cout<<std::endl;
    }


    config["camera"]["intrinsic"] = intrinsic;
    config["camera"]["distortion"] = distortion;

    std::ofstream ofs(dst_path + "/intrinsic.json");
    ofs << config.dump(4) << std::endl;
    ofs.close();


    std::vector<std::string> bag_filenames;
    for (const auto &path: std::filesystem::directory_iterator(bag_path)) {
        std::string extension = path.path().extension().string();
        if (extension != ".db3") {
            continue;
        }
        bag_filenames.emplace_back(path.path().string());
    }

    if (bag_filenames.empty()) {
        std::cerr << "Error: No input bag!!" << std::endl;
        return true;
    }

    std::sort(bag_filenames.begin(), bag_filenames.end());

    std::cout << "- There are " << bag_filenames.size() << " bags found" << std::endl;


    const auto topics = get_topics(vm, bag_filenames.front());
    const auto image_topic = std::get<0>(topics);
    const auto points_topic = std::get<1>(topics);

    std::cout << "\n* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n" <<std::endl;

    for (int i = 1; i < bag_filenames.size() + 1; i++) {

        std::cout  << i << "-th bag preprocessing" << std::endl;
        const auto &bag_filename = bag_filenames[i-1];
        std::cout<<bag_filename<<std::endl;
        std::filesystem::create_directories(dst_path + "/" + std::to_string(i));

        auto [image, points] = image_acc_points(bag_filename, image_topic, points_topic);

        std::cout << "   - PCD saved" << std::endl;
        std::cout << "         Accumulated pointcloud size :  " << points->size() << std::endl;
        pcl::io::savePCDFileBinary(dst_path + "/" + std::to_string(i) + "/pointcloud.pcd", *points);
        cv::imwrite(dst_path + "/" + std::to_string(i) + "/image.png", image);
        std::cout << "   - Image saved\n" << std::endl;
    }


    return true;
}


std::pair<std::string, std::string> Preprocess::get_topics(const boost::program_options::variables_map& vm, const std::string &bag_filename) {
    std::string image_topic;
    std::string points_topic;
    if (vm.count("auto_topic")) {
        const auto topics_and_types = get_topics_types(bag_filename);
        std::cout << "- Topic in the bag : " << std::endl;
        for (const auto &[topic, type]: topics_and_types) {
            std::cout << "    - " << topic << " : " << type << std::endl;
            if (type.find("Image") != std::string::npos) {
                if (!image_topic.empty()) {
                    std::cerr << "Warning: bag contains multiple image topics!!" << std::endl;
                }
                image_topic = topic;
            } else if (type.find("PointCloud2") != std::string::npos) {
                if (!points_topic.empty()) {
                    std::cerr << "Warning: bag contains multiple points topics!!" << std::endl;
                }
                points_topic = topic;
            }
        }
    }
    else {
        image_topic = vm["image_topic"].as<std::string>();
        points_topic = vm["points_topic"].as<std::string>();
    }

    if (image_topic.empty()) {
        std::cerr << "Warning: failed to get image topic!!" << std::endl;
    }
    if (points_topic.empty()) {
        std::cerr << "Warning: failed to get points topic!!" << std::endl;
    }

    return {image_topic, points_topic};
}

std::vector<std::pair<std::string, std::string>> Preprocess::get_topics_types(const std::string &bag_filename) {
    rosbag2_cpp::Reader reader;

    reader.open(bag_filename);

    std::vector<std::pair<std::string, std::string>> topics_and_types;
    for (const auto &topic_metadata: reader.get_all_topics_and_types()) {
        topics_and_types.emplace_back(topic_metadata.name, topic_metadata.type);
    }
    reader.close();
    return topics_and_types;
}


std::pair<cv::Mat, pcl::PointCloud<pcl::PointXYZI>::Ptr> Preprocess::image_acc_points(
        const std::string &bag_filename,
        const std::string &image_topic,
        const std::string &points_topic){

    cv::Mat image = get_image(bag_filename, image_topic);


    rosbag2_cpp::Reader reader;
    reader.open(bag_filename);

    rosbag2_storage::StorageFilter filter;
    filter.topics.emplace_back(points_topic);
    reader.set_filter(filter);

    pcl::PointCloud<pcl::PointXYZI>::Ptr accumulated_cloud(new pcl::PointCloud<pcl::PointXYZI>);

    rclcpp::Serialization<sensor_msgs::msg::PointCloud2> serialization;

    while (reader.has_next()) {
        const auto serialized_message = reader.read_next();
        rclcpp::SerializedMessage serialized_msg(*serialized_message->serialized_data);

        auto pointcloud_msg = std::make_shared<sensor_msgs::msg::PointCloud2>();
        serialization.deserialize_message(&serialized_msg, pointcloud_msg.get());

        pcl::PointCloud<pcl::PointXYZI>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZI>);
        pcl::fromROSMsg(*pointcloud_msg, *cloud);

        // Step 7: Accumulate the points
        *accumulated_cloud += *cloud;
    }
    reader.close();
    return {image, accumulated_cloud};
}

cv::Mat Preprocess::get_image(const std::string &bag_filename, const std::string &image_topic) {
    if (image_topic.find("compressed") == std::string::npos) {
        const auto image_msg = get_first_message<sensor_msgs::msg::Image>(bag_filename, image_topic);
        return cv_bridge::toCvCopy(*image_msg, sensor_msgs::image_encodings::BGR8)->image;
    }

    const auto image_msg = get_first_message<sensor_msgs::msg::CompressedImage>(bag_filename, image_topic);
    return cv_bridge::toCvCopy(*image_msg, sensor_msgs::image_encodings::BGR8)->image;
}

template <typename T>
std::shared_ptr<T> Preprocess::get_first_message(const std::string& bag_filename, const std::string& topic)  {
    rosbag2_cpp::Reader reader;

    reader.open(bag_filename);

    rosbag2_storage::StorageFilter filter;
    filter.topics.emplace_back(topic);
    reader.set_filter(filter);

    if (!reader.has_next()) {
        std::cerr << "error: bag does not contain topic " << std::endl;
        std::cerr << "     : bag_filename=" << bag_filename << std::endl;
        abort();
    }

    rclcpp::Serialization<T> serialization;
    const auto msg = reader.read_next();
    const rclcpp::SerializedMessage serialized_msg(*msg->serialized_data);

    auto deserialized = std::make_shared<T>();
    serialization.deserialize_message(&serialized_msg, deserialized.get());
    reader.close();
    return deserialized;
}

}; // namespace marscalib





int main(int argc, char** argv) {
    marscalib::Preprocess preprocess;
    preprocess.run(argc, argv);

    return 0;
}
