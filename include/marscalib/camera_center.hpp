#pragma once

#include <iostream>
#include <fstream>
#include <eigen3/Eigen/Core>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include <boost/program_options.hpp>
#include <boost/range/algorithm.hpp>


namespace marscalib {

class Camera {
public:
Camera();
~Camera();

bool run(int argc, char** argv);

private:
    static double computeMedian(cv::Mat &image);
    static float distance_p2e(cv::RotatedRect &ellipse, cv::Point2f point);
    static std::tuple<cv::RotatedRect, std::vector<cv::Point2f>, int> extract_ellipse(const cv::Mat& edgeImage, float Ratio_sample);
    static bool evaluate_ellipse(cv::RotatedRect &ellipse, cv::Mat image, float threshold_inner, float threshold_outer);
    static cv::RotatedRect choose_optimal_ellipse(std::vector<cv::RotatedRect> ellipses);
    static std::pair<cv::Point2f, float> find_real_center(cv::RotatedRect &ellipse, float &focal_length, cv::Point2f image_center);
    void on_mouse(int event, int x, int y, int flags);
    static void mouse_callback(int event, int x, int y, int flags, void* userdata) {
        Camera* inst = reinterpret_cast<Camera*>(userdata);
        inst->on_mouse(event, x, y, flags);
    }};

    cv::Point picked_point;

} //namespace marscalib