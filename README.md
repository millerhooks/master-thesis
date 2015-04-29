# master-thesis
KTH Master's thesis in Systems, Control and Robotics
# Install
The code for this project uses ROS Indigo.

You may need to install some additional packages to get things working.

Documentation is generated by [Doxygen](https://www.doxygen.org). The test framework
uses the [Google C++ Testing Framework](https://code.google.com/p/googletest/).
You can install both of these with

    sudo apt-get install doxygen libgtest-dev

As of writing (03/15) The `libgtest-dev` package on Ubuntu 14.04 does not
install libraries, only the headers. To create the library files you need to
compile them manually (code from
[here](http://www.thebigblob.com/getting-started-with-google-test-on-ubuntu/)
and
[here](http://stackoverflow.com/questions/13513905/how-to-properly-setup-googletest-on-linux)
for some pointers).

    sudo apt-get install cmake # install cmake
    cd /usr/src/gtest
    sudo cmake CMakeLists.txt
    sudo make
    
    # copy or symlink libgtest.a and libgtest_main.a to your /usr/lib folder
    sudo cp *.a /usr/lib

# Compilation

Some of the packages used by the STRANDS project require `qt_build` and
`mongodb_store`. On Ubuntu systems set up according to the
[tutorial](http://wiki.ros.org/indigo/Installation/Ubuntu), this can be done
with

    sudo apt-get install ros-indigo-qt-build
    sudo apt-get install ros-indigo-mongodb-store

You will also require the OpenCV nonfree library. There doesn't seem to be a
package for this in Ubuntu (although `libopencv-nonfree-dev` existed at some
point). To install, download OpenCV and run the following in the top level of
the extracted zip:

    cmake CMakeLists.txt
    make

This will take a while to compile everything. It may be possible to just compile
the nonfree library, but I don't know how. Find where other libraries from
OpenCV sit on your system with `locate --regexp 'libopencv.*.so'`. For me, they
sit in `/usr/lib/x86_64-linux-gnu/`. Copy the library files to that location.

    sudo cp lib/libopencv_nonfree* /usr/lib/x86-64-linux-gnu

And things should compile when `catkin_make` is run in the workspace which
contains the project. To set up a workspace, see the
[ROS tutorial](http://wiki.ros.org/catkin/Tutorials/create_a_workspace).

# Testing

In the `code/test` directory there are some files which you can use to check if
things are properly configured. To compile use

    catkin_make run_tests

This will also run the tests and give information about which ones passed or
failed (all should pass).

# Data

The data used comes from an internal dataset at KTH which consists of multiple
frames of rooms taken from a single position within the room. The frames are
registered to form a single large cloud of the whole room. Both the full cloud
and individual frames are available. Some information about transforms relating
the frames to the full cloud are found in an XML file.

All clouds start off in their own frame of reference, so the origin is at the
position at which the camera was when the frames were taken. The complete cloud
can be transformed into the map frame using the transform of the first
intermediate cloud from `room.xml`. The rooms are in a corridor, and there are
several rooms for which there are scans available, so doing this puts the cloud
in the correct position in the corridor. To transform the individual frames (e.g.
`intermediate_cloud0001.pcd`) into the map frame, it is necessary to first apply
the transform from the original position to the registered position, and then
apply the same transform as was applied to the complete cloud.

In addition, some annotated clouds of objects present in the rooms are also
available. Each cloud (e.g. `rgb_0013_label_0.pcd`) corresponds to an object
extracted from one of the intermediate clouds, in this case the 13th. Each cloud
has an associated label found in a text file with a single line (e.g.
`rgb_0013_label_0.txt`).

# Usage

## Preprocessing

The preprocessing node will reduce the size of the cloud by trimming the floors
and ceilings, and removing large planes. It is intended to be run on raw data
from scans of a room from a single position. There are numerous parameters which
can be specified in the
[launch file](code/obj_search/preprocess/launch/preprocess.launch). The default
is to carry out all preprocessing steps: downsampling, plane extraction,
trimming, annotation rotation and normal extraction.

The result of the processing is output to a `processed` directory. If given data
in `/pcddata/raw/annotated/rares/20140901/patrol_run_31/room_2/`, for example,
the resulting processed data will be placed in
`/pcddata/processed/annotated/rares/20140901/patrol_run_31/room_2/`.

### Complete clouds

This command will do preprocessing steps on the complete cloud specified, and
also process the associated annotations.

    roslaunch preprocess preprocess.launch cloud:=/home/michal/Downloads/pcddata/raw/annotated/rares/20140901/patrol_run_31/room_2/complete_cloud.pcd

This command will do preprocessing steps for all complete clouds in
subdirectories of the specified directory.
	
    roslaunch preprocess preprocess.launch cloud:=/home/michal/Downloads/pcddata/raw/annotated/rares/

This command will do preprocessing steps for all clouds in subdirectories of the
specified directory that contain the string "intermediate". You can also
provide regex input in the same position and it should work (untested).
	
    roslaunch preprocess preprocess.launch cloud:=/home/michal/Downloads/pcddata/raw/annotated/rares/ match:=intermediate


### Intermediate clouds
The intended use of the preprocessing on intermediate clouds is to generate
clouds which can be used to check the efficacy of the system. We extract
features from the complete cloud and an intermediate cloud, and then check the
correspondences between the extracted features.

    roslaunch preprocess preprocess.launch cloud:=/home/michal/Downloads/pcddata/raw/annotated/rares/20140901/patrol_run_31/room_2/intermediate_cloud0014.pcd

## Feature Extraction

## Object Query

Comparing SHOT features extracted from the 14th intermediate cloud to the
features extracted from the complete cloud.
	
    roslaunch object_query object_query.launch query:=/home/michal/Downloads/pcddata/processed/annotated/rares/20140901/patrol_run_31/room_2/features/0014_nonPlanes_shot.pcd target:=/home/michal/Downloads/pcddata/processed/annotated/rares/20140901/patrol_run_31/room_2/features/nonPlanes_shot.pcd
