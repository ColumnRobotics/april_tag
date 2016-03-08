april_tag
=========

Detects 2D fiducial markers (april tags) from ros image stream and produces id, location and orientation of the tags. This ros node wraps the C++ April Tag library written by Michael Kaess and Hordur Johannson. April tags were developed by Edwin Olson. 

More on april tags here:
http://april.eecs.umich.edu/wiki/index.php/AprilTags

April Tags C++ library:
http://people.csail.mit.edu/kaess/apriltags/

Input:

`/camera/image_raw`

Output:

AprilTagList which is a list of AprilTag:

```
uint32 	id
uint32 	hamming_distance
float64 distance
float64 x
float64 y
float64 z
float64 yaw
float64 pitch
float64 roll
```

distance,x,y,z are in cms. z is depth away from camera. x is horizontal with camera right as positive. 

Depends on: libeigen3-dev

-- palash

Here in this repository is the camera calibration file for the PlayStation Eye camera. The file is called pseye.yml.

You should rename this file camera.yml and place it in a folder that may need to be created called .ros/camera_info.

The file also contains information that may need to be changed in the cole_node.cpp.

This should be changed:
double f_x, f_y;
private_node_handle.param<double>("focal_length_px", camera_focal_length_x, f_x);
private_node_handle.param<double>("focal_length_px", camera_focal_length_y, f_y);

The values for f_x and f_y can be found in the pseye.yml file:
camera_matrix:
  rows: 3
  cols: 3
  data: [794.204174, 0, 258.925189, 0, 790.914562, 240.688826, 0, 0, 1]
  
In data:[794.204174, 0, 258.925189, 0, 790.914562, 240.688826, 0, 0, 1] = 
                                    [f_x, 0, d_x, 0, f_y, d_y, 0, 0, 1]

Where this is the camera intrinsic matrix:
                                    [f_x    0     d_x]
                                    [0      f_y   0  ]
                                    [0      0     1  ]

-- Cole


