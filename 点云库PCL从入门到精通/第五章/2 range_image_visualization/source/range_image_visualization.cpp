﻿#include <pcl/common/common_headers.h>
#include <pcl/console/parse.h>
#include <pcl/io/pcd_io.h>
#include <pcl/range_image/range_image.h>
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/visualization/range_image_visualizer.h>
#include <boost/thread/thread.hpp>
#include <iostream>

typedef pcl::PointXYZ PointType;
// 全局参数
float angular_resolution = 0.5f;
pcl::RangeImage::CoordinateFrame coordinate_frame =
    pcl::RangeImage::CAMERA_FRAME;
bool live_update = false;
// -----打印帮助-----
void printUsage(const char* progName) {
  std::cout << "\n\nUsage: " << progName << " [options] <scene.pcd>\n\n"
            << "Options:\n"
            << "-------------------------------------------\n"
            << "-r <float>   angular resolution in degrees (default "
            << angular_resolution << ")\n"
            << "-c <int>     coordinate frame (default "
            << (int)coordinate_frame << ")\n"
            << "-l           live update - update the range image according to "
               "the selected view in the 3D viewer.\n"
            << "-h           this help\n"
            << "\n\n";
}

void setViewerPose(pcl::visualization::PCLVisualizer& viewer,
                   const Eigen::Affine3f& viewer_pose) {
  Eigen::Vector3f pos_vector = viewer_pose * Eigen::Vector3f(0, 0, 0);
  Eigen::Vector3f look_at_vector =
      viewer_pose.rotation() * Eigen::Vector3f(0, 0, 1) + pos_vector;
  Eigen::Vector3f up_vector =
      viewer_pose.rotation() * Eigen::Vector3f(0, -1, 0);
#if 0
  viewer.camera_.pos[0] = pos_vector[0];
  viewer.camera_.pos[1] = pos_vector[1];
  viewer.camera_.pos[2] = pos_vector[2];
  viewer.camera_.focal[0] = look_at_vector[0];
  viewer.camera_.focal[1] = look_at_vector[1];
  viewer.camera_.focal[2] = look_at_vector[2];
  viewer.camera_.view[0] = up_vector[0];
  viewer.camera_.view[1] = up_vector[1];
  viewer.camera_.view[2] = up_vector[2];
#endif
  viewer.setCameraPosition(pos_vector[0], pos_vector[1], pos_vector[2],
                           look_at_vector[0], look_at_vector[1],
                           look_at_vector[2], up_vector[0], up_vector[1],
                           up_vector[2]);
  viewer.updateCamera();
}

// -----Main-----
int main(int argc, char** argv) {
  //解析命令行参数
  if (pcl::console::find_argument(argc, argv, "-h") >= 0) {
    printUsage(argv[0]);
    return 0;
  }
  if (pcl::console::find_argument(argc, argv, "-l") >= 0) {
    live_update = true;
    std::cout << "Live update is on.\n";
  }
  if (pcl::console::parse(argc, argv, "-r", angular_resolution) >= 0)
    std::cout << "Setting angular resolution to " << angular_resolution
              << "deg.\n";
  int tmp_coordinate_frame;
  if (pcl::console::parse(argc, argv, "-c", tmp_coordinate_frame) >= 0) {
    coordinate_frame = pcl::RangeImage::CoordinateFrame(tmp_coordinate_frame);
    std::cout << "Using coordinate frame " << (int)coordinate_frame << ".\n";
  }
  angular_resolution = pcl::deg2rad(angular_resolution);

  // 读取给定的pcd点云文件或者自行创建随机点云
  pcl::PointCloud<PointType>::Ptr point_cloud_ptr(
      new pcl::PointCloud<PointType>);
  pcl::PointCloud<PointType>& point_cloud = *point_cloud_ptr;
  Eigen::Affine3f scene_sensor_pose(Eigen::Affine3f::Identity());
  std::vector<int> pcd_filename_indices =
      pcl::console::parse_file_extension_argument(argc, argv, "pcd");
  if (!pcd_filename_indices.empty()) {
    std::string filename = argv[pcd_filename_indices[0]];
    if (pcl::io::loadPCDFile(filename, point_cloud) == -1) {
      std::cout << "Was not able to open file \"" << filename << "\".\n";
      printUsage(argv[0]);
      return 0;
    }
    scene_sensor_pose =
        Eigen::Affine3f(Eigen::Translation3f(point_cloud.sensor_origin_[0],
                                             point_cloud.sensor_origin_[1],
                                             point_cloud.sensor_origin_[2])) *
        Eigen::Affine3f(point_cloud.sensor_orientation_);
  } else {
    std::cout << "\nNo *.pcd file given => Genarating example point cloud.\n\n";
    for (float x = -0.5f; x <= 0.5f; x += 0.01f) {
      for (float y = -0.5f; y <= 0.5f; y += 0.01f) {
        PointType point;
        point.x = x;
        point.y = y;
        point.z = 2.0f - y;
        point_cloud.points.push_back(point);
      }
    }
    point_cloud.width = (int)point_cloud.points.size();
    point_cloud.height = 1;
  }
  //从点云创建深度图像对象
  float noise_level = 0.0;
  float min_range = 0.0f;
  int border_size = 1;
  boost::shared_ptr<pcl::RangeImage> range_image_ptr(new pcl::RangeImage);
  pcl::RangeImage& range_image = *range_image_ptr;
  range_image.createFromPointCloud(point_cloud, angular_resolution,
                                   pcl::deg2rad(360.0f), pcl::deg2rad(180.0f),
                                   scene_sensor_pose, coordinate_frame,
                                   noise_level, min_range, border_size);
  //创建3D视图并且添加点云进行显示
  pcl::visualization::PCLVisualizer viewer("3D Viewer");
  viewer.setBackgroundColor(1, 1, 1);
  pcl::visualization::PointCloudColorHandlerCustom<pcl::PointWithRange>
      range_image_color_handler(range_image_ptr, 0, 0, 0);
  viewer.addPointCloud(range_image_ptr, range_image_color_handler,
                       "range image");
  viewer.setPointCloudRenderingProperties(
      pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "range image");
  // viewer.addCoordinateSystem (1.0f);
  // PointCloudColorHandlerCustom<PointType> point_cloud_color_handler
  // (point_cloud_ptr, 150, 150, 150); viewer.addPointCloud (point_cloud_ptr,
  // point_cloud_color_handler, "original point cloud");
  viewer.initCameraParameters();
  setViewerPose(viewer, range_image.getTransformationToWorldSystem());
  //显示深度图像
  pcl::visualization::RangeImageVisualizer range_image_widget("Range image");
  range_image_widget.showRangeImage(range_image);
  //主循环
  while (!viewer.wasStopped()) {
    range_image_widget.spinOnce();
    viewer.spinOnce();
    pcl_sleep(0.01);
    if (live_update) {
      scene_sensor_pose = viewer.getViewerPose();
      range_image.createFromPointCloud(
          point_cloud, angular_resolution, pcl::deg2rad(360.0f),
          pcl::deg2rad(180.0f), scene_sensor_pose, pcl::RangeImage::LASER_FRAME,
          noise_level, min_range, border_size);
      range_image_widget.showRangeImage(range_image);
    }
  }
}
