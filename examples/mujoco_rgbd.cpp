#include <torch/torch.h>

#include <iostream>
#include <algorithm>
#include <fstream>
#include <chrono>
#include <ctime>
#include <sstream>
#include <thread>
#include <filesystem>
#include <memory>

#include <opencv2/core/core.hpp>

#include "ORB-SLAM3/include/System.h"

#include "include/gaussian_mapper.h"
#include "viewer/imgui_viewer.h"

#include <zmq.hpp>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>

std::queue<std::pair<cv::Mat, cv::Mat>> imageBuffer;
std::mutex bufferMutex;
std::condition_variable bufferCondVar;

const size_t MAX_BUFFER_SIZE = 10; 

void saveTrackingTime(std::vector<float> &vTimesTrack, const std::string &strSavePath);
void saveGpuPeakMemoryUsage(std::filesystem::path pathSave);


void socketReceiver(zmq::context_t& context, zmq::socket_t& socket) {
    while (true) {
        zmq::message_t rgb_msg, depth_msg;
        socket.recv(rgb_msg, zmq::recv_flags::none);
        socket.recv(depth_msg, zmq::recv_flags::none);

        // Convert RGB data to vector
        std::vector<uchar> rgb_data(static_cast<uchar*>(rgb_msg.data()), 
                                    static_cast<uchar*>(rgb_msg.data()) + rgb_msg.size());
        cv::Mat imRGB = cv::imdecode(rgb_data, cv::IMREAD_COLOR);

        // Convert Depth data to vector
        std::vector<uchar> depth_data(static_cast<uchar*>(depth_msg.data()), 
                                      static_cast<uchar*>(depth_msg.data()) + depth_msg.size());
        cv::Mat imD = cv::imdecode(depth_data, cv::IMREAD_UNCHANGED); // Ensure 16-bit depth

        if (imRGB.empty() || imD.empty()) {
            std::cerr << "Error: Image decoding failed!\n";
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(bufferMutex);
            if (imageBuffer.size() >= MAX_BUFFER_SIZE) {
                // Discard the oldest frame if the buffer is too long
                imageBuffer.pop();
            }
            imageBuffer.push(std::make_pair(imRGB, imD));
        }
        bufferCondVar.notify_one();
    }
}

int main(int argc, char **argv)
{
    if (argc != 5)
    {
        std::cerr << std::endl
                  << "Usage: " << argv[0]
                  << " path_to_vocabulary"                   /*1*/
                  << " path_to_ORB_SLAM3_settings"           /*2*/
                  << " path_to_gaussian_mapping_settings"    /*3*/
                  << " path_to_trajectory_output_directory/" /*4*/
                  << std::endl;
        return 1;
    }
    bool use_viewer = true;

    std::string output_directory = std::string(argv[4]);
    if (output_directory.back() != '/')
        output_directory += "/";
    std::filesystem::path output_dir(output_directory);

    // Device
    torch::DeviceType device_type;
    if (torch::cuda::is_available())
    {
        std::cout << "CUDA available! Training on GPU." << std::endl;
        device_type = torch::kCUDA;
    }
    else
    {
        std::cout << "Training on CPU." << std::endl;
        device_type = torch::kCPU;
    }

    // Create SLAM system. It initializes all system threads and gets ready to process frames.
    std::shared_ptr<ORB_SLAM3::System> pSLAM =
        std::make_shared<ORB_SLAM3::System>(
            argv[1], argv[2], ORB_SLAM3::System::RGBD);
    float imageScale = pSLAM->GetImageScale();

    // Create GaussianMapper
    std::filesystem::path gaussian_cfg_path(argv[3]);
    std::shared_ptr<GaussianMapper> pGausMapper =
        std::make_shared<GaussianMapper>(
            pSLAM, gaussian_cfg_path, output_dir, 0, device_type);
    std::thread training_thd(&GaussianMapper::run, pGausMapper.get());

    // Create Gaussian Viewer
    std::thread viewer_thd;
    std::shared_ptr<ImGuiViewer> pViewer;
    if (use_viewer)
    {
        pViewer = std::make_shared<ImGuiViewer>(pSLAM, pGausMapper);
        viewer_thd = std::thread(&ImGuiViewer::run, pViewer.get());
    }

    // Vector for tracking time statistics
    std::vector<float> vTimesTrack;
    vTimesTrack.reserve(3000000);

    std::cout << std::endl << "-------" << std::endl;
    std::cout << "Start processing sequence ..." << std::endl;

    // Set up ZeroMQ context and socket
    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_SUB);
    socket.connect("tcp://localhost:5555");
    socket.setsockopt(ZMQ_SUBSCRIBE, "", 0);

    // Start socket receiver thread
    std::thread socket_thread(socketReceiver, std::ref(context), std::ref(socket));

    // Main loop
    cv::Mat imRGB, imD;
    int ni = 0;
    while (true)
    {
        if (pSLAM->isShutDown())
            break;

        std::unique_lock<std::mutex> lock(bufferMutex);
        bufferCondVar.wait(lock, []{ return !imageBuffer.empty(); });

        auto images = imageBuffer.front();
        imageBuffer.pop();
        lock.unlock();

        imRGB = images.first;
        imD = images.second;

        if (imageScale != 1.f)
        {
            int width = imRGB.cols * imageScale;
            int height = imRGB.rows * imageScale;
            cv::resize(imRGB, imRGB, cv::Size(width, height));
            cv::resize(imD, imD, cv::Size(width, height));
        }

        double tframe = ni++;

        std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();

        // Pass the image to the SLAM system
        pSLAM->TrackRGBD(imRGB, imD, tframe, std::vector<ORB_SLAM3::IMU::Point>(), "");

        std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();

        double ttrack = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1).count();
        vTimesTrack.push_back(ttrack);
    }

    // Stop all threads
    pSLAM->Shutdown();
    training_thd.join();
    if (use_viewer)
        viewer_thd.join();
    socket_thread.detach();

    // GPU peak usage
    saveGpuPeakMemoryUsage(output_dir / "GpuPeakUsageMB.txt");

    // Tracking time statistics
    saveTrackingTime(vTimesTrack, (output_dir / "TrackingTime.txt").string());

    // Save camera trajectory
    pSLAM->SaveTrajectoryTUM((output_dir / "CameraTrajectory_TUM.txt").string());
    pSLAM->SaveKeyFrameTrajectoryTUM((output_dir / "KeyFrameTrajectory_TUM.txt").string());
    pSLAM->SaveTrajectoryEuRoC((output_dir / "CameraTrajectory_EuRoC.txt").string());
    pSLAM->SaveKeyFrameTrajectoryEuRoC((output_dir / "KeyFrameTrajectory_EuRoC.txt").string());
    pSLAM->SaveTrajectoryKITTI((output_dir / "CameraTrajectory_KITTI.txt").string());

    socket.close();
    context.close();
    cv::destroyAllWindows();

    return 0;
}

void saveTrackingTime(std::vector<float> &vTimesTrack, const std::string &strSavePath)
{
    std::ofstream out;
    out.open(strSavePath.c_str());
    std::size_t nImages = vTimesTrack.size();
    float totaltime = 0;
    for (int ni = 0; ni < nImages; ni++)
    {
        out << std::fixed << std::setprecision(4)
            << vTimesTrack[ni] << std::endl;
        totaltime += vTimesTrack[ni];
    }

    // std::sort(vTimesTrack.begin(), vTimesTrack.end());
    // out << "-------" << std::endl;
    // out << std::fixed << std::setprecision(4)
    //     << "median tracking time: " << vTimesTrack[nImages / 2] << std::endl;
    // out << std::fixed << std::setprecision(4)
    //     << "mean tracking time: " << totaltime / nImages << std::endl;

    out.close();
}

void saveGpuPeakMemoryUsage(std::filesystem::path pathSave)
{
    namespace c10Alloc = c10::cuda::CUDACachingAllocator;
    c10Alloc::DeviceStats mem_stats = c10Alloc::getDeviceStats(0);

    c10Alloc::Stat reserved_bytes = mem_stats.reserved_bytes[static_cast<int>(c10Alloc::StatType::AGGREGATE)];
    float max_reserved_MB = reserved_bytes.peak / (1024.0 * 1024.0);

    c10Alloc::Stat alloc_bytes = mem_stats.allocated_bytes[static_cast<int>(c10Alloc::StatType::AGGREGATE)];
    float max_alloc_MB = alloc_bytes.peak / (1024.0 * 1024.0);

    std::ofstream out(pathSave);
    out << "Peak reserved (MB): " << max_reserved_MB << std::endl;
    out << "Peak allocated (MB): " << max_alloc_MB << std::endl;
    out.close();
}