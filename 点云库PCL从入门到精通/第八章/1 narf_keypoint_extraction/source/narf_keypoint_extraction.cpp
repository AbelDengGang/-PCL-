﻿/* \author Bastian Steder */

#include <pcl/console/parse.h>
#include <pcl/features/range_image_border_extractor.h>
#include <pcl/io/pcd_io.h>
#include <pcl/keypoints/narf_keypoint.h>
#include <pcl/range_image/range_image.h>
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/visualization/range_image_visualizer.h>
#include <boost/thread/thread.hpp>
#include <iostream>

typedef pcl::PointXYZ PointType;
//参数
float angular_resolution = 0.5f;
float support_size = 0.2f;
pcl::RangeImage::CoordinateFrame coordinate_frame =
    pcl::RangeImage::CAMERA_FRAME;
bool setUnseenToMaxRange = false;
//打印帮助
void printUsage(const char* progName) {
  std::cout
      << "\n\nUsage: " << progName << " [options] <scene.pcd>\n\n"
      << "Options:\n"
      << "-------------------------------------------\n"
      << "-r <float>   angular resolution in degrees (default "
      << angular_resolution << ")\n"
      << "-c <int>     coordinate frame (default " << (int)coordinate_frame
      << ")\n"
      << "-m           Treat all unseen points as maximum range readings\n"
      << "-s <float>   support size for the interest points (diameter of the "
         "used sphere - "
      << "default " << support_size << ")\n"
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
  if (pcl::console::find_argument(argc, argv, "-m") >= 0) {
    setUnseenToMaxRange = true;
    cout << "Setting unseen values in range image to maximum range readings.\n";
  }
  int tmp_coordinate_frame;
  if (pcl::console::parse(argc, argv, "-c", tmp_coordinate_frame) >= 0) {
    coordinate_frame = pcl::RangeImage::CoordinateFrame(tmp_coordinate_frame);
    cout << "Using coordinate frame " << (int)coordinate_frame << ".\n";
  }
  if (pcl::console::parse(argc, argv, "-s", support_size) >= 0)
    cout << "Setting support size to " << support_size << ".\n";
  if (pcl::console::parse(argc, argv, "-r", angular_resolution) >= 0)
    cout << "Setting angular resolution to " << angular_resolution << "deg.\n";
  angular_resolution = pcl::deg2rad(angular_resolution);
  //读取给定的pcd文件或者自行创建随机点云
  pcl::PointCloud<PointType>::Ptr point_cloud_ptr(
      new pcl::PointCloud<PointType>);
  pcl::PointCloud<PointType>& point_cloud = *point_cloud_ptr;
  pcl::PointCloud<pcl::PointWithViewpoint> far_ranges;
  Eigen::Affine3f scene_sensor_pose(Eigen::Affine3f::Identity());
  std::vector<int> pcd_filename_indices =
      pcl::console::parse_file_extension_argument(argc, argv, "pcd");
  if (!pcd_filename_indices.empty()) {
    std::string filename = argv[pcd_filename_indices[0]];
    if (pcl::io::loadPCDFile(filename, point_cloud) == -1) {
      cerr << "Was not able to open file \"" << filename << "\".\n";
      printUsage(argv[0]);
      return 0;
    }
    scene_sensor_pose =
        Eigen::Affine3f(Eigen::Translation3f(point_cloud.sensor_origin_[0],
                                             point_cloud.sensor_origin_[1],
                                             point_cloud.sensor_origin_[2])) *
        Eigen::Affine3f(point_cloud.sensor_orientation_);
    std::string far_ranges_filename =
        pcl::getFilenameWithoutExtension(filename) + "_far_ranges.pcd";
    if (pcl::io::loadPCDFile(far_ranges_filename.c_str(), far_ranges) == -1)
      std::cout << "Far ranges file \"" << far_ranges_filename
                << "\" does not exists.\n";
  } else {
    setUnseenToMaxRange = true;
    cout << "\nNo *.pcd file given =>Genarating example point cloud.\n\n";
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
  //从点云创建距离图像
  float noise_level = 0.0;
  float min_range = 0.0f;
  int border_size = 1;
  boost::shared_ptr<pcl::RangeImage> range_image_ptr(new pcl::RangeImage);
  pcl::RangeImage& range_image = *range_image_ptr;
  range_image.createFromPointCloud(point_cloud, angular_resolution,
                                   pcl::deg2rad(360.0f), pcl::deg2rad(180.0f),
                                   scene_sensor_pose, coordinate_frame,
                                   noise_level, min_range, border_size);
  range_image.integrateFarRanges(far_ranges);
  if (setUnseenToMaxRange) range_image.setUnseenToMaxRange();
  // 创建3D点云可视化窗口，并显示点云
  pcl::visualization::PCLVisualizer viewer("3D Viewer");
  viewer.setBackgroundColor(1, 1, 1);
  pcl::visualization::PointCloudColorHandlerCustom<pcl::PointWithRange>
      range_image_color_handler(range_image_ptr, 0, 0, 0);
  viewer.addPointCloud(range_image_ptr, range_image_color_handler,
                       "range image");
  viewer.setPointCloudRenderingProperties(
      pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "range image");
  // viewer.addCoordinateSystem (1.0f);
  // PointCloudColorHandlerCustom<PointType>point_cloud_color_handler
  // (point_cloud_ptr, 150, 150, 150); viewer.addPointCloud (point_cloud_ptr,
  // point_cloud_color_handler, "original point cloud");
  viewer.initCameraParameters();
  setViewerPose(viewer, range_image.getTransformationToWorldSystem());
  // 显示距离图像
  pcl::visualization::RangeImageVisualizer range_image_widget("Range image");
  range_image_widget.showRangeImage(range_image);

  //提取NARF关键点
  pcl::RangeImageBorderExtractor range_image_border_extractor;
  pcl::NarfKeypoint narf_keypoint_detector(&range_image_border_extractor);
  narf_keypoint_detector.setRangeImage(&range_image);
  narf_keypoint_detector.getParameters().support_size = support_size;
  // narf_keypoint_detector.getParameters ().add_points_on_straight_edges =
  // true; narf_keypoint_detector.getParameters
  // ().distance_for_additional_points = 0.5;

  pcl::PointCloud<int> keypoint_indices;
  narf_keypoint_detector.compute(keypoint_indices);
  std::cout << "Found " << keypoint_indices.points.size() << " key points.\n";
  //在距离图像显示组件内显示关键点
  // for (size_ti=0; i<keypoint_indices.points.size (); ++i)
  // range_image_widget.markPoint (keypoint_indices.points[i]%range_image.width,
  // keypoint_indices.points[i]/range_image.width);
  //在3D窗口中显示关键点
  pcl::PointCloud<pcl::PointXYZ>::Ptr keypoints_ptr(
      new pcl::PointCloud<pcl::PointXYZ>);
  pcl::PointCloud<pcl::PointXYZ>& keypoints = *keypoints_ptr;
  keypoints.points.resize(keypoint_indices.points.size());
  for (size_t i = 0; i < keypoint_indices.points.size(); ++i)
    keypoints.points[i].getVector3fMap() =
        range_image.points[keypoint_indices.points[i]].getVector3fMap();

  pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZ>
      keypoints_color_handler(keypoints_ptr, 0, 255, 0);
  viewer.addPointCloud<pcl::PointXYZ>(keypoints_ptr, keypoints_color_handler,
                                      "keypoints");
  viewer.setPointCloudRenderingProperties(
      pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 7, "keypoints");
  // 主循环
  while (!viewer.wasStopped()) {
    range_image_widget.spinOnce();  // process GUI events
    viewer.spinOnce();
    pcl_sleep(0.01);
  }
}
