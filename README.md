<div align="center">
  <h1>MARSCalib</h1>
  <!-- <a href="https://github.com/sparolab/solid/tree/master/"><img src="https://img.shields.io/badge/-C++-blue?logo=cplusplus" /></a>
  <a href="https://github.com/sparolab/solid/tree/master"><img src="https://img.shields.io/badge/Python-3670A0?logo=python&logoColor=ffdd54" /></a>
  <a href="https://sparolab.github.io/research/marscalib/"><img src="https://github.com/sparolab/Joint_ID/blob/main/fig/badges/badge-website.svg" alt="Project" /></a>
  <a href="https://ieeexplore.ieee.org/abstract/document/10629042"><img src="https://img.shields.io/badge/Paper-PDF-yellow" alt="Paper" /></a>
  <a href="https://arxiv.org/abs/2408.07330"><img src="https://img.shields.io/badge/arXiv-2408.07330-b31b1b.svg?style=flat-square" alt="Arxiv" /></a>
  <a href="https://www.youtube.com/watch?v=4sAWWfZTwLs"><img src="https://badges.aleen42.com/src/youtube.svg" alt="YouTube" /></a> -->
  <br />
  <br />

**[IEEE IROS 25]** This repository is the official code for MARSCalib: Multi-robot, Automatic, Robust, Spherical Target-based Extrinsic Calibration in Field and Extraterrestrial Environments.

  <a href="https://scholar.google.com/citations?user=ZAO6skQAAAAJ&hl=ko" target="_blank">Seokhwan Jeong</a><sup></sup>,
  <a href="https://scholar.google.com/citations?user=t5UEbooAAAAJ&hl=ko" target="_blank">Hogyun Kim</a><sup></sup>,
  <a href="https://scholar.google.com/citations?user=W5MOKWIAAAAJ&hl=ko" target="_blank">Younggun Cho</a><sup>†</sup>

**[Spatial AI and Robotics Lab (SPARO)](https://sparolab.github.io/)**

  <p align="center"><img src="fig/main.png") alt="animated" width="75%" /></p>
  
</div>

## 🚀 Contents Table
### 🛠️ [**Prerequisites**](https://github.com/sparolab/MARSCalib?tab=readme-ov-file#-prerequisites)
### 📷 [**Data Acquisition**](https://github.com/sparolab/MARSCalib?tab=readme-ov-file#-data-acquisition)
### ✏️ [**Introduction**](https://github.com/sparolab/MARSCalib?tab=readme-ov-file#-introduction)
### ✉️ [**Contact**](https://github.com/sparolab/MARSCalib?tab=readme-ov-file#-contact)


## 🛠️ Prerequisites
* __ROS2__ environment

* PCL

* OpenCV

* GTSAM

* Ceres

* SAM
   
* Sample dataset
    * https://drive.google.com/drive/u/2/folders/1cf9hkyxft-V8sNcHXUFzVnjNg9rRtZ26
    * There are three dataset utilizing three different LiDARs: OS1-32, Mid-360, MLX-120.
    

## 📷 Data Acquisition
* The camera's intrinsic parameters must be known in advance.

* Remain the two sensors(camera and LiDAR) and the spherical target stationary while data is collected for more than 40 seconds.

* Place the spherical target within about 30 cm, ensuring it is visible to both sensors. (If placed too far, the sphere may not be captured for LiDAR)




## ✏️ Introduction

**0. Create workspace & place model in model folder**
  * Extract image and accumlated point cloud from ros2 bag.
```
    mkdir ~/sphere_calib/src
```

```
    git clone -b master --single-branch https://github.com/sparolab/-private-MARSCalib
``` 

```
    cd .. && colcon build && source install/setup.bash
```
  * Download SAM Model Weight
    * Default Vit-h model: https://dl.fbaipublicfiles.com/segment_anything/sam_vit_h_4b8939.pth
    * If you want to use other model, please refer to the relevant page : https://github.com/facebookresearch/segment-anything

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


##  ✉️ Contact
* **Seokhwan Jeong     eric5709@inha.edu**
* **Hogyun Kim         hg.kim@inha.edu**

