#!/bin/bash

../bin/mujoco_rgbd \
    ../ORB-SLAM3/Vocabulary/ORBvoc.txt \
    ../cfg/ORB_SLAM3/RGB-D/Simulation/cam_d435i.yaml \
    ../cfg/gaussian_mapper/RGB-D/RealCamera/realsense_rgbd.yaml \
    ../results/simulation_cam_d435i