docker build . -t photo-slam:init

docker run --privileged \
           --gpus all \
           --runtime=nvidia \
           --net host \
            -e DISPLAY=$DISPLAY \
            -e NVIDIA_DRIVER_CAPABILITIES=all \
            -p 80:80 \
            -p 443:443 \
            -v /tmp/.X11-unix:/tmp/.X11-unix \
            -it \
            --shm-size=16gb \
            --name photo-slam_env \
            photo-slam_img:init
