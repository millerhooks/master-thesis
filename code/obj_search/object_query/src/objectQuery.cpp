#include "objectQuery.hpp"

namespace objsearch {
    namespace objectquery {
	ObjectQuery::ObjectQuery(int argc, char *argv[]){
	    ros::init(argc, argv, "object_query");
	    ros::NodeHandle handle;

	    // Retrieve the directory containing the cloud to be processed
	    ROSUtil::getParam(handle, "/object_query/query_features", queryFile_);
	    // Retrieve the directory containing the cloud of features generated by
	    // feature extraction
	    ROSUtil::getParam(handle, "/object_query/target_features", targetFile_);
	    ROSUtil::getParam(handle, "/object_query/output_regions", outputRegions_);

	    // Extract the K parameter specified in the launch file. If it is
	    // still at the default value of -1, read the parameter from the
	    // param file instead.
	    ROSUtil::getParam(handle, "/object_query/K", K_);
	    if (K_ == -1){
		ROSUtil::getParam(handle, "/obj_search/object_query/K", K_);
	    }

	    ROSUtil::getParam(handle, "/obj_search/processed_data_dir", dataPath_);
	    
	    // If the given cloud file corresponds to a file in the raw data directory,
	    // extract the remaining directories in the path of the file so that the
	    // data can be put into the output directory with the same path.
	    if (queryFile_.compare(0, dataPath_.size(), dataPath_) == 0){
		dataSubDir_ = SysUtil::trimPath(std::string(queryFile_, dataPath_.size()), 1);
	    }

	    ROSUtil::getParam(handle, "/object_query/output_dir", outDir_);
	    // If output is not specified, set the output directory to be the processed
	    // data directory specified by the global parameters.
	    if (std::string("NULL").compare(outDir_) == 0) {
		ROSUtil::getParam(handle, "/obj_search/processed_data_dir", outDir_);
	    }

	    // The output path for processed clouds is the subdirectory combined
	    // with the top level output directory. If dataSubDir_ is not
	    // initialised, then clouds are simply output to the top level
	    // output directory
	    outPath_ = SysUtil::fullDirPath(SysUtil::combinePaths(outDir_, dataSubDir_));

	    // Read the headers for the point clouds that were provided as
	    // input, and look at the field names to determine which descriptor
	    // type is stored in the cloud.
	    pcl::PCDReader reader;
	    pcl::PCLPointCloud2 targetHeader;
	    reader.readHeader(targetFile_, targetHeader);
	    std::string targetField = targetHeader.fields[0].name;
	    
	    pcl::PCLPointCloud2 queryHeader;
	    reader.readHeader(queryFile_, queryHeader);
	    std::string queryField = queryHeader.fields[0].name;

	    // The descriptors for both clouds must be the same, otherwise we
	    // cannot compare them.
	    if (queryField.compare(targetField) != 0){
		ROS_ERROR("Fields of the two descriptor clouds do not match: \n"\
			  "Query: %s, target: %s", queryField.c_str(), targetField.c_str());
		exit(1);
	    }

	    // Define the locations of the original point clouds used to extract
	    // features by extracting filenames from the query and target files.
	    // We assume that the features directory is a subdirectory of the
	    // directory containing those original files.
	    std::string filePath = SysUtil::fullDirPath(SysUtil::trimPath(targetFile_, 2)); // original file dir
	    std::string queryFName = SysUtil::trimPath(queryFile_, -1);
	    std::string targetFName = SysUtil::trimPath(targetFile_, -1);

	    // if the files are intermediate, start the search for the
	    // underscore after the first five characters.
	    int qstart = 0;
	    int tstart = 0;
	    if (queryFName.front() == '0') {
		qstart = 5;
	    } else if (targetFName.front() == '0'){
		tstart = 5;
	    }

	    // Construct the name of the file by extracting part of the
	    // query/target file names
	    std::string queryName = std::string(queryFName.begin(),
						queryFName.begin()
						+ queryFName.find_first_of('_', qstart));
	    std::string targetName = std::string(targetFName.begin(),
						 targetFName.begin()
						 + targetFName.find_first_of('_', tstart));

	    // Combine the paths, filenames and the extension to get the full path
	    targetPointFile_ = filePath + targetName + ".pcd";
	    queryPointFile_ = filePath + queryName + ".pcd";
	    ROS_INFO("Loading target points from %s", targetPointFile_.c_str());
	    ROS_INFO("Loading query points from %s", queryPointFile_.c_str());
	    
	    // Depending on the type of the descriptor in the cloud, we need to
	    // instantiate a different template for the search function
	    if (queryField.compare("shot") == 0) {
		doSearch<pcl::SHOT352>();
	    } else if (queryField.compare("shape_context") == 0) {
		doSearch<pcl::ShapeContext1980>();
	    } else {
		ROS_ERROR("Unknown descriptor field specifier: %s", queryField.c_str());
                exit(1);
	    }
	}

	/** 
	 * Annotate points in a cloud loaded from a certain directory based on
	 * the annotations which are assumed to be in a corresponding directory
	 * with the "raw" directory as a prefix. For example, if given the
	 * directory /home/user/items/processed/cloud/room1, the corresponding
	 * raw directory would be /home/user/items/raw/cloud/room1. Points in
	 * the given cloud will be compared to the annotated objects, and
	 * labelled with the label of the nearest object, but only if the point
	 * is within a euclidean distance of maxDist of the object.
	 * 
	 * @param dir The top level room directory containing the cloud of
	 * interest. The corresponding raw directory will be deduced.
	 * @param cloud The cloud containing points to annotate.
	 * @param indices This vector will be populated with the indices of the
	 * points which have been labelled
	 * @param labels Will be populated with the labels of the points
	 * @param distances Will be populated with the minimum distance of the point to its object
	 * @param maxDist The maximum distance from an object a point can be to
	 * still be considered part of the object
	 */
	void ObjectQuery::annotatePoints(std::string dir, const pcl::PointCloud<pcl::PointXYZRGB>::Ptr& cloud,
					 std::vector<int>& indices, std::vector<std::string>& labels,
					 std::vector<float>& distances, float maxDist) {
	    // the location of the annotated objects is in the raw directory.
	    // replace "processed" with "raw" in the path. This should work OK
	    // if the directory received is a raw directory.
	    std::string rawDir = dir.replace(dir.find("processed"),
					     std::string("processed").size(), "raw");

	    // extract the annotated clouds from the raw directory along with their labels
	    std::vector<pclutil::AnnotatedCloud<pcl::PointXYZRGB> > annotations
		= pclutil::getAnnotatedClouds<pcl::PointXYZRGB>(rawDir);

	    pcl::KdTreeFLANN<pcl::PointXYZRGB> searchTree;
	    // want to find the minimum distance and the corresponding index.

	    pcl::PointXYZRGB point;
	    int minInd = 0; // index of the point closest to the current object
	    float minDist = std::numeric_limits<float>::max(); // minimum distance from the point to an object
	    std::vector<int> nn(1); // index of nearest point on annotated object
	    std::vector<float> pointDistance(1); // distance of point from annotated object

	    // look through all the points in the cloud to be annotated
	    for (size_t i = 0; i < cloud->size(); i++) {
		point = cloud->points[i];

		// reset the min index and distance for the new point
		minInd = 0;
		minDist = std::numeric_limits<float>::max();
		// look through all the annotated object clouds and find the nearest
		// neighbour to the point received.
		for (size_t j = 0; j < annotations.size(); j++) {
		    searchTree.setInputCloud(annotations[j].cloud);
		    searchTree.nearestKSearch(point, 1, nn, pointDistance); // search for 1-nn
		    // update index and minimum distance to the object
		    if (pointDistance[0] < minDist){
			minInd = j;
			minDist = pointDistance[0];
		    }
		}

		// if the point is within the requested distance of the object,
		// push information about the point onto the vectors
		if (minDist < maxDist){
		    indices.push_back(i);
		    labels.push_back(annotations[minInd].label);
		    distances.push_back(minDist);
		}
	    }
	}

	/** 
	 * Do a nearest neighbour search for the features in the query cloud in
	 * the target cloud. 
	 * 
	 */
	template<typename DescType>
	void ObjectQuery::doSearch() {
	    ROS_INFO("Doing descriptor search.");
	    
	    pcl::PCDReader reader;

	    // Read the input clouds for the target and query descriptors. We
	    // want to find descriptors in targetCloud which are close to those
	    // in queryCloud. Need to use typename here because of dependent
	    // scope - what it is depends on the instantiation of the template
	    // argument
	    typename pcl::PointCloud<DescType>::Ptr targetCloud(new pcl::PointCloud<DescType>());
	    typename pcl::PointCloud<DescType>::Ptr queryCloud(new pcl::PointCloud<DescType>());
	    reader.read(targetFile_, *targetCloud);
	    reader.read(queryFile_, *queryCloud);
	    

	    // Create a flannsearch object to use to do the NN search
	    typename pcl::search::FlannSearch<DescType, flann::L2_Simple<float> >
		*search(new pcl::search::FlannSearch<DescType, flann::L2_Simple<float> >());
	    // Flann needs to know the point representation so that it can
	    // convert it to its internal format
	    typename pcl::DefaultPointRepresentation<DescType>::ConstPtr
		descRepr(new pcl::DefaultPointRepresentation<DescType>());
	    search->setPointRepresentation(descRepr);
	    search->setInputCloud(targetCloud);
	    
	    // Use these to find the xyz position of the features in the clouds
	    pcl::PointCloud<pcl::PointXYZRGB>::Ptr targetPoints(new pcl::PointCloud<pcl::PointXYZRGB>());
	    pcl::PointCloud<pcl::PointXYZRGB>::Ptr queryPoints(new pcl::PointCloud<pcl::PointXYZRGB>());
	    reader.read(targetPointFile_, *targetPoints);
	    reader.read(queryPointFile_, *queryPoints);

	    std::vector<int> indices;
	    std::vector<std::string> labels;
	    std::vector<float> distances;
	    
	    annotatePoints(SysUtil::trimPath(queryPointFile_, 2), queryPoints, indices, labels, distances, 0.4);

	    ROS_INFO("%d annotated points of %d total", (int)indices.size(), (int)queryPoints->size());
	    
	    exit(1);

	    ROS_INFO("Starting search");
	    // Loop over all points in the query cloud
	    std::vector<std::vector<int> > nearest((int)queryCloud->size());
	    for (int i = 0; i < queryCloud->size(); i++) {
		ROS_INFO("Query point %d of %d", i + 1, (int)queryCloud->size());
		// Initialise vectors to store the closest K points to the query point.	
		std::vector<int> indices(K_);
		std::vector<float> square_dists(K_);
	    
		// Search for the closest K points to the query point
		search->nearestKSearch(queryCloud->points[i], K_, indices, square_dists);

		nearest.push_back(indices);

		if (outputRegions_) {
		    pcl::PCDWriter writer;

		    // create kd trees to use to find points within a given radius of a
		    // specific point
		    pcl::KdTreeFLANN<pcl::PointXYZRGB> kdtreeQuery;
		    pcl::KdTreeFLANN<pcl::PointXYZRGB> kdtreeTarget;
		    kdtreeQuery.setInputCloud(queryPoints);
		    kdtreeTarget.setInputCloud(targetPoints);
		    std::vector<int> nn; // indices of points within the radius
		    std::vector<float> pointRadiusSquaredDistance; // distances of those points

		    float radius = 1;
		    // find points withing a given radius of the query point
		    kdtreeQuery.radiusSearch(queryPoints->points[0], radius, nn,
					     pointRadiusSquaredDistance);
	    
		    ROS_INFO("%d points within radius %f of query point.", (int)nn.size(), radius);

		    // create a cloud to hold those points within the spherical region
		    pcl::PointCloud<pcl::PointXYZRGB>::Ptr regionPoints(new pcl::PointCloud<pcl::PointXYZRGB>());
		    for (int i = 0; i < nn.size(); i++) {
			regionPoints->push_back(queryPoints->points[nn[i]]);
		    }

		    // output the cloud
		    std::string filePath = SysUtil::trimPath(queryFile_, 1);
		    std::string queryRegionOut = outPath_ + "queryRegion.pcd";
		    ROS_INFO("Outputting query point region to %s", queryRegionOut.c_str());
		    writer.write<pcl::PointXYZRGB>(queryRegionOut, *regionPoints, true);

	    
		    ROS_INFO("Query point (%f, %f, %f)", queryPoints->points[0].x,
			     queryPoints->points[0].y, queryPoints->points[0].z);

		    // Loop over all the nearest neighbours and create regions for them as well.
		    for (int i = 0; i < K_; i++) {
			ROS_INFO("Index: %d, descriptor distance: %f", indices[i],
				 square_dists[i]);
			ROS_INFO("Target point (%f, %f, %f)", targetPoints->points[indices[i]].x,
				 queryPoints->points[indices[i]].y,
				 queryPoints->points[indices[i]].z);
			// clear the spherical region to populate it with points in the
			// region of the target points
			regionPoints->clear();
			kdtreeTarget.radiusSearch(targetPoints->points[indices[i]],
						  radius, nn, pointRadiusSquaredDistance);
			ROS_INFO("%d points within radius %f of matched target point.",
				 (int)nn.size(), radius);
			// push all points from the radius search into the new cloud
			for (int j = 0; j < nn.size(); j++) {
			    regionPoints->push_back(targetPoints->points[nn[j]]);
			}

			// output the region cloud
			std::string targetRegionOut = outPath_ + "nn" + std::to_string(i) + ".pcd";
			ROS_INFO("Outputting target point region to %s", targetRegionOut.c_str());
			writer.write<pcl::PointXYZRGB>(targetRegionOut, *regionPoints, true);
		    }
		}
	    }
	    ROS_INFO("Search complete");
	}

    } // namespace objectquery
} // namespace obj_search

int main(int argc, char *argv[]) {
    objsearch::objectquery::ObjectQuery oq(argc, argv);
    
    return 0;
}
