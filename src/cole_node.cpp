//
// adapted from ros example and april tag examples - palash
//
#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "AprilTags/TagDetector.h"
#include "AprilTags/Tag36h11.h"
#include "AprilTags/Tag25h9.h"
#include <geometry_msgs/Pose.h>
#include <geometry_msgs/PoseArray.h>
#include <geometry_msgs/PoseStamped.h>

#include "april_tag/AprilTag.h" // rosmsg
#include "april_tag/AprilTagList.h" // rosmsg


static const std::string OPENCV_WINDOW = "Image window";

const double PI = 3.14159265358979323846;
const double TWOPI = 2.0*PI;

/**
 * Normalize angle to be within the interval [-pi,pi].
 */
inline double standardRad(double t) {
  if (t >= 0.) {
    t = fmod(t+PI, TWOPI) - PI;
  } else {
    t = fmod(t-PI, -TWOPI) + PI;
  }
  return t;
}

/**
 * Convert rotation matrix to Euler angles
 */
void wRo_to_euler(const Eigen::Matrix3d& wRo, double& yaw, double& pitch, double& roll) {
    yaw = standardRad(atan2(wRo(1,0), wRo(0,0)));
    double c = cos(yaw);
    double s = sin(yaw);
    pitch = standardRad(atan2(-wRo(2,0), wRo(0,0)*c + wRo(1,0)*s));
    roll  = standardRad(atan2(wRo(0,2)*s - wRo(1,2)*c, -wRo(0,1)*s + wRo(1,1)*c));
}


class AprilTagNode
{
  ros::NodeHandle nh_;
  image_transport::ImageTransport it_;
  image_transport::Subscriber image_sub_;
  image_transport::Publisher image_pub_;
  ros::Publisher tag_list_pub;
  AprilTags::TagDetector* tag_detector;
  ros::Publisher april_pose_pub;

  // allow configurations for these:  
  AprilTags::TagCodes tag_codes;
  double camera_focal_length_x; // in pixels. late 2013 macbookpro retina = 700
  double camera_focal_length_y; // in pixels
  double tag_size; // tag side length of frame in meters 
  bool  show_debug_image;

  //geometry_msgs::Pose april_pose;
  geometry_msgs::PoseStamped april_pose;

public:
  AprilTagNode() : 
    it_(nh_), 
    //tag_codes(AprilTags::tagCodes36h11), 
    tag_codes(AprilTags::tagCodes25h9), 
    tag_detector(NULL),
    camera_focal_length_y(790.914562),
    camera_focal_length_x(794.204174),
    tag_size(0.029), // 1 1/8in marker = 0.029m
    show_debug_image(true)
  {
    cout << "Entered Constructor" << endl;
    // Subscrive to input video feed and publish output video feed
    image_sub_ = it_.subscribe("/camera/image_raw", 1, &AprilTagNode::imageCb, this);
    image_pub_ = it_.advertise("/april_tag_debug/output_video", 1);
    tag_list_pub = nh_.advertise<april_tag::AprilTagList>("/april_tags", 100);
    //april_pose_pub = nh_.advertise<geometry_msgs::Pose>("/april_pose",50);
    april_pose_pub = nh_.advertise<geometry_msgs::PoseStamped>("/april_pose",50);

    // Use a private node handle so that multiple instances of the node can
    // be run simultaneously while using different parameters.
    ros::NodeHandle private_node_handle("~"); 
    private_node_handle.param<double>("focal_length_px", camera_focal_length_x, 794.204174);
    private_node_handle.param<double>("focal_length_px", camera_focal_length_y, 790.914562);
    private_node_handle.param<double>("tag_size_cm", tag_size, 2.9);
    private_node_handle.param<bool>("show_debug_image", show_debug_image, false);

    //camera_focal_length_y = camera_focal_length_x; // meh
    tag_size = tag_size / 100.0; // library takes input in meters


    cout << "got focal length x: " << camera_focal_length_x << endl;
    cout << "got focal length y: " << camera_focal_length_y << endl;
    cout << "got tag size " << tag_size << endl;
    tag_detector = new AprilTags::TagDetector(tag_codes);
    if (show_debug_image) {
      cv::namedWindow(OPENCV_WINDOW);
    }
    cout << "Exiting Constructor" << endl;

  }

  ~AprilTagNode()
  {
    if (show_debug_image) {
     cv::destroyWindow(OPENCV_WINDOW);
    }
  }

  april_tag::AprilTag convert_to_msg(AprilTags::TagDetection& detection, int width, int height) {
    // recovering the relative pose of a tag:

    // NOTE: for this to be accurate, it is necessary to use the
    // actual camera parameters here as well as the actual tag size
    // (m_fx, m_fy, m_px, m_py, m_tagSize)

    Eigen::Vector3d translation;
    Eigen::Matrix3d rotation;
    detection.getRelativeTranslationRotation(tag_size, 
                                             camera_focal_length_x, 
                                             camera_focal_length_y, 
                                             width / 2, 
                                             height / 2,
                                             translation, 
                                             rotation);

    Eigen::Matrix3d F;
    F <<
      1, 0,  0,
      0,  -1,  0,
      0,  0,  1;
    Eigen::Matrix3d fixed_rot = F*rotation;
    double yaw, pitch, roll;
    wRo_to_euler(fixed_rot, yaw, pitch, roll);


    april_tag::AprilTag tag_msg;
    //geometry_msgs::Pose april_pose;
    geometry_msgs::PoseStamped april_pose;

    tag_msg.id = detection.id;
    tag_msg.hamming_distance = detection.hammingDistance;
    tag_msg.distance = translation.norm() * 100.0;
    tag_msg.z = translation(0) * 100.0; // depth from camera
    tag_msg.x = translation(1) * 100.0; // horizontal displacement (camera pov right = +ve)
    tag_msg.y = translation(2) * 100.0; // vertical displacement
    tag_msg.yaw = yaw;
    tag_msg.pitch = pitch;
    tag_msg.roll = roll;

    /*april_pose.position.x = tag_msg.x;
    april_pose.position.y = tag_msg.y;
    april_pose.position.z = tag_msg.z;
    april_pose.orientation.x = tag_msg.yaw;
    april_pose.orientation.y = tag_msg.pitch;
    april_pose.orientation.z = tag_msg.roll;
    april_pose_pub.publish(april_pose);*/

    april_pose.pose.position.x = tag_msg.x;
    april_pose.pose.position.y = tag_msg.y;
    april_pose.pose.position.z = tag_msg.z;
    april_pose.pose.orientation.x = tag_msg.yaw;
    april_pose.pose.orientation.y = tag_msg.pitch;
    april_pose.pose.orientation.z = tag_msg.roll;
    april_pose.header.stamp = ros::Time::now();
    april_pose_pub.publish(april_pose);

    return tag_msg;
  }


  void processCvImage(cv_bridge::CvImagePtr cv_ptr) 
  {
    cv::Mat image_gray;
    cv::cvtColor(cv_ptr->image, image_gray, CV_BGR2GRAY);
    vector<AprilTags::TagDetection> detections = tag_detector->extractTags(image_gray);
    vector<april_tag::AprilTag> tag_msgs;

    for (int i=0; i<detections.size(); i++) {
      detections[i].draw(cv_ptr->image);
      tag_msgs.push_back(convert_to_msg(detections[i], cv_ptr->image.cols, cv_ptr->image.rows));
    }

    if(detections.size() > 0) { // take this out if you want absence notificaiton
      april_tag::AprilTagList tag_list;
      tag_list.april_tags = tag_msgs;
      tag_list_pub.publish(tag_list);
    }
  }


  void imageCb(const sensor_msgs::ImageConstPtr& msg)
  {
    cv_bridge::CvImagePtr cv_ptr;
    try
    {
      cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
    }
    catch (cv_bridge::Exception& e)
    {
      ROS_ERROR("cv_bridge exception: %s", e.what());
      return;
    }

    processCvImage(cv_ptr);

    if (show_debug_image) {
      // Update GUI Window
      cv::imshow(OPENCV_WINDOW, cv_ptr->image);
      cv::waitKey(3);
    }

    // Output modified video stream
    //image_pub_.publish(cv_ptr->toImageMsg());
  }
};

int main(int argc, char** argv)
{
  cout << "Ran Main" << std::endl;
  ros::init(argc, argv, "cole_node");
  AprilTagNode atn;
  ros::spin();
  return 0;
}
