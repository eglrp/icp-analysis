#include <iostream>
#include <fstream>

#include "Eigen.h"
#include "VirtualSensor.h"
#include "SimpleMesh.h"
#include "ICPOptimizer.h"
#include "ProcrustesAligner.h"
#include "PointCloud.h"

#define USE_POINT_TO_PLANE	1

#define RUN_PROCRUSTES		0
#define RUN_SHAPE_ICP		0
#define RUN_SEQUENCE_ICP	1

void debugCorrespondenceMatching() {
	// Load the source and target mesh.
	const std::string filenameSource = PROJECT_DIR + std::string("/data/bunny/bunny_part1.off");
	const std::string filenameTarget = PROJECT_DIR + std::string("/data/bunny/bunny_part2_trans.off");

	SimpleMesh sourceMesh;
	if (!sourceMesh.loadMesh(filenameSource)) {
		std::cout << "Mesh file wasn't read successfully." << std::endl;
		return;
	}

	SimpleMesh targetMesh;
	if (!targetMesh.loadMesh(filenameTarget)) {
		std::cout << "Mesh file wasn't read successfully." << std::endl;
		return;
	}

	PointCloud source{ sourceMesh };
	PointCloud target{ targetMesh };
	
	// Search for matches using FLANN.
	std::unique_ptr<NearestNeighborSearch> nearestNeighborSearch = std::make_unique<NearestNeighborSearchFlann>();
	nearestNeighborSearch->setMatchingMaxDistance(0.0001f);
	nearestNeighborSearch->buildIndex(target.getPoints());
	auto matches = nearestNeighborSearch->queryMatches(source.getPoints());

	// Visualize the correspondences with lines.
	SimpleMesh resultingMesh = SimpleMesh::joinMeshes(sourceMesh, targetMesh, Matrix4f::Identity());
	auto sourcePoints = source.getPoints();
	auto targetPoints = target.getPoints();

	for (unsigned i = 0; i < 100; ++i) { // sourcePoints.size()
		const auto match = matches[i];
		if (match.idx >= 0) {
			const auto& sourcePoint = sourcePoints[i];
			const auto& targetPoint = targetPoints[match.idx];
			resultingMesh = SimpleMesh::joinMeshes(SimpleMesh::cylinder(sourcePoint, targetPoint, 0.002f, 2, 15), resultingMesh, Matrix4f::Identity());
		}
	}

	resultingMesh.writeMesh(PROJECT_DIR + std::string("/results/correspondences.off"));
}

int debugReconstructRoomCorrespondences() {
	std::string filenameIn = PROJECT_DIR + std::string("/data/rgbd_dataset_freiburg1_xyz/");
	std::string filenameBaseOut = PROJECT_DIR + std::string("/results/mesh_");
	bool saveAll = false;

	// Load video
	std::cout << "Initialize virtual sensor..." << std::endl;
	VirtualSensor sensor;
	if (!sensor.init(filenameIn)) {
		std::cout << "Failed to initialize the sensor!\nCheck file path!" << std::endl;
		return -1;
	}

	// We store a first frame as a reference frame. All next frames are tracked relatively to the first frame.
	sensor.processNextFrame();
	if(PROJECTIVE)
		saveAll = true;
		
	PointCloud target{ sensor.getDepth(), sensor.getDepthIntrinsics(), sensor.getDepthExtrinsics(), sensor.getDepthImageWidth(), sensor.getDepthImageHeight(), 1, 0.1f, saveAll };
	//std::cout<<"Depth Extrinsic for target frame : "<<sensor.getDepthExtrinsics();
	
	// Setup the optimizer.
	ICPOptimizer optimizer;
	optimizer.setMatchingMaxDistance(0.1f);
	if (USE_POINT_TO_PLANE) {
		optimizer.usePointToPlaneConstraints(true);
		optimizer.setNbOfIterations(1); //10
	}
	else {
		optimizer.usePointToPlaneConstraints(false);
		optimizer.setNbOfIterations(1); //20
	}
	// TODO: debug param, Remove
	//optimizer.setNbOfIterations(1);
	// We store the estimated camera poses.
	std::vector<Matrix4f> estimatedPoses;
	Matrix4f currentCameraToWorld = Matrix4f::Identity();
	estimatedPoses.push_back(currentCameraToWorld.inverse());

	SimpleMesh currentDepthMeshTargetCorres{ sensor, estimatedPoses.back(), 0.1f };
	SimpleMesh currentCameraMeshTargetCorres = SimpleMesh::camera(estimatedPoses.back(), 0.0015f);
	SimpleMesh resultingMeshTargetCorres = SimpleMesh::joinMeshes(currentDepthMeshTargetCorres, currentCameraMeshTargetCorres, Matrix4f::Identity());
	std::string corres_class = std::string("/Debug_Nearest_Correspondences");
	if(PROJECTIVE)
		corres_class = std::string("/Debug_Projective_Correspondences");
	resultingMeshTargetCorres.writeMesh(PROJECT_DIR + std::string("/results") + corres_class + std::string("/target_correspondences") + std::string(".off"));

	int i = 0;
	const int iMax = 1;
	while (sensor.processNextFrame() && i <= iMax) {
		float* depthMap = sensor.getDepth();
		Matrix3f depthIntrinsics = sensor.getDepthIntrinsics();
		Matrix4f depthExtrinsics = sensor.getDepthExtrinsics();

		// Estimate the current camera pose from source to target mesh with ICP optimization.
		// We downsample the source image to speed up the correspondence matching.
		PointCloud source{ sensor.getDepth(), sensor.getDepthIntrinsics(), sensor.getDepthExtrinsics(), sensor.getDepthImageWidth(), sensor.getDepthImageHeight(), 8 };
		
		
		currentCameraToWorld = optimizer.estimatePose(source, target, currentCameraToWorld, i);
		
		SimpleMesh currentDepthMeshSourceCorres{ sensor, estimatedPoses.back(), 0.1f };
		SimpleMesh currentCameraMeshSourceCorres = SimpleMesh::camera(estimatedPoses.back(), 0.0015f);
		SimpleMesh resultingMeshSourceCorres = SimpleMesh::joinMeshes(currentDepthMeshSourceCorres, currentCameraMeshSourceCorres, Matrix4f::Identity());
		resultingMeshSourceCorres.writeMesh(PROJECT_DIR +  std::string("/results") + corres_class + std::string("/source_correspondences") + std::to_string(i) + std::string(".off"));
		// Invert the transformation matrix to get the current camera pose.
		Matrix4f currentCameraPose = currentCameraToWorld.inverse();
		std::cout << "Current camera pose: " << std::endl << currentCameraPose << std::endl;
		estimatedPoses.push_back(currentCameraPose);

		//if (i % 5 == 0) 
		{
			// We write out the mesh to file for debugging.
			SimpleMesh currentDepthMesh{ sensor, estimatedPoses.back(), 0.1f };
			SimpleMesh currentCameraMesh = SimpleMesh::camera(estimatedPoses.back(), 0.0015f);
			SimpleMesh resultingMesh = SimpleMesh::joinMeshes(currentDepthMesh, currentCameraMesh, Matrix4f::Identity());

			std::stringstream ss;
			ss << filenameBaseOut << sensor.getCurrentFrameCnt() << ".off";
			if (!resultingMesh.writeMesh(ss.str())) {
				std::cout << "Failed to write mesh!\nCheck file path!" << std::endl;
				return -1;
			}
		}
		
		i++;
	}

	return 0;
}

int alignBunnyWithProcrustes() {
	// Load the source and target mesh.
	const std::string filenameSource = PROJECT_DIR + std::string("/data/bunny/bunny.off");
	const std::string filenameTarget = PROJECT_DIR + std::string("/data/bunny/bunny_trans.off");

	SimpleMesh sourceMesh;
	if (!sourceMesh.loadMesh(filenameSource)) {
		std::cout << "Mesh file wasn't read successfully at location: " << filenameSource << std::endl;
		return -1;
	}

	SimpleMesh targetMesh;
	if (!targetMesh.loadMesh(filenameTarget)) {
		std::cout << "Mesh file wasn't read successfully at location: " << filenameTarget << std::endl;
		return -1;
	}

	// Fill in the matched points: sourcePoints[i] is matched with targetPoints[i].
	std::vector<Vector3f> sourcePoints; 
	sourcePoints.push_back(Vector3f(-0.0106867f, 0.179756f, -0.0283248f)); // left ear
	sourcePoints.push_back(Vector3f(-0.0639191f, 0.179114f, -0.0588715f)); // right ear
	sourcePoints.push_back(Vector3f(0.0590575f, 0.066407f, 0.00686641f)); // tail
	sourcePoints.push_back(Vector3f(-0.0789843f, 0.13256f, 0.0519517f)); // mouth
	
	std::vector<Vector3f> targetPoints;
	targetPoints.push_back(Vector3f(-0.02744f, 0.179958f, 0.00980739f)); // left ear
	targetPoints.push_back(Vector3f(-0.0847672f, 0.180632f, -0.0148538f)); // right ear
	targetPoints.push_back(Vector3f(0.0544159f, 0.0715162f, 0.0231181f)); // tail
	targetPoints.push_back(Vector3f(-0.0854079f, 0.10966f, 0.0842135f)); // mouth
		
	// Estimate the pose from source to target mesh with Procrustes alignment.
	ProcrustesAligner aligner;
	Matrix4f estimatedPose = aligner.estimatePose(sourcePoints, targetPoints);
	std::cout << "Estimated pose Procrustes: " << std::endl << estimatedPose << std::endl;

	// Visualize the resulting joined mesh. We add triangulated spheres for point matches.
	SimpleMesh resultingMesh = SimpleMesh::joinMeshes(sourceMesh, targetMesh, estimatedPose);
	for (const auto& sourcePoint : sourcePoints) {
		resultingMesh = SimpleMesh::joinMeshes(SimpleMesh::sphere(sourcePoint, 0.002f), resultingMesh, estimatedPose);
	}
	for (const auto& targetPoint : targetPoints) {
		resultingMesh = SimpleMesh::joinMeshes(SimpleMesh::sphere(targetPoint, 0.002f), resultingMesh, Matrix4f::Identity());
	}
	resultingMesh.writeMesh(PROJECT_DIR + std::string("/results/bunny_procrustes.off"));
	std::cout << "Resulting mesh written." << std::endl;
	
	return 0;
}

int alignBunnyWithICP() {
	// Load the source and target mesh.
	const std::string filenameSource = PROJECT_DIR + std::string("/data/bunny/bunny.off");
	const std::string filenameTarget = PROJECT_DIR + std::string("/data/bunny/bunny_trans.off");
	//const std::string filenameSource = PROJECT_DIR + std::string("/data/bunny/bunny_part1.off");
	//const std::string filenameTarget = PROJECT_DIR + std::string("/data/bunny/bunny_part2_trans.off");

	// Fill in the matched points: sourcePoints[i] is matched with targetPoints[i].
	std::vector<Vector3f> sourcePoints; 
	sourcePoints.push_back(Vector3f(-0.0106867f, 0.179756f, -0.0283248f)); // left ear
	sourcePoints.push_back(Vector3f(-0.0639191f, 0.179114f, -0.0588715f)); // right ear
	sourcePoints.push_back(Vector3f(0.0590575f, 0.066407f, 0.00686641f)); // tail
	sourcePoints.push_back(Vector3f(-0.0789843f, 0.13256f, 0.0519517f)); // mouth
	
	std::vector<Vector3f> targetPoints;
	targetPoints.push_back(Vector3f(-0.02744f, 0.179958f, 0.00980739f)); // left ear
	targetPoints.push_back(Vector3f(-0.0847672f, 0.180632f, -0.0148538f)); // right ear
	targetPoints.push_back(Vector3f(0.0544159f, 0.0715162f, 0.0231181f)); // tail
	targetPoints.push_back(Vector3f(-0.0854079f, 0.10966f, 0.0842135f)); // mouth

	SimpleMesh sourceMesh;
	if (!sourceMesh.loadMesh(filenameSource)) {
		std::cout << "Mesh file wasn't read successfully at location: " << filenameSource << std::endl;
		return -1;
	}

	SimpleMesh targetMesh;
	if (!targetMesh.loadMesh(filenameTarget)) {
		std::cout << "Mesh file wasn't read successfully at location: " << filenameTarget << std::endl;
		return -1;
	}

	// Estimate the pose from source to target mesh with ICP optimization.
	ICPOptimizer optimizer;
	optimizer.setMatchingMaxDistance(0.0003f);
	if (USE_POINT_TO_PLANE) {
		optimizer.usePointToPlaneConstraints(true);
		optimizer.setNbOfIterations(10);
	}
	else {
		optimizer.usePointToPlaneConstraints(false);
		optimizer.setNbOfIterations(20);
	}

	PointCloud source{ sourceMesh };
	PointCloud target{ targetMesh };

	Matrix4f estimatedPose = optimizer.estimatePose(source, target);
	std::cout << "Estimated pose: " << std::endl << estimatedPose << std::endl;
	
	// Visualize the resulting joined mesh. 
	SimpleMesh resultingMesh = SimpleMesh::joinMeshes(sourceMesh, targetMesh, estimatedPose);
	resultingMesh.writeMesh(PROJECT_DIR + std::string("/results/bunny_icp.off"));
	std::cout << "Resulting mesh written." << std::endl;	

	// Visualize the resulting joined mesh. We add triangulated spheres for point matches.
	std::vector<Vector3f> transformedSourcePoints; 
	const auto rotation = estimatedPose.block(0, 0, 3, 3);
	const auto translation = estimatedPose.block(0, 3, 3, 1);
	float error=0;
	int i=0;
	for (const auto& sourcePoint : sourcePoints) {
		resultingMesh = SimpleMesh::joinMeshes(SimpleMesh::sphere(sourcePoint, 0.002f), resultingMesh, estimatedPose);
		transformedSourcePoints.push_back(rotation * sourcePoint + translation);
		float error_point = (transformedSourcePoints[i] - targetPoints[i]).norm();
		std::cout<<"Error for point "<<(i+1)<<" : "<<error_point<<std::endl;
		error += error_point;
		i++;
	}
	std::cout<<"Error calculated: "<<error<<std::endl;
	for (const auto& targetPoint : targetPoints) {
		resultingMesh = SimpleMesh::joinMeshes(SimpleMesh::sphere(targetPoint, 0.002f, { 0, 255, 0, 255 }), resultingMesh, Matrix4f::Identity());
	}


	resultingMesh.writeMesh(PROJECT_DIR + std::string("/results/bunny_icp_spheres.off"));
	std::cout << "Resulting mesh written." << std::endl;

	return 0;
}

int reconstructRoom() {
	std::string filenameIn = PROJECT_DIR + std::string("/data/rgbd_dataset_freiburg1_xyz/");
	std::string filenameBaseOut = PROJECT_DIR + std::string("/results/mesh_");
	bool saveAll = false;

	// Load video
	std::cout << "Initialize virtual sensor..." << std::endl;
	VirtualSensor sensor;
	if (!sensor.init(filenameIn)) {
		std::cout << "Failed to initialize the sensor!\nCheck file path!" << std::endl;
		return -1;
	}

	// We store a first frame as a reference frame. All next frames are tracked relatively to the first frame.
	sensor.processNextFrame();
	if(PROJECTIVE)
		saveAll = true;
		
	PointCloud target{ sensor.getDepth(), sensor.getDepthIntrinsics(), sensor.getDepthExtrinsics(), sensor.getDepthImageWidth(), sensor.getDepthImageHeight(), 1, 0.1f, saveAll };
	//std::cout<<"Depth Extrinsic for target frame : "<<sensor.getDepthExtrinsics();
	
	// Setup the optimizer.
	ICPOptimizer optimizer;
	optimizer.setMatchingMaxDistance(0.1f);
	if (USE_POINT_TO_PLANE) {
		std::cout<<"POINT TO PLANE"<<std::endl;
		optimizer.usePointToPlaneConstraints(true);
		optimizer.setNbOfIterations(10);
	}
	else {
		std::cout<<"POINT TO POINT"<<std::endl;
		optimizer.usePointToPlaneConstraints(false);
		optimizer.setNbOfIterations(20);
	}

	// We store the estimated camera poses.
	std::vector<Matrix4f> estimatedPoses;
	Matrix4f currentCameraToWorld = Matrix4f::Identity();
	estimatedPoses.push_back(currentCameraToWorld.inverse());

	int i = 0;
	const int iMax = 50;
	while (sensor.processNextFrame() && i <= iMax) {
		float* depthMap = sensor.getDepth();
		Matrix3f depthIntrinsics = sensor.getDepthIntrinsics();
		Matrix4f depthExtrinsics = sensor.getDepthExtrinsics();

		//std::cout<<"Depth Extrinsic for source frame "<<i<<" : "<<depthExtrinsics;

		// Estimate the current camera pose from source to target mesh with ICP optimization.
		// We downsample the source image to speed up the correspondence matching.
		PointCloud source{ sensor.getDepth(), sensor.getDepthIntrinsics(), sensor.getDepthExtrinsics(), sensor.getDepthImageWidth(), sensor.getDepthImageHeight(), 8 };
		currentCameraToWorld = optimizer.estimatePose(source, target, currentCameraToWorld);
		
		// Invert the transformation matrix to get the current camera pose.
		Matrix4f currentCameraPose = currentCameraToWorld.inverse();
		std::cout << "Current camera pose: " << std::endl << currentCameraPose << std::endl;
		estimatedPoses.push_back(currentCameraPose);

		if (i % 5 == 0) {
			// We write out the mesh to file for debugging.
			SimpleMesh currentDepthMesh{ sensor, currentCameraPose, 0.1f };
			SimpleMesh currentCameraMesh = SimpleMesh::camera(currentCameraPose, 0.0015f);
			SimpleMesh resultingMesh = SimpleMesh::joinMeshes(currentDepthMesh, currentCameraMesh, Matrix4f::Identity());

			std::stringstream ss;
			ss << filenameBaseOut << sensor.getCurrentFrameCnt() << ".off";
			if (!resultingMesh.writeMesh(ss.str())) {
				std::cout << "Failed to write mesh!\nCheck file path!" << std::endl;
				return -1;
			}
		}
		
		i++;
	}

	return 0;
}

int reconstructRoomGetSources() {
	std::string filenameIn = PROJECT_DIR + std::string("/data/rgbd_dataset_freiburg1_xyz/");
	std::string filenameBaseOut = PROJECT_DIR + std::string("/results/mesh_");
	bool saveAll = false;

	// Load video
	std::cout << "Initialize virtual sensor..." << std::endl;
	VirtualSensor sensor;
	if (!sensor.init(filenameIn)) {
		std::cout << "Failed to initialize the sensor!\nCheck file path!" << std::endl;
		return -1;
	}

	// We store a first frame as a reference frame. All next frames are tracked relatively to the first frame.
	sensor.processNextFrame();
	if(PROJECTIVE)
		saveAll = true;
		
	PointCloud target{ sensor.getDepth(), sensor.getDepthIntrinsics(), sensor.getDepthExtrinsics(), sensor.getDepthImageWidth(), sensor.getDepthImageHeight(), 1, 0.1f, saveAll };
	//std::cout<<"Depth Extrinsic for target frame : "<<sensor.getDepthExtrinsics();

	int i = 0;
	const int iMax = 50;
	Matrix4f mIdentity = Matrix4f::Identity();
	while (sensor.processNextFrame() && i <= iMax) {
		float* depthMap = sensor.getDepth();
		Matrix3f depthIntrinsics = sensor.getDepthIntrinsics();
		Matrix4f depthExtrinsics = sensor.getDepthExtrinsics();

		//std::cout<<"Depth Extrinsic for source frame "<<i<<" : "<<depthExtrinsics;

		// Estimate the current camera pose from source to target mesh with ICP optimization.
		// We downsample the source image to speed up the correspondence matching.
		PointCloud source{ sensor.getDepth(), sensor.getDepthIntrinsics(), sensor.getDepthExtrinsics(), sensor.getDepthImageWidth(), sensor.getDepthImageHeight(), 8 };

		if (i % 5 == 0) {
			// We write out the mesh to file for debugging.
			SimpleMesh currentDepthMesh{ sensor, mIdentity, 0.1f };
			SimpleMesh currentCameraMesh = SimpleMesh::camera(mIdentity, 0.0015f);
			SimpleMesh resultingMesh = SimpleMesh::joinMeshes(currentDepthMesh, currentCameraMesh, Matrix4f::Identity());

			std::stringstream ss;
			ss << filenameBaseOut << sensor.getCurrentFrameCnt() << ".off";
			if (!resultingMesh.writeMesh(ss.str())) {
				std::cout << "Failed to write mesh!\nCheck file path!" << std::endl;
				return -1;
			}
		}
		
		i++;
	}

	return 0;
}

int reconstructRoom2() {
	std::string filenameIn = PROJECT_DIR + std::string("/data/rgbd_dataset_freiburg1_xyz/");
	std::string filenameBaseOut = PROJECT_DIR + std::string("/results/mesh_");

	// Load video
	std::cout << "Initialize virtual sensor..." << std::endl;
	VirtualSensor sensor;
	if (!sensor.init(filenameIn)) {
		std::cout << "Failed to initialize the sensor!\nCheck file path!" << std::endl;
		return -1;
	}

	// We store a first frame as a reference frame. All next frames are tracked relatively to the first frame.
	sensor.processNextFrame();
	PointCloud target{ sensor.getDepth(), sensor.getDepthIntrinsics(), sensor.getDepthExtrinsics(), sensor.getDepthImageWidth(), sensor.getDepthImageHeight() };
	
	// Setup the optimizer.
	ICPOptimizer optimizer;
	optimizer.setMatchingMaxDistance(0.1f);
	if (USE_POINT_TO_PLANE) {
		optimizer.usePointToPlaneConstraints(true);
		optimizer.setNbOfIterations(10);
	}
	else {
		optimizer.usePointToPlaneConstraints(false);
		optimizer.setNbOfIterations(20);
	}

	// We store the estimated camera poses.
	std::vector<Matrix4f> estimatedPoses;
	std::vector<Matrix4f> transformedEstC2WPoses;
	Matrix4f currentCameraToWorld = Matrix4f::Identity();
	estimatedPoses.push_back(currentCameraToWorld.inverse());
	// transformedEstC2WPoses stores accumulated estimated transform from the 1st frame to the current frame
	transformedEstC2WPoses.push_back(currentCameraToWorld);


	int i = 0;
	const int iMax = 50;
	while (sensor.processNextFrame() && i <= iMax) {
		float* depthMap = sensor.getDepth();
		Matrix3f depthIntrinsics = sensor.getDepthIntrinsics();
		Matrix4f depthExtrinsics = sensor.getDepthExtrinsics();

		// Estimate the current camera pose from source to target mesh with ICP optimization.
		// We downsample the source image to speed up the correspondence matching.
		PointCloud source{ sensor.getDepth(), sensor.getDepthIntrinsics(), sensor.getDepthExtrinsics(), sensor.getDepthImageWidth(), sensor.getDepthImageHeight(), 8 };
		currentCameraToWorld = optimizer.estimatePose(source, target, Matrix4f::Identity());
		
		//Multiplying the current estimated transform from the previous frame to the current.
		Matrix4f transformedEstC2WPose = transformedEstC2WPoses.back()*currentCameraToWorld;
		transformedEstC2WPoses.push_back(transformedEstC2WPose);
		// Invert the transformation matrix to get the current camera pose.
		Matrix4f currentCameraPose = transformedEstC2WPose.inverse();
		std::cout << "Current camera pose: " << std::endl << currentCameraPose << std::endl;
		Matrix4f prevCameraPose = estimatedPoses.back();
		estimatedPoses.push_back(currentCameraPose);

		if (i % 5 == 0) {
			// We write out the mesh to file for debugging.
			SimpleMesh currentDepthMesh{ sensor, currentCameraPose, 0.1f };
			SimpleMesh currentCameraMesh = SimpleMesh::camera(currentCameraPose, 0.0015f);
			SimpleMesh resultingMesh = SimpleMesh::joinMeshes(currentDepthMesh, currentCameraMesh, Matrix4f::Identity());

			std::stringstream ss;
			ss << filenameBaseOut << sensor.getCurrentFrameCnt() << ".off";
			if (!resultingMesh.writeMesh(ss.str())) {
				std::cout << "Failed to write mesh!\nCheck file path!" << std::endl;
				return -1;
			}
		}

		target = source;
		
		i++;
	}

	return 0;
}

int main() {
	int result = -1;

	clock_t begin = clock();
	
	if (RUN_PROCRUSTES)
		result = alignBunnyWithProcrustes();
	else if (RUN_SHAPE_ICP)
		result = alignBunnyWithICP();
	else if (RUN_SEQUENCE_ICP)
		//result = reconstructRoomGetSources();
		//result = debugReconstructRoomCorrespondences();
		result = reconstructRoom();

	clock_t end = clock();

	double elapsedSecs = double(end - begin) / CLOCKS_PER_SEC;
	std::cout << "Completed in " << elapsedSecs << " seconds." << std::endl;

	return result;
}
