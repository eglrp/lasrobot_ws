#include <descartes_tutorials/trajvis.h>

namespace trajvis{
	
	descartes_core::TrajectoryPtPtr visualizedTrajectory::makeTolerancedCartesianPoint(	double transX, 
																						double transY, 
																						double transZ, 
																						double rotX, 
																						double rotY, 
																						double rotZ)
	{
		using namespace descartes_core;
		using namespace descartes_trajectory;
		Eigen::Affine3d pose = utils::toFrame(transX, transY, transZ, rotX, rotY, rotZ, utils::EulerConventions::XYZ);
	
		//Works
		descartes_trajectory::PositionTolerance p;
		p = ToleranceBase::zeroTolerance<PositionTolerance>(transX, transY, transZ);
		descartes_trajectory::OrientationTolerance o;
		o = ToleranceBase::createSymmetric<OrientationTolerance>(rotX, rotY, rotZ, 0, 0, 2*M_PI);
		return TrajectoryPtPtr( new CartTrajectoryPt( TolerancedFrame(pose, p, o), 0.0, M_PI/12) );
	}

	descartes_core::TrajectoryPtPtr visualizedTrajectory::makeAxialSymmetricPoint(	double x, double y, double z, double rx, double ry,
																					double rz)
	{
		using namespace descartes_core;
		using namespace descartes_trajectory;
		return TrajectoryPtPtr( new AxialSymmetricPt(x, y, z, rx, ry, rz, M_PI/12, AxialSymmetricPt::Z_AXIS) );
	}
	
	Eigen::Affine3d visualizedTrajectory::definePose(double transX, double transY, double transZ, double rotX, double rotY, double rotZ)
	{
		Eigen::Matrix3d m;
		m = Eigen::AngleAxisd(rotX, Eigen::Vector3d::UnitX())
		* Eigen::AngleAxisd(rotY, Eigen::Vector3d::UnitY())
		* Eigen::AngleAxisd(rotZ, Eigen::Vector3d::UnitZ());
	
		Eigen::Affine3d pose;
		pose = Eigen::Translation3d(transX, transY, transZ);
		pose.linear() = m;
	
		return pose;
	}
	
	Eigen::Quaternion<double> visualizedTrajectory::eulerToQuat(double rotX, double rotY, double rotZ)
	{
		Eigen::Matrix3d m;
		m = Eigen::AngleAxisd(rotX, Eigen::Vector3d::UnitX())
			* Eigen::AngleAxisd(rotY, Eigen::Vector3d::UnitY())
			* Eigen::AngleAxisd(rotZ, Eigen::Vector3d::UnitZ());
	
		Eigen::AngleAxis<double> aa;
		aa = Eigen::AngleAxisd(m);
		
		Eigen::Quaternion<double> quat;
		quat = Eigen::Quaternion<double>(aa);
		return quat;
	}
	
	descartes_core::TrajectoryPtPtr visualizedTrajectory::addPose(	double transX, double transY, double transZ, double rotX, double rotY,
																	double rotZ, bool symmetric = true)
	{
		//Define the pose
		Eigen::Affine3d pose;
	
		descartes_core::TrajectoryPtPtr pt;
		if(symmetric){
			//Convert to axialsymmetric point
			pt = makeAxialSymmetricPoint(transX, transY, transZ, rotX, rotY, rotZ); 
		} else {
			pt = makeTolerancedCartesianPoint(transX, transY, transZ, rotX, rotY, rotZ);
		}
		return pt;
	}
	
	visualization_msgs::Marker visualizedTrajectory::createMarker(	double transX, double transY, double transZ, double rotX, double rotY, 																		double rotZ, descartes_trajectory::AxialSymmetricPt::FreeAxis axis =
																	descartes_trajectory::AxialSymmetricPt::X_AXIS)
	{
		static int count;
		visualization_msgs::Marker marker;
		marker.header.frame_id = "base_link";
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
		Eigen::Quaternion<double> quat;
		if(axis == descartes_trajectory::AxialSymmetricPt::X_AXIS)
		{
			quat = eulerToQuat(rotX, rotY, rotZ);
			marker.color.r = 1.0;
			marker.color.g = 0.0;
			marker.color.b = 0.0;
		} else if(axis == descartes_trajectory::AxialSymmetricPt::Y_AXIS)
		{
			quat = eulerToQuat(rotX, rotY, rotZ) * eulerToQuat(0, 0, M_PI / 2);
			marker.color.r = 0.0;
			marker.color.g = 1.0;
			marker.color.b = 0.0;
		} else if(axis == descartes_trajectory::AxialSymmetricPt::Z_AXIS)
		{
			quat = eulerToQuat(rotX, rotY, rotZ) * eulerToQuat(0, 3 * (M_PI / 2), 0);
			marker.color.r = 0.0;
			marker.color.g = 0.0;
			marker.color.b = 1.0;
		}
	
		marker.pose.orientation.x = quat.x();
		marker.pose.orientation.y = quat.y();
		marker.pose.orientation.z = quat.z();
		marker.pose.orientation.w = quat.w();
		marker.scale.x = 0.02;
		marker.scale.y = 0.002;
		marker.scale.z = 0.002;
		marker.color.a = 1.0;	//Alpha

		count++;
		return marker;
	}
	
	void visualizedTrajectory::addPoint(double transX, double transY, double transZ, double rotX, double rotY, double rotZ,
										ToleranceOption TO)
	{
		if(TO == TolerancedCartesianPoint) {
			trajvec.push_back(addPose(transX, transY, transZ, rotX, rotY, rotZ, false));
			markervec.push_back(createMarker(transX, transY, transZ, rotX, rotY, rotZ, descartes_trajectory::AxialSymmetricPt::X_AXIS));
			markervec.push_back(createMarker(transX, transY, transZ, rotX, rotY, rotZ, descartes_trajectory::AxialSymmetricPt::Y_AXIS));
			markervec.push_back(createMarker(transX, transY, transZ, rotX, rotY, rotZ, descartes_trajectory::AxialSymmetricPt::Z_AXIS));
		} else if(TO == AxialSymmetricPoint) {
			trajvec.push_back(addPose(transX, transY, transZ, rotX, rotY, rotZ, true));
			markervec.push_back(createMarker(transX, transY, transZ, rotX, rotY, rotZ, descartes_trajectory::AxialSymmetricPt::X_AXIS));
			markervec.push_back(createMarker(transX, transY, transZ, rotX, rotY, rotZ, descartes_trajectory::AxialSymmetricPt::Y_AXIS));
			markervec.push_back(createMarker(transX, transY, transZ, rotX, rotY, rotZ, descartes_trajectory::AxialSymmetricPt::Z_AXIS));
		}
	}
}
