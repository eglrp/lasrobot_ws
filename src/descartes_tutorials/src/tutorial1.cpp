// Core ros functionality like ros::init and spin
#include <ros/ros.h>
// ROS Trajectory Action server definition
#include <control_msgs/FollowJointTrajectoryAction.h>
// Means by which we communicate with above action-server
#include <actionlib/client/simple_action_client.h>

// Includes the descartes robot model we will be using
#include <descartes_moveit/moveit_state_adapter.h>
// Includes the descartes trajectory type we will be using
#include <descartes_trajectory/axial_symmetric_pt.h>
#include <descartes_trajectory/cart_trajectory_pt.h>
// Includes the planner we will be using
#include <descartes_planner/dense_planner.h>
//Include Eigen geometry header for rotations
#include </usr/include/eigen3/Eigen/Geometry>
//Include visualization markers for RViz
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>

typedef std::vector<descartes_core::TrajectoryPtPtr> TrajectoryVec;
typedef TrajectoryVec::const_iterator TrajectoryIter;

/**
 * Generates an completely defined (zero-tolerance) cartesian point from a pose
 */
descartes_core::TrajectoryPtPtr makeCartesianPoint(const Eigen::Affine3d& pose);
/**
 * Generates a cartesian point with free rotation about the Z axis of the EFF frame
 */
descartes_core::TrajectoryPtPtr makeTolerancedCartesianPoint(const Eigen::Affine3d& pose);

/**
 * Translates a descartes trajectory to a ROS joint trajectory
 */
trajectory_msgs::JointTrajectory
toROSJointTrajectory(const TrajectoryVec& trajectory, const descartes_core::RobotModel& model,
                     const std::vector<std::string>& joint_names, double time_delay);

/**
 * Sends a ROS trajectory to the robot controller
 */
bool executeTrajectory(const trajectory_msgs::JointTrajectory& trajectory);

//Function for easily defining poses
Eigen::Affine3f definePose(float transX, float transY, float transZ, float rotX, float rotY, float rotZ);
  
//Function for constructing quaternion starting from Euler rotations XYZ
Eigen::Quaternion<float> eulerToQuat(float rotX, float rotY, float rotZ);
  
//Creates pose that can be added to the TrajectoryVec vector.
descartes_core::TrajectoryPtPtr addPose(float transX, float transY, float transZ, float rotX, float rotY, float rotZ);

//Define function for easy marker creation
visualization_msgs::Marker createMarker(float transX, float transY, float transZ, float rotX, float rotY, float rotZ);

//Waits for a subscriber to subscribe to a publisher
//Used to wait for RViz to subscribe to the Marker topic before publishing them
bool waitForSubscribers(ros::Publisher & pub, ros::Duration timeout);

int main(int argc, char** argv)
{
  // Initialize ROS
  ros::init(argc, argv, "descartes_tutorial");
  ros::NodeHandle nh;

  // Required for communication with moveit components
  ros::AsyncSpinner spinner (1);
  spinner.start();

  
  /*
  	Eigen::Affine3d pose;
	pose = Eigen::Translation3d(0, 0, 1);

	Eigen::Matrix3d m;
		m = AngleAxisd(angle1, Vector3f::UnitZ())
    	* AngleAxisd(angle2, Vector3f::UnitY())
    	* AngleAxisd(angle3, Vector3f::UnitZ());

	pose.linear() = m;
  */
  
  // 1. Define sequence of points
  TrajectoryVec points;
  visualization_msgs::Marker marker;
  std::vector<visualization_msgs::Marker> markerVec;

  //Start the publisher for the Rviz Markers
  ros::Publisher vis_pub = nh.advertise<visualization_msgs::MarkerArray>( "visualization_marker_array", 1 );
  
  for (unsigned int i = 0; i < 10; ++i)
  {
  	marker = createMarker(0.6, 0.3, 0.2 + 0.05 * i, 0, 0, 0);
  	markerVec.push_back(marker);
    points.push_back(addPose(0.6, 0.3, 0.2 + 0.05 * i, 0, 0, 0));
  }

  for (unsigned int i = 0; i < 10; ++i)
  {
  	marker = createMarker(0.6, 0.3 + 0.04 * i, 0.7, 0, 0, 0);
  	markerVec.push_back(marker);
    points.push_back(addPose(0.6, 0.3 + 0.04 * i, 0.7, 0, 0, 0));
  }
  
  for (unsigned int i = 0; i < 9; ++i)
  {
    //toFrame() functie uit descartes_core/include/utils.h
    //pose = descartes_core::utils::toFrame(0.6, 0.7, 0.7, 0.0, 0.0, 0.1 * i, 1);
    marker = createMarker(0.6, 0.7, 0.7, 0.0, 0.0, 0.1 * i);
  	markerVec.push_back(marker);
	  points.push_back(addPose(0.6, 0.7, 0.7, 0.0, 0.0, 0.1 * i));
  }
  
  int size = markerVec.size();
  //Convert vector to array so we can publish it as a MarkerArray type.
  visualization_msgs::MarkerArray ma;
  ma.markers.resize(size);
  for(int i = 0;i < size;i++)
  {
    ma.markers[i] = markerVec[i];
  }
  
  ros::Rate loop_rate(10);
  ROS_INFO("Waiting for subscribers.");
  if(waitForSubscribers(vis_pub, ros::Duration(2.0)))
  {
  ROS_INFO("Subscriber found, publishing markers.");
    while(true){
      vis_pub.publish(ma);
	    ros::spinOnce();
	    loop_rate.sleep();
	  }
  } else {
    ROS_ERROR("No subscribers connected, markers not published");
  }

  // 2. Create a robot model and initialize it
  
  descartes_core::RobotModelPtr model (new descartes_moveit::MoveitStateAdapter);
  model->setCheckCollisions(true);

  // Name of description on parameter server. Typically just "robot_description".
  const std::string robot_description = "robot_description";

  // name of the kinematic group you defined when running MoveitSetupAssistant
  const std::string group_name = "robot_arm";

  // Name of frame in which you are expressing poses. Typically "world_frame" or "base_link".
  const std::string world_frame = "odom_combined";

  // tool center point frame (name of link associated with tool)
  const std::string tcp_frame = "Link8";

  if (!model->initialize(robot_description, group_name, world_frame, tcp_frame))
  {
    ROS_INFO("Could not initialize robot model");
    return -1;
  }

  // 3. Create a planner and initialize it with our robot model
  descartes_planner::DensePlanner planner;
  planner.initialize(model);

  // 4. Feed the trajectory to the planner
  if (!planner.planPath(points))
  {
    ROS_ERROR("Could not solve for a valid path");
    return -2;
  }

  TrajectoryVec result;
  if (!planner.getPath(result))
  {
    ROS_ERROR("Could not retrieve path");
    return -3;
  }

  // 5. Translate the result into a type that ROS understands
  // Get Joint Names
  std::vector<std::string> names;
  
  nh.getParam("controller_joint_names", names);
  // Generate a ROS joint trajectory with the result path, robot model, given joint names,
  // a certain time delta between each trajectory point
  trajectory_msgs::JointTrajectory joint_solution = toROSJointTrajectory(result, *model, names, 1.0);

  // 6. Send the ROS trajectory to the robot for execution

  if (!executeTrajectory(joint_solution))
  {
    ROS_ERROR("Could not execute trajectory!");
    return -4;
  }

  // Wait till user kills the process (Control-C)
  
  ROS_INFO("Done!");
  return 0;
}

descartes_core::TrajectoryPtPtr makeCartesianPoint(const Eigen::Affine3d& pose)
{
  using namespace descartes_core;
  using namespace descartes_trajectory;

  return TrajectoryPtPtr( new CartTrajectoryPt( TolerancedFrame(pose)) );
}

descartes_core::TrajectoryPtPtr makeTolerancedCartesianPoint(const Eigen::Affine3d& pose)
{
  using namespace descartes_core;
  using namespace descartes_trajectory;
  return TrajectoryPtPtr( new AxialSymmetricPt(pose, M_PI * 2 - 0.01, AxialSymmetricPt::X_AXIS) );
}

trajectory_msgs::JointTrajectory
toROSJointTrajectory(const TrajectoryVec& trajectory,
                     const descartes_core::RobotModel& model,
                     const std::vector<std::string>& joint_names,
                     double time_delay)
{
  // Fill out information about our trajectory
  trajectory_msgs::JointTrajectory result;
  result.header.stamp = ros::Time::now();
  result.header.frame_id = "world_frame";
  result.joint_names = joint_names;

  // For keeping track of time-so-far in the trajectory
  double time_offset = 0.0;
  // Loop through the trajectory
  for (TrajectoryIter it = trajectory.begin(); it != trajectory.end(); ++it)
  {
    // Find nominal joint solution at this point
    std::vector<double> joints;
    it->get()->getNominalJointPose(std::vector<double>(), model, joints);

    // Fill out a ROS trajectory point
    trajectory_msgs::JointTrajectoryPoint pt;
    pt.positions = joints;
    // velocity, acceleration, and effort are given dummy values
    // we'll let the controller figure them out
    pt.velocities.resize(joints.size(), 0.0);
    pt.accelerations.resize(joints.size(), 0.0);
    pt.effort.resize(joints.size(), 0.0);
    // set the time into the trajectory
    pt.time_from_start = ros::Duration(time_offset);
    // increment time
    time_offset += time_delay;

    result.points.push_back(pt);
  }

  return result;
}

bool executeTrajectory(const trajectory_msgs::JointTrajectory& trajectory)
{
  // Create a Follow Joint Trajectory Action Client
  actionlib::SimpleActionClient<control_msgs::FollowJointTrajectoryAction> ac("joint_trajectory_action", true);
  ROS_INFO("Waiting for action server to start.");
  if (!ac.waitForServer(ros::Duration(2.0)))
  {
    ROS_ERROR("Could not connect to action server");
    return false;
  }
  ROS_INFO("Action server started.");

  control_msgs::FollowJointTrajectoryGoal goal;
  goal.trajectory = trajectory;
  goal.goal_time_tolerance = ros::Duration(1.0);
  
  ac.sendGoal(goal);

  if (ac.waitForResult(goal.trajectory.points[goal.trajectory.points.size()-1].time_from_start + ros::Duration(5)))
  {
    ROS_INFO("Action server reported successful execution");
    return true;
  } else {
    ROS_WARN("Action server could not execute trajectory");
    return false;
  }
}

//Function for easily defining poses
Eigen::Affine3f definePose(float transX, float transY, float transZ, float rotX, float rotY, float rotZ)
{
  	Eigen::Matrix3f m;
	m = Eigen::AngleAxisf(rotX, Eigen::Vector3f::UnitX())
	  * Eigen::AngleAxisf(rotY, Eigen::Vector3f::UnitY())
	  * Eigen::AngleAxisf(rotZ, Eigen::Vector3f::UnitZ());
	
	Eigen::Affine3f pose;
	pose = Eigen::Translation3f(transX, transY, transZ);
	pose.linear() = m;
	
	return pose;
	
}
  
//Function for constructing quaternion starting from Euler rotations XYZ
Eigen::Quaternion<float> eulerToQuat(float rotX, float rotY, float rotZ)
{
  	Eigen::Matrix3f m;
	m = Eigen::AngleAxisf(rotX, Eigen::Vector3f::UnitX())
	  * Eigen::AngleAxisf(rotY, Eigen::Vector3f::UnitY())
	  * Eigen::AngleAxisf(rotZ, Eigen::Vector3f::UnitZ());
	
	Eigen::AngleAxis<float> aa;
	aa = Eigen::AngleAxisf(m);
	
	Eigen::Quaternion<float> quat;
	quat = Eigen::Quaternion<float>(aa);
	return quat;
}
  
//Define function for easy marker publishing (combines adding poses to the "points" array and publishing them)
descartes_core::TrajectoryPtPtr addPose(float transX, float transY, float transZ, float rotX, float rotY, float rotZ)
{
	//Define the pose
	Eigen::Affine3f pose;
	pose = definePose(transX, transY, transZ, rotX, rotY, rotZ);
	
	//Convert Affine3f to Affine3d
	Eigen::Affine3d pose_d = pose.cast<double>();
	
	//Convert to axialsymmetric point
	descartes_core::TrajectoryPtPtr pt = makeTolerancedCartesianPoint(pose_d); 
	return pt;
}

//Define function for easy marker creation
visualization_msgs::Marker createMarker(float transX, float transY, float transZ, float rotX, float rotY, float rotZ)
{
	static int count;
  visualization_msgs::Marker marker;
	marker.header.frame_id = "odom_combined";
	marker.header.stamp = ros::Time();
	marker.ns = "my_namespace";
	marker.id = count;
	marker.type = visualization_msgs::Marker::ARROW;
	marker.action = visualization_msgs::Marker::ADD;
	marker.lifetime = ros::Duration(0);
	
	marker.pose.position.x = transX;
	marker.pose.position.y = transY;
	marker.pose.position.z = transZ;
	
	//To calculate the quaternion values we first define an AngleAxis object using Euler rotations, then convert it
	Eigen::Quaternion<float> quat;
	quat = eulerToQuat(rotX, rotY, rotZ);
	
	marker.pose.orientation.x = quat.x();
	marker.pose.orientation.y = quat.y();
	marker.pose.orientation.z = quat.z();
	marker.pose.orientation.w = quat.w();
	marker.scale.x = 0.1;
	marker.scale.y = 0.01;
	marker.scale.z = 0.01;
	marker.color.a = 1.0;	//Alpha
	marker.color.r = 1.0;
	marker.color.g = 0.0;
	marker.color.b = 0.0;
	count++;
	return marker;
}

//Waits for a subscriber to subscribe to a publisher
bool waitForSubscribers(ros::Publisher & pub, ros::Duration timeout)
{
    if(pub.getNumSubscribers() > 0)
        return true;
    ros::Time start = ros::Time::now();
    ros::Rate waitTime(0.5);
    while(ros::Time::now() - start < timeout) {
        waitTime.sleep();
        if(pub.getNumSubscribers() > 0)
            break;
    }
    return pub.getNumSubscribers() > 0;
}
