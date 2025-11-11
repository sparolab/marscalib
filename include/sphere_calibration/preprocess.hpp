#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include <boost/program_options.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rosbag2_cpp/reader.hpp>
#include <rosbag2_storage/storage_filter.hpp>

#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/io/pcd_io.h>
#include <pcl/PCLPointCloud2.h>
#include <pcl/conversions.h>


namespace marscalib {

class Preprocess {
public:
    Preprocess();
    ~Preprocess();

    bool run(int argc, char** argv);

private:
    std::pair<std::string, std::string> get_topics(const boost::program_options::variables_map& vm, const std::string& bag_filename);
    std::vector<std::pair<std::string, std::string>> get_topics_types(const std::string& bag_filename);
    std::pair<cv::Mat, pcl::PointCloud<pcl::PointXYZI>::Ptr> image_acc_points(const std::string& bag_filename,
                                                             const std::string& image_topic, const std::string& points_topic);
    cv::Mat get_image(const std::string& bag_filename, const std::string& image_topic);
    template <typename T>
    std::shared_ptr<T> get_first_message(const std::string& bag_filename, const std::string& topic);
};


}