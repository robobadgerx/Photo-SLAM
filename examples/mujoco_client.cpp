#include <zmq.hpp>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <vector>

int main() {
    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_SUB);
    socket.connect("tcp://localhost:5555");
    socket.setsockopt(ZMQ_SUBSCRIBE, "", 0);


    while (true) {
        zmq::message_t rgb_msg, depth_msg;
        socket.recv(rgb_msg, zmq::recv_flags::none);
        socket.recv(depth_msg, zmq::recv_flags::none);

        // Convert RGB data to vector
        std::vector<uchar> rgb_data(static_cast<uchar*>(rgb_msg.data()), 
                                    static_cast<uchar*>(rgb_msg.data()) + rgb_msg.size());
        cv::Mat rgb = cv::imdecode(rgb_data, cv::IMREAD_COLOR);

        // Convert Depth data to vector
        std::vector<uchar> depth_data(static_cast<uchar*>(depth_msg.data()), 
                                      static_cast<uchar*>(depth_msg.data()) + depth_msg.size());
        cv::Mat depth = cv::imdecode(depth_data, cv::IMREAD_UNCHANGED); // Ensure 16-bit depth

        if (rgb.empty() || depth.empty()) {
            std::cerr << "Error: Image decoding failed!\n";
            continue;
        }
        std::ofstream depthFile("depth_data_client.csv");
        if (!depthFile.is_open()) {
            std::cerr << "Error: Could not open depth_data_client.csv for writing!\n";
            return 1;
        }
    
        // Write depth data to CSV file
        for (int i = 0; i < depth.rows; ++i) {
            for (int j = 0; j < depth.cols; ++j) {
                depthFile << depth.at<uint16_t>(i, j);
                if (j < depth.cols - 1) {
                    depthFile << ",";
                }
            }
            depthFile << "\n";
        }
        
        depthFile.close();

        // Display images
        cv::imshow("RGB", rgb);
        cv::imshow("Depth", depth);  // Normalize for visualization
        if (cv::waitKey(1) == 27)
            break;  // ESC to exit
    }

    
    socket.close();
    context.close();
    cv::destroyAllWindows();

    return 0;
}