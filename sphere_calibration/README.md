# -private-MARSCalib

## Prerequisites
* Download SAM Model Weight
   * Vit-h model download : https://dl.fbaipublicfiles.com/segment_anything/sam_vit_h_4b8939.pth
   * If you want to use other model, please refer to the relevant page : https://github.com/facebookresearch/segment-anything
   
* ROS2 environment

* Sample dataset
    * https://drive.google.com/drive/u/2/folders/1cf9hkyxft-V8sNcHXUFzVnjNg9rRtZ26
    * There are three dataset utilizing three different LiDAR, Ouster, Mid360, MLX-120(sos).
    
## Introduction

**0. Create workspace & and place model in model folder**
  * Extract image and accumlated point cloud from ros2 bag.
```
    mkdir ~/marscalib_ws/src
```

```
    git clone -b master --single-branch https://github.com/sparolab/-private-MARSCalib
``` 

```
    cd .. && colcon build && source install/setup.bash
```

  * Place downloaded model(pth) file in ``` <sphere_calibration/model> ```.


**1. Preprocess**
  * Extract image and accumlated point cloud from ros2 bag.
  * The output folder will be generated.

```
    ros2 run sphere_calibration preprocess <dataset location>
```


**2. SAM**
  * Extract mask images from the raw image.
```
    python ~/marscalib_ws/src/sphere_calibration/scripts/amg.py --checkpoint <model checkpoint location> --model-type <model type> --input <preprocess folder location>
```
  * Example
```
    python ~/sphere_ws/src/sphere_calibration/scripts/amg.py --checkpoint ~/marscalib_ws/src/sphere_calibration/segment_anything/model/sam_vit_h_4b8939.pth --model-type vit_h --input ~/sphere_calib/ouster_preprocess
```

**3. Camera ellipse center detection**
  * Detect image with ellipses from the mask image, then extract center of the ellipse.
```
    ros2 run sphere_calibration camera <preprocess folder location>
```

**4. Range image & Hough transform**
  * Generate range image from the accumulated pointcloud and search for circle in the range image. Then detect the points that are inside the detected circle.
  * Enter LiDAR type. 
    * o : ouster
    * m : mid360
    * s : mlx
```
    python ~/marscalib_ws/src/sphere_calibration/scripts/hough.py <preprocess folder location> <LiDAR type>
```

  * Example
```
    python ~/marscalib_ws/src/sphere_calibration/scripts/hough.py ~/sphere_calib/ouster_preprocess/ o
```



**5. LiDAR sphere center detection**
  * Detect center of the sphere.
  * Enter LiDAR type.
    * o : ouster
    * m : mid360
    * s : mlx
  * Enter target's radius.
  * If you want to observet the output of every stage, add "-v" in the command line.

```
    ros2 run sphere_calibration ouster <preprocess folder location> <LiDAR type> <target's radius>
```

  * Example
```
    ros2 run sphere_calibration ouster ~/sphere_calib/ouster_preprocess/ o 0.1 -v
```

**6. [R|t] calculation**
  * Calculate transformation matrix with 2D-3D center pair.
```
    ros2 run sphere_calibration rt  <preprocess folder location>
```
