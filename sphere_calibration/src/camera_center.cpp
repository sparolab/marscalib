#include "../include/sphere_calibration/camera_center.hpp"

#include <array>
#include <cmath>
#include <random>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>




namespace marscalib {
    Camera::Camera() {}
    Camera::~Camera() {}


    bool Camera::run(int argc, char** argv){
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

        float RatioSamples = 0.15;   // if the ratio is too small, small ellipse is not detected
        float eval_threshold_inner = 0.15f;
        float eval_threshold_outer = 0.05f;

        nlohmann::json config_camera, config_ellipse_center;
        std::ifstream camera_ifs(input_path + "intrinsic.json");
        camera_ifs >> config_camera;
        std::vector<float> focal_length = config_camera["camera"]["intrinsic"];

        float fx = focal_length[0];
        if (fx == 0.0f) {
            std::cout<< "'intrinsic.json' is empty! Please fill intrinsic parameter and distortion coefficient."<<std::endl;
            return 0;
        }
        cv::Point2f image_center(focal_length[2], focal_length[3]);



        for (int k = 1; k < folder_count + 1; k++) {

            std::ofstream ofs(input_path + std::to_string(k) + "/2D_center.json");
            std::cout  << k << "-th scene processing ..." << std::endl;
            std::string target = input_path + std::to_string(k) + "/";

            std::vector<std::string> ellipse_index;
            std::vector<int> target_index;
            std::vector<cv::Point2f> ellipse_centers;
            std::vector<cv::RotatedRect> ellipses;
            std::vector<cv::Point2f> real_centers;
            std::vector<float> errors;

            std::string data_path = target + "mask/";
            std::ofstream outFile(data_path + "complement.txt");

            std::vector<cv::String> filenames;
            cv::glob(data_path, filenames);

            std::sort(filenames.begin(), filenames.end(), [](const std::string &a, const std::string &b) {
                return std::filesystem::path(a).filename() < std::filesystem::path(b).filename();
            });

            int break_iter = 0;
            while (break_iter <= 5) {
                for (int i = 0; i < filenames.size(); i++) {
                    std::filesystem::path filePath(filenames[i]);
                    std::string extension = filePath.extension().string();
                    if (extension != ".jpg" && extension != ".png" && extension != ".jpeg") {
                        continue;
                    }
                    size_t last_slash_pos = filenames[i].find_last_of('/');
                    std::string filename = filenames[i].substr(last_slash_pos + 1);

                    cv::Mat raw_image = cv::imread(filenames[i], cv::IMREAD_GRAYSCALE);

                    cv::Mat labels, stats, centroids;
                    int num_clusters = cv::connectedComponentsWithStats(raw_image, labels, stats, centroids, 4);

                    if (num_clusters >= 3) {
                        raw_image = cv::Mat::zeros(raw_image.size(), CV_8UC1);
                        for (int i = 1; i < num_clusters + 1; i++) {
                            int mask_size = stats.at<int>(i, cv::CC_STAT_AREA);
                            if (mask_size >= 200) {
                                raw_image.setTo(255, labels == i);
                            } else {
                                num_clusters--;
                            }
                        }
                    }

                    if (num_clusters - 1 != 1) {
                        outFile << filename << " :  More than 1 cluster in the image" << std::endl;
                        continue;
                    }


                    // Canny Edge Detection
                    cv::Mat edges;
                    double median = computeMedian(raw_image);

                    double lower_thresh = std::max(0.0, 0.66 * median);
                    double upper_thresh = std::min(255.0, 1.33 * median);
                    cv::Canny(raw_image, edges, lower_thresh, upper_thresh);

                    // Fit ellipses from the scene
                    auto result = extract_ellipse(edges, RatioSamples);
                    cv::RotatedRect ellipse = std::get<0>(result);
                    int reason = std::get<2>(result);


                    if (reason == 2) {
                        outFile << filename << " :  Initial ellipse is not formed" << std::endl;
                        continue;
                    } else if (reason == 3) {
                        outFile << filename << " :  Extracted ellipse is too small" << std::endl;
                        continue;
                    } else if (reason == 4) {
                        outFile << filename << " :  Invalid ellipse" << std::endl;
                        continue;
                    } else {
                        if (!evaluate_ellipse(ellipse, raw_image, eval_threshold_inner, eval_threshold_outer)) {
                            outFile << filename
                                    << " :  Too many points outside the ellipse or too many damaged part(maybe not sphere)"
                                    << std::endl;
                            continue;
                        }


                        if (reason == 0) outFile << filename << " :  *** Ellipse is extracted! *** " << std::endl;
                        else if (reason == 1)
                            outFile << filename << " :  *** Ellipse is extracted(damaged)! *** " << std::endl;

                        size_t last_dot_pos = filename.find_last_of('.');

                        ellipse_index.emplace_back(filename.substr(0, last_dot_pos));
                        ellipse_centers.emplace_back(ellipse.center);
                        ellipses.emplace_back(ellipse);
                    }
                }
                if (!ellipses.empty()) break;
                if (break_iter == 5) {
                    std::cout << "   - There are no ellipse detected in this scene"<<std::endl;
                    break;
                }
                break_iter++;
            }

            cv::RotatedRect optimal_ellipse;
            if (ellipses.size() > 1){
                optimal_ellipse = choose_optimal_ellipse(ellipses);
            }
            else if (ellipses.size() == 1){
                optimal_ellipse = ellipses[0];
            }

            // Center Compensation
            std::pair<cv::Point2f, float> result_real = find_real_center(optimal_ellipse, fx, image_center);
            if (result_real.second < 12.0) {
                real_centers.emplace_back(result_real.first);
                errors.emplace_back(result_real.second);
                config_ellipse_center["Camera"] = {result_real.first.x, result_real.first.y};

                ofs << config_ellipse_center.dump(2) << std::endl;
            }

            // Visualize the ellipse
            std::cout << "   - There is one target detected(red) in this scene." << std::endl;
            cv::Mat raw_image = cv::imread(input_path + "/" + std::to_string(k) + "/image.png");
            std::string window_name = "Scene " + std::to_string(k);
            cv::ellipse(raw_image, optimal_ellipse, cv::Scalar(0, 255, 0), 3);
            // cv::drawMarker(raw_image, optimal_ellipse.center, cv::MARKER_CROSS);
            // //                        cv::drawMarker(output_image, result_real.first, cv::Scalar(0, 255, 0), cv::MARKER_TILTED_CROSS, 5);
            // cv::circle(raw_image, result_real.first, 4, cv::Scalar(0, 255, 0), -1);

            cv::imshow(window_name, raw_image);
            std::cout << "   - Press any key to continue.\n" << std::endl;


            cv::imwrite(input_path + "/" + std::to_string(k) + "/image_output.png", raw_image);
            cv::waitKey(0);
            cv::destroyWindow(window_name);


            outFile << "\n\nEccentricity error :  " << errors[0] << std::endl;
            outFile << "Before compensation :  " << ellipse_centers[0] << std::endl;
            outFile << "After compensation :  " << real_centers[0] << std::endl;


            outFile.close();
            ofs.close();
        }
        std::cout<<"Camera ellipse detection done!"<<std::endl;
        return true;
    }



    double Camera::computeMedian(cv::Mat &image) {
        cv::Mat flatten_image;
        image.reshape(1, 1).copyTo(flatten_image);
        std::vector<double> vec_Mat;
        flatten_image.copyTo(vec_Mat);
        nth_element(vec_Mat.begin(), vec_Mat.begin() + vec_Mat.size() / 2, vec_Mat.end());
        return vec_Mat[vec_Mat.size() / 2];
    }

    float Camera::distance_p2e(cv::RotatedRect &ellipse, cv::Point2f point){
        cv::Point2f center = ellipse.center;
        cv::Size2f axes    = ellipse.size;
        float angle        = ellipse.angle;
        float angle2Rad    = angle * CV_PI / 180.0;
        float cosAngle     = cos(angle2Rad);
        float sinAngle     = sin(angle2Rad);

        cv::Point2f translatedPoint = point - center;
        cv::Point2f alignedPoint(
                translatedPoint.x * cosAngle + translatedPoint.y * sinAngle,
                translatedPoint.x * sinAngle - translatedPoint.y * cosAngle
        );
        float a = axes.width  / 2.0f;  // Semi-major axis
        float b = axes.height / 2.0f;  // Semi-minor axis
        float dst_p2e = alignedPoint.x * alignedPoint.x / (a * a) +
                        alignedPoint.y * alignedPoint.y / (b * b);
        return dst_p2e;
    }


    std::tuple<cv::RotatedRect, std::vector<cv::Point2f>, int> Camera::extract_ellipse(const cv::Mat& edgeImage, float Ratio_sample) {
        // *****************************************************
        // ************* Initial Ellipse Detection *************
        //
        // 1. Randomly sample points on mask (sampled_points) from pool (new_edge_point)
        // 2. fit ellipse
        // 3. if sample points are inside the fitted ellipse,
        //    remove the points from the pool.
        // 4. if all sample points are outside the ellipse, ellipse is detected.
        //
        // *****************************************************

        bool all_points_valid = false;
        int reason = 0;
        std::vector<cv::Point2f> edge_point;

        // Exclude points that are too close to the image end
        for (int y = 5; y < edgeImage.rows - 5; ++y) {
            for (int x = 5; x < edgeImage.cols - 5; ++x) {
                if (edgeImage.at<uchar>(y, x) > 0) {
                    edge_point.emplace_back(x, y);
                }
            }
        }

        int numSamples = static_cast<int>(edge_point.size() * Ratio_sample);
        if (numSamples < 5){ // mask is too small
            reason = 3;
            return std::make_tuple(cv::RotatedRect(), std::vector<cv::Point2f>(), reason);
        }


        std::vector<cv::Point2f> new_edge_point(edge_point.size()); // pool
        std::copy(edge_point.begin(), edge_point.end(), new_edge_point.begin());
        std::vector<cv::Point2f> sampled_points;

        std::random_device rd;
        std::mt19937 g(rd());


        cv::RotatedRect ellipse;

        // repeat until initial ellipse is found
        while(!all_points_valid) {
            std::vector<cv::Point2f> points_to_remove;
            sampled_points.clear();
            if (static_cast<int>(new_edge_point.size()) < numSamples) { // sampling pool is exhausted
                reason = 2;
                return std::make_tuple(cv::RotatedRect(), std::vector<cv::Point2f>(), reason);
            }
            std::sample(new_edge_point.begin(), new_edge_point.end(), std::back_inserter(sampled_points),
                        numSamples, g);
            ellipse = cv::fitEllipse(sampled_points);

            // remove the sampled point that are inside the extracted ellipse
            for (const auto &point: sampled_points) {
                float dst_p2e = distance_p2e(ellipse, point);
                if (dst_p2e < 0.95) {
                    points_to_remove.push_back(point);
//                std::cout<<dst_p2e<<std::endl;
                }
            }

            // if there is no point inside the ellipse, escape the loop!
            if (points_to_remove.empty()) {
                all_points_valid = true;
            }
                // remove the points that are in the ellipse.
            else {
                for (const auto &point: points_to_remove) {
                    new_edge_point.erase(std::remove(new_edge_point.begin(), new_edge_point.end(), point),
                                         new_edge_point.end());
                }
            }
        // cv::Mat before_eval;
        // cv::cvtColor(edgeImage, before_eval, cv::COLOR_GRAY2BGR);
        // cv::ellipse(before_eval, ellipse, cv::Scalar(0, 255, 0), 2);
        //
        // for (const auto& point : sampled_points) {
        //     cv::circle(before_eval, point, 4, cv::Scalar(0, 0, 255), -1);
        // }
        // cv::Point2f ellipse_center = ellipse.center;
        // // cv::circle(before_eval, ellipse_center, 5, cv::Scalar(255, 255, 255), -1);
        // cv::imshow("Before eval output",before_eval);
        // cv::waitKey(0);
        }
        // std::cout<<"initial done"<<std::endl;

        // if part of sphere is outside the camera's fov
        if (ellipse.center.x < 0 || ellipse.center.y < 0){
            reason = 3;
            return std::make_tuple(cv::RotatedRect(), std::vector<cv::Point2f>(), reason);
        }

        // if extracted ellipse's eccentricity is too large
        float a = std::max(ellipse.size.width  / 2.0f, ellipse.size.height / 2.0f);  // Semi-major axis
        float b = std::min(ellipse.size.width / 2.0f, ellipse.size.height / 2.0f);  // Semi-minor axis
        if (a / b >= 1.24){
            reason = 3;
            return std::make_tuple(ellipse, sampled_points, reason);
        }

        // if extracted ellipse is too small
        else if (ellipse.size.width < 40 || ellipse.size.height < 40){
            reason = 3;
            return std::make_tuple(cv::RotatedRect(), std::vector<cv::Point2f>(), reason);
        }

        // *****************************************************
        // *************** Ellipse Evaluation ******************
        //
        // 1. Histogramize the sampled_points
        // 2. Check if the sampled_points are scattered.
        // 3. If scattered, the initial ellipse is not damaged.
        // 4. If not, the initial ellipse is damaged.
        //    Rectification is needed.
        //
        // *****************************************************

        std::vector<float> degree_vec;
        for (int i = 0; i < sampled_points.size(); i++){
            cv::Point2f point = sampled_points[i];
            float degree = atan2((point.y - ellipse.center.y),(point.x - ellipse.center.x)) * (180.0 / CV_PI);
            if (degree < 0) {
                degree += 360;
            }
            degree_vec.emplace_back(degree);
        }

        int bin_size = 10;
        int num_bins = 360 / 10;
        std::vector<std::vector<int>> hist(num_bins);

        for (int i = 0; i < degree_vec.size(); ++i) {
            float degree = degree_vec[i];
            int bin_index = static_cast<int>(degree) / bin_size;
            if (bin_index == num_bins) {
                bin_index = 0;
            }
            hist[bin_index].push_back(i);
        }


        std::vector<std::vector<int>> clusters;
        std::vector<int> cluster;

        for (int i = 0; i < hist.size(); i++){
            const auto& bin = hist[i];
            if (bin.empty() && cluster.empty()){
                continue;
            }
            else if (bin.empty() && !cluster.empty()){
                clusters.emplace_back(cluster);
                cluster.clear();
            }
            else if (i != hist.size() - 1 && !bin.empty()){
                for (const auto& ele : bin){
                    cluster.emplace_back(ele);
                }
            }
            else if (i == hist.size() - 1 && !bin.empty() && !hist[0].empty()){
                for (const auto& ele : bin){
                    clusters[0].emplace_back(ele);
                }
            }
        }


        // Ellipse is considered undamaged.
        if (clusters.size() > 4 && a / b < 1.3 &&
            (ellipse.center.x > std::max(ellipse.size.width/2, ellipse.size.height/2)) &&
            (ellipse.center.y > std::max(ellipse.size.width/2, ellipse.size.height/2))){
            return std::make_tuple(ellipse, sampled_points, reason);
        }

        // Ellipse is considered damaged, so rectify it.
        else{
            for (int i = 0; i < 100; i++) {
                // new_sample_points                =  current sample points that form ellipse + new point
                // new_edge_point_no_sample_points  =  edge point without current ellipse points

                std::vector<cv::Point2f> new_sample_points;
                std::vector<cv::Point2f> new_sample_points_ds;
                std::vector<cv::Point2f> new_edge_point_no_sample_points(edge_point.size());
                std::copy(edge_point.begin(), edge_point.end(), new_edge_point_no_sample_points.begin());

                for (const auto &point: sampled_points) {
                    new_edge_point_no_sample_points.erase(
                            std::remove(new_edge_point_no_sample_points.begin(), new_edge_point_no_sample_points.end(),
                                        point),
                            new_edge_point_no_sample_points.end());
                }


                for (const auto& ele : clusters) {
                    std::vector<int> result;
                    std::sample(ele.begin(), ele.end(), std::back_inserter(result),1, g);
                    new_sample_points.emplace_back(sampled_points[result[0]]);
                }
                for (int j = clusters.size() + 1; j < 6; j++){
                    std::vector<cv::Point2f> result;
                    std::sample(sampled_points.begin(), sampled_points.end(), std::back_inserter(new_sample_points),1, g);
                }

                int num_valid_point = 0;
                cv::RotatedRect eval_ellipse;
                std::vector<cv::Point2f> evaluation_point;
                std::sample(new_edge_point_no_sample_points.begin(), new_edge_point_no_sample_points.end(),
                            std::back_inserter(evaluation_point), 1, g);
                new_sample_points.emplace_back(evaluation_point[0]);
                eval_ellipse = cv::fitEllipse(new_sample_points);

                for (const auto &point: sampled_points) {
                    float eval_dst_p2e;
                    eval_dst_p2e = distance_p2e(eval_ellipse, point);
                    if (eval_dst_p2e > 0.96 && eval_dst_p2e < 1.04) {
                        num_valid_point++;
                    }
                }


            // cv::Mat middle_output;
            // cv::cvtColor(edgeImage, middle_output, cv::COLOR_GRAY2BGR);
            // cv::ellipse(middle_output, eval_ellipse, cv::Scalar(0, 255, 0), 2);
            //
            // for (const auto& point : new_sample_points) {
            //     cv::circle(middle_output, point, 8, cv::Scalar(0, 0, 255), -1);
            // }
            //
            // cv::circle(middle_output, evaluation_point[0], 8, cv::Scalar(255, 0, 0), -1);
            // cv::imshow("Invalid output",middle_output);
            // cv::waitKey(0);

                if (num_valid_point >= static_cast<int>(sampled_points.size()) * 0.92 && a / b < 1.25 &&
                    (eval_ellipse.center.x > std::max(eval_ellipse.size.width/2, eval_ellipse.size.height/2)) &&
                    (eval_ellipse.center.y > std::max(eval_ellipse.size.width/2, eval_ellipse.size.height/2))) {
//                std::cout << a/b <<std::endl;
                    reason = 1;
                    return std::make_tuple(eval_ellipse, sampled_points, reason);
                }
            }
        }
        reason = 4;
        return std::make_tuple(cv::RotatedRect(), std::vector<cv::Point2f>(), reason);
    }




    bool Camera::evaluate_ellipse(cv::RotatedRect &ellipse, cv::Mat image, float threshold_inner, float threshold_outer){
        int num_inside = 0;
        int num_outside = 0;
        int total_point = 0;

        std::vector<cv::Point2f> mask_point;
        for (int y = 0; y < image.rows; ++y) {
            for (int x = 0; x < image.cols; ++x) {
                float dst_p2e = distance_p2e(ellipse, cv::Point(x, y));
                if (dst_p2e <= 1.0) {
                    total_point++;
                    if (image.at<uchar>(y, x) == 0){
                        num_inside++;
                    }
                }
                else {
                    if (image.at<uchar>(y, x) != 0){
                        num_outside++;
                    }
                }
            }
        }

        float inner_ratio = static_cast<float>(num_inside) / static_cast<float>(total_point);
        float outer_ration = static_cast<float>(num_outside) / static_cast<float>(total_point);

//    std::cout<<"**************"<<std::endl;
//    std::cout<<"Fill ration:  " <<inner_ratio<<std::endl;
//    std::cout<<"Outer ration: " <<static_cast<float>(num_outside) / static_cast<float>(total_point)<<std::endl;
//    std::cout<<std::endl;
        return inner_ratio <= threshold_inner && outer_ration <= threshold_outer;
    }

    cv::RotatedRect Camera::choose_optimal_ellipse(std::vector<cv::RotatedRect> ellipses){

        cv::RotatedRect optimal_ellipse = ellipses[0];
        float min_ratio = std::numeric_limits<float>::max();

        for (const auto& ellipse : ellipses) {
            float a = std::max(ellipse.size.height, ellipse.size.width) / 2.0;
            float b = std::min(ellipse.size.height, ellipse.size.width) / 2.0;
            float ratio = a / b;

            if (ratio < min_ratio) {
                min_ratio = ratio;
                optimal_ellipse = ellipse;
            }
        }

        return optimal_ellipse;
    }

    std::pair<cv::Point2f, float> Camera::find_real_center(cv::RotatedRect &ellipse, float &fx, cv::Point2f image_center) {
        cv::Point2f real_center;
        float error;

        float a = std::max(ellipse.size.height, ellipse.size.width) / 2.0;
        float b = std::min(ellipse.size.height, ellipse.size.width) / 2.0;
//    std::cout<<"a and b : "<<a<<", "<<b<<std::endl;
        cv::Point2f ellipse_center = ellipse.center;

        float m = (image_center.y - ellipse_center.y) / (image_center.x - ellipse_center.x);
        float c = ellipse_center.y - m * ellipse_center.x;


        float A = b * b + a * a * m * m;
        float B = 2 * a * a * m * (c - ellipse_center.y) - 2 * b * b * ellipse_center.x;
        float C = b * b * ellipse_center.x * ellipse_center.x + a * a * (c - ellipse_center.y) * (c - ellipse_center.y) -
                  a * a * b * b;

        // Solve the quadratic equation Ax^2 + Bx + C = 0
        float discriminant = B * B - 4 * A * C;
        if (discriminant < 0) {
            return std::make_pair(real_center, 100.0);
        }

        std::vector<cv::Point2f> intersections;

        // Calculate the two intersection points
        float x1 = (-B + std::sqrt(discriminant)) / (2 * A);
        float y1 = m * x1 + c;
        intersections.push_back(cv::Point2f(x1, y1));

        float x2 = (-B - std::sqrt(discriminant)) / (2 * A);
        float y2 = m * x2 + c;
        intersections.push_back(cv::Point2f(x2, y2));


        float distance1 = cv::norm(intersections[0] - image_center);
        float distance2 = cv::norm(intersections[1] - image_center);

        // Determine which point is closer to the image center
        cv::Point2f G, F;
        if (distance1 < distance2) {
            G = intersections[0];
            F = intersections[1];
        } else {
            G = intersections[1];
            F = intersections[0];
        }

        if (std::pow(((image_center.x - ellipse_center.x) / a), 2) + std::pow(((image_center.y - ellipse_center.y) / b), 2) > 1) {
//        std::cout<<"Image center is out of ellipse"<<std::endl;
            float PF = sqrt(std::pow((F.x - image_center.x), 2) + std::pow((F.y - image_center.y), 2));
            float PG = sqrt(std::pow((G.x - image_center.x), 2) + std::pow((G.y - image_center.y), 2));
            float PC = tan(0.5 * ((atan2(PF, fx)) + atan2(PG, fx))) * fx;
//        std::cout << PG << ", " << a << ", " << PC << ", " << PF << std::endl;
            error = PG + a - PC;
//        std::cout << "Eccentricity error :  " <<error << std::endl;
        }
        else{
//        std::cout<<"Image center is in the ellipse"<<std::endl;
            float PH = sqrt(std::pow((ellipse_center.x - image_center.x), 2) + std::pow((ellipse_center.y - image_center.y), 2));
            float PF = sqrt(std::pow((F.x - image_center.x), 2) + std::pow((F.y - image_center.y), 2));
            float PG = sqrt(std::pow((G.x - image_center.x), 2) + std::pow((G.y - image_center.y), 2));
            float PC = tan(0.5 * ((atan2(PF, fx)) - atan2(PG, fx))) * fx;
            error = PH - PC;
//        std::cout << PH << ", " << PC << std::endl;
//        std::cout << "Eccentricity error :  " <<error << std::endl;
        }

        if(error < 12.0) {
            if (ellipse_center.x < image_center.x && ellipse_center.y < image_center.y) {
                real_center.x = ellipse_center.x + sqrt((error * error) / (m * m + 1));
                real_center.y = ellipse_center.y + m * sqrt((error * error) / (m * m + 1));
            } else if (ellipse_center.x > image_center.x && ellipse_center.y < image_center.y) {
                real_center.x = ellipse_center.x - sqrt((error * error) / (m * m + 1));
                real_center.y = ellipse_center.y - m * sqrt((error * error) / (m * m + 1));
            } else if (ellipse_center.x < image_center.x && ellipse_center.y > image_center.y) {
                real_center.x = ellipse_center.x + sqrt((error * error) / (m * m + 1));
                real_center.y = ellipse_center.y + m * sqrt((error * error) / (m * m + 1));
            } else {
                real_center.x = ellipse_center.x - sqrt((error * error) / (m * m + 1));
                real_center.y = ellipse_center.y - m * sqrt((error * error) / (m * m + 1));
            }
//        std::cout << "Error :  "<<error<<std::endl;
//        std::cout << "Original center :  " << ellipse_center << std::endl;
//        std::cout << "Real center :     " << real_center << std::endl;
            return std::make_pair(real_center, error);
        }
        else {
            return std::make_pair(real_center, 100.0);
        }
    }

}; // namespace marscalib


int main(int argc, char** argv) {
    marscalib::Camera camera;
    camera.run(argc, argv);

    return 0;
}