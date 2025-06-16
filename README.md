<div align="center">
  <h1>MARSCalib</h1>
  <a href="https://github.com/sparolab/solid/tree/master/"><img src="https://img.shields.io/badge/-C++-blue?logo=cplusplus" /></a>
  <a href="https://github.com/sparolab/solid/tree/master"><img src="https://img.shields.io/badge/Python-3670A0?logo=python&logoColor=ffdd54" /></a>
  <a href="https://sparolab.github.io/research/marscalib/"><img src="https://github.com/sparolab/Joint_ID/blob/main/fig/badges/badge-website.svg" alt="Project" /></a>
  <a href="https://ieeexplore.ieee.org/abstract/document/10629042"><img src="https://img.shields.io/badge/Paper-PDF-yellow" alt="Paper" /></a>
  <a href="https://arxiv.org/abs/2408.07330"><img src="https://img.shields.io/badge/arXiv-2408.07330-b31b1b.svg?style=flat-square" alt="Arxiv" /></a>
  <a href="https://www.youtube.com/watch?v=4sAWWfZTwLs"><img src="https://badges.aleen42.com/src/youtube.svg" alt="YouTube" /></a>
  <br />
  <br />

**[IEEE IROS 25]** This repository is the official code for MARSCalib: Multi-robot, Automatic, Robust, Spherical Target-based Extrinsic Calibration in Field and Extraterrestrial Environments.

  <a href="https://scholar.google.com/citations?user=ZAO6skQAAAAJ&hl=ko" target="_blank">Seokhwan Jeong</a><sup></sup>,
  <a href="https://scholar.google.com/citations?user=t5UEbooAAAAJ&hl=ko" target="_blank">Hogyun Kim</a><sup></sup>,
  <a href="https://scholar.google.com/citations?user=W5MOKWIAAAAJ&hl=ko" target="_blank">Younggun Cho</a><sup>†</sup>

**[Spatial AI and Robotics Lab (SPARO)](https://sparolab.github.io/)**

  <p align="center"><img src="fig/main.png") alt="animated" width="75%" /></p>
  
</div>

<br/>
<br/>

<a name="readme-table"></a>

## 🚀 Contents Table
1. 🛠️ [**Prerequisites**](https://github.com/sparolab/MARSCalib?tab=readme-ov-file#%EF%B8%8F-prerequisites)
2. 📷 [**Data Acquisition**](https://github.com/sparolab/MARSCalib?tab=readme-ov-file#-data-acquisition)
3. ✏️ [**Introduction**](https://github.com/sparolab/MARSCalib?tab=readme-ov-file#%EF%B8%8F-introduction)
4. ✉️ [**Contact**](https://github.com/sparolab/MARSCalib?tab=readme-ov-file#%EF%B8%8F-contact)

<br/>
<br/>


## 🛠️ Prerequisites
* [**ROS2**](https://docs.ros.org/en/foxy/index.html)

* PCL

* OpenCV

* json
  ```
  sudo apt-get install nlohmann-json3-dev
  ```

* [**GTSAM**](https://github.com/borglab/gtsam)

* [**Ceres**](http://ceres-solver.org/installation.html)

* [**SAM**](https://github.com/facebookresearch/segment-anything)
   ```
   pip install git+https://github.com/facebookresearch/segment-anything.git
   ```

* Sample dataset
    * https://drive.google.com/drive/u/2/folders/1cf9hkyxft-V8sNcHXUFzVnjNg9rRtZ26
    * There are three dataset utilizing three different LiDARs: OS1-32, Mid-360, MLX-120.

<p align="right">(<a href="#readme-table">back to table</a>)</p>

<br/>
<br/>


## 📷 Data Acquisition
* The camera's intrinsic parameters must be known in advance.

* Two topics are required in the bag: one for image(sensor_msgs/Image), and one for point clouds(sensor_msgs/PointCloud2).

* Place the spherical target within about 30 cm, ensuring it is visible to both sensors. (If placed too far, the sphere may not be captured for LiDAR)

* Remain the two sensors(camera and LiDAR) and the spherical target __❗stationary❗__ while data is collected for more than 40 seconds.

* After acquiring the data, arrange the bags as follows (recommended: at least 10 samples).
    ```
    📂 Dataset
    ┣  1.db3 (file name is not necessary)
    ┣  2.db3
    ┣  3.db3
    ┣  4.db3
        ...
    ```

<p align="right">(<a href="#readme-table">back to table</a>)</p>

<br/>
<br/>


## ✏️ Introduction

**0. Create workspace & download SAM model weight**
  * Extract image and accumlated point cloud from ros2 bag.
```
    cd ~/ros2_ws/src
```

```
    git clone https://github.com/sparolab/MARSCalib
``` 

```
    cd .. && colcon build && source install/setup.bash
```
  * Download SAM Model Weight
    * Default Vit-h model: https://dl.fbaipublicfiles.com/segment_anything/sam_vit_h_4b8939.pth
    * If you want to use other model, please refer to the relevant page : https://github.com/facebookresearch/segment-anything

  * Place downloaded model(pth) file in ``` ./marscalib/segment_anything/model ```.

<br/>

**1. Preprocess**
  * Extract image and accumlated point cloud from ros2 bag.
  * The output folder will be generated.
  * To automatically detect image and point cloud topics:
```
    ros2 run marscalib preprocess <dataset location> -a 
```
  * To manually specify topics when multiple are present:
```
    ros2 run marscalib preprocess <dataset location> \
      --image_topic <image_topic> \
      --points_topic <points_topic>
```
  * Example:
```
    ros2 run marscalib preprocess ~/data/sphere \
      --image_topic /camera/color/image_raw \
      --points_topic /ouster/points
```
  * ❗Fill intrinsic parameters of camera in file ``` ./input_preprocess/intrinsic.json ```.
<br/>


**2. SAM**
  * Extract mask images from the raw image.
```
    ros2 run marscalib amg.py \
      --checkpoint <model checkpoint location> \
      --model-type <model type> \
      --input <preprocess folder location>
```
  * Example:
```
    ros2 run marscalib amg.py \
      --checkpoint ~/ros2_ws/src/marscalib/segment_anything/model/sam_vit_h_4b8939.pth \
      --model-type vit_h \
      --input ~/data/sphere_preprocess
```
<br/>

**3. Camera ellipse center detection**
  * Detect image with ellipses from the mask image, then extract center of the ellipse.
```
    ros2 run marscalib camera <preprocess folder location>
```
<br/>

**4. Range image generation & Hough transform**
  * Generate range image from the accumulated pointcloud and search for circle in the range image. The points inside the detected circle consist sphere.
  * Enter LiDAR type. 
    * o : ouster
    * m : mid360
    * s : mlx
```
    ros2 run hough.py <preprocess folder location> <LiDAR type>
```

  * Example:
```
    ros2 run hough.py ~/data/sphere_preprocess o
```

<br/>


**5. LiDAR sphere center detection**
  * Detect center of the sphere.
  * Enter LiDAR type.
    * o : ouster
    * m : mid360
    * s : mlx
  * Enter target's radius.
  * If you want to observet the output of every stage, add "-v" in the command line.
```
    ros2 run marscalib ouster <preprocess folder location> <LiDAR type> <target's radius>
```

  * Example
```
    ros2 run marscalib ouster ~/data/sphere_preprocess o 0.1 -v
```
<br/>

**6. [R|t] calculation**
  * Calculate transformation matrix with 2D-3D center pair.
```
    ros2 run marscalib rt  <preprocess folder location>
```

<p align="right">(<a href="#readme-table">back to table</a>)</p>

<br/>
<br/>


##  ✉️ Contact
* **Seokhwan Jeong     eric5709@inha.edu**
* **Hogyun Kim         hg.kim@inha.edu**

<p align="right">(<a href="#readme-table">back to table</a>)</p>
