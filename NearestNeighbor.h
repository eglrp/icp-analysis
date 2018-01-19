#pragma once
#include <flann/flann.hpp>

#include "Eigen.h"
#include <math.h>

#define DEBUG 0


struct Match {
	int idx;
	float weight;
};

class NearestNeighborSearch {
public:
	virtual ~NearestNeighborSearch() {}

	virtual void setMatchingMaxDistance(float maxDistance) {
		m_maxDistance = maxDistance;
	}

	virtual void buildIndex(const std::vector<Eigen::Vector3f>& targetPoints) = 0;
	virtual std::vector<Match> queryMatches(const std::vector<Vector3f>& transformedPoints) = 0;
	virtual void setDepthIntrinsicsAndRes(Matrix3f depthIntrinsics, unsigned width, unsigned height) = 0;
	virtual void setSourceIndices(std::vector<Vector2i> indices) = 0;

protected:
	float m_maxDistance;

	NearestNeighborSearch() : m_maxDistance{ 0.005f } {}
};


/**
 * Projective Correspondences.
 */
class ProjectiveCorrespondences : public NearestNeighborSearch {
public:

	static int count_matches_wo_dist0;
	ProjectiveCorrespondences() : NearestNeighborSearch() {}

	void buildIndex(const std::vector<Eigen::Vector3f>& targetPoints) {
		m_points = targetPoints;
	}

	std::vector<Match> queryMatches(const std::vector<Vector3f>& transformedPoints) {
		const unsigned nMatches = transformedPoints.size();
		std::vector<Match> matches(nMatches);
		const unsigned nTargetPoints = m_points.size();
		std::cout << "total possible nMatches: " << nMatches << std::endl;
		std::cout << "nTargetPoints: " << nTargetPoints << std::endl;
		count_matches_wo_dist0 = 0;

		int match_cnt = 0;

		#pragma omp parallel for
		for (int i = 0; i < nMatches; i++) {
			matches[i] = getClosestPoint(transformedPoints[i], i);
			if(matches[i].idx >= 0)
				match_cnt++;
		}
		std::cout << "total actual nMatches: " << match_cnt << std::endl;
		std::cout << "total non 0 dist nMatches: " << count_matches_wo_dist0 << std::endl;

		return matches;
	}

	void setDepthIntrinsicsAndRes(Matrix3f depthIntrinsics, unsigned width, unsigned height) {
		m_depthIntrinsics = depthIntrinsics;
		m_width = width;
		m_height = height;
	}

	void setSourceIndices(std::vector<Vector2i> indices){
		m_indices = indices;
	}


private:
	std::vector<Eigen::Vector3f> m_points;
	std::vector<Vector2i> m_indices;

	unsigned m_width = 0;
	unsigned m_height = 0;	
	Matrix3f m_depthIntrinsics = Matrix3f::Zero();

	Match getClosestPoint(const Vector3f& p, const int transPointIndex) {
		int idx = -1;
		int u=-1,v=-1;
		float dist, fovX, fovY, cX, cY;
		// float minDist = std::numeric_limits<float>::max();
		// for (unsigned int i = 0; i < m_points.size(); ++i) {
		// 	float dist = (p - m_points[i]).norm();
		// 	if (minDist > dist) {
		// 		idx = i;
		// 		minDist = dist;
		// 	}
		// }

		if(m_height == 0)
		{
			std::cout<<"m_height = "<<m_height<<"\nm_width = "<<m_width<<"\ndepthIntrinsics =\n"<<m_depthIntrinsics<<std::endl;
			return Match{ -1, 0.f };
		}

		fovX = m_depthIntrinsics(0, 0);
		fovY = m_depthIntrinsics(1, 1);
		cX = m_depthIntrinsics(0, 2);
		cY = m_depthIntrinsics(1, 2);

		u = rint(cX + ((p.x()*fovX)/p.z()));
		v = rint(cY + ((p.y()*fovY)/p.z()));

		int radius = 5;
		float minDist = std::numeric_limits<float>::max();
		for(int i=u-radius; (i>=0 && i<m_width && i<=u+radius) ; i++){
			for(int j=v-radius; (j>=0 && j<m_height && j<=v+radius); j++){
				int temp_idx = j*m_width + i;
				if(temp_idx<0 || temp_idx>=m_points.size()){
					continue;
				}
				if(m_points[temp_idx].allFinite()){
					float temp_dist = (p - m_points[temp_idx]).norm();
					if(temp_dist < minDist){
						idx = temp_idx;
						minDist = temp_dist;
					}
				}
			}
		}

		if(m_points[idx].allFinite() && minDist <= m_maxDistance){
			return Match{ idx, 1.f };
		}else{
			return Match{ -1, 0.f };
		}

		// idx = v*m_width + u;
		// if(idx<0 || idx>=m_points.size())
		// {
		// 	if(DEBUG){
		// 		std::cout<<"index: "<<idx;
		// 		std::cout<<"; u = "<<u;
		// 		//std::cout<<"; u_index = "<<m_indices[transPointIndex].y();
		// 		std::cout<<"; v = "<<v;
		// 		//std::cout<<"; v_index = "<<m_indices[transPointIndex].x();
		// 		std::cout<<"; p.x() = "<<p.x();
		// 		std::cout<<"; p.y() = "<<p.y();
		// 		std::cout<<"; p.z() = "<<p.z();
		// 		std::cout<<"; fovX = "<<fovX;
		// 		std::cout<<"; fovY = "<<fovY;
		// 		std::cout<<"; cX = "<<cX;
		// 		std::cout<<"; cY = "<<cY;
		// 		std::cout<<std::endl;
		// 	}
		// 	return Match{ -1, 0.f };
		// }

		// dist = (p - m_points[idx]).norm();

		// if (m_points[idx].allFinite()&&(dist <= m_maxDistance))
		// {
		// 	if(DEBUG && dist!=0){
		// 		count_matches_wo_dist0++;
		// 		std::cout<<"index: "<<idx;
		// 		std::cout<<"; u = "<<u;
		// 		std::cout<<"; u_index = "<<m_indices[transPointIndex].y();
		// 		std::cout<<"; v = "<<v;
		// 		std::cout<<"; v_index = "<<m_indices[transPointIndex].x();
		// 		std::cout<<"; p.x() = "<<p.x();
		// 		std::cout<<"; p.y() = "<<p.y();
		// 		std::cout<<"; p.z() = "<<p.z();
		// 		std::cout<<"; fovX = "<<fovX;
		// 		std::cout<<"; fovY = "<<fovY;
		// 		std::cout<<"; cX = "<<cX;
		// 		std::cout<<"; cY = "<<cY;
		// 		std::cout<<"; dist = "<<dist;
		// 		std::cout<<std::endl;
		// 	}
		// 	return Match{ idx, 1.f };
		// }
		// else
		// 	return Match{ -1, 0.f };
	}
};

int ProjectiveCorrespondences::count_matches_wo_dist0 =0;


/**
 * Brute-force nearest neighbor search.
 */
class NearestNeighborSearchBruteForce : public NearestNeighborSearch {
public:
	NearestNeighborSearchBruteForce() : NearestNeighborSearch() {}

	void buildIndex(const std::vector<Eigen::Vector3f>& targetPoints) {
		m_points = targetPoints;
	}

	std::vector<Match> queryMatches(const std::vector<Vector3f>& transformedPoints) {
		const unsigned nMatches = transformedPoints.size();
		std::vector<Match> matches(nMatches);
		const unsigned nTargetPoints = m_points.size();
		std::cout << "nMatches: " << nMatches << std::endl;
		std::cout << "nTargetPoints: " << nTargetPoints << std::endl;

		#pragma omp parallel for
		for (int i = 0; i < nMatches; i++) {
			matches[i] = getClosestPoint(transformedPoints[i]);
		}

		return matches;
	}

	void setDepthIntrinsicsAndRes(Matrix3f depthIntrinsics, unsigned width, unsigned height) {
			m_depthIntrinsics = depthIntrinsics;
			m_width = width;
			m_height = height;
	}

	void setSourceIndices(std::vector<Vector2i> indices){
		m_indices = indices;
	}

private:
	std::vector<Eigen::Vector3f> m_points;
	std::vector<Vector2i> m_indices;

	unsigned m_width = 0;
	unsigned m_height = 0;	
	Matrix3f m_depthIntrinsics = Matrix3f::Zero();

	Match getClosestPoint(const Vector3f& p) {
		int idx = -1;

		float minDist = std::numeric_limits<float>::max();
		for (unsigned int i = 0; i < m_points.size(); ++i) {
			float dist = (p - m_points[i]).norm();
			if (minDist > dist) {
				idx = i;
				minDist = dist;
			}
		}

		if (minDist <= m_maxDistance)
			return Match{ idx, 1.f };
		else
			return Match{ -1, 0.f };
	}
};


/**
 * Nearest neighbor search using FLANN.
 */
class NearestNeighborSearchFlann : public NearestNeighborSearch {
public:
	NearestNeighborSearchFlann() :
		NearestNeighborSearch(),
		m_nTrees{ 1 },
		m_index{ nullptr },
		m_flatPoints{ nullptr }
	{ }

	~NearestNeighborSearchFlann() {
		if (m_index) {
			delete m_flatPoints;
			delete m_index;
			m_flatPoints = nullptr;
			m_index = nullptr;
		}
	}

	void buildIndex(const std::vector<Eigen::Vector3f>& targetPoints) {
		std::cout << "Initializing FLANN index with " << targetPoints.size() << " points." << std::endl;

		// FLANN requires that all the points be flat. Therefore we copy the points to a separate flat array.
		m_flatPoints = new float[targetPoints.size() * 3];
		for (size_t pointIndex = 0; pointIndex < targetPoints.size(); pointIndex++) {
			for (size_t dim = 0; dim < 3; dim++) {
				m_flatPoints[pointIndex * 3 + dim] = targetPoints[pointIndex][dim];
			}
		}

		flann::Matrix<float> dataset(m_flatPoints, targetPoints.size(), 3);

		// Building the index takes some time.
		m_index = new flann::Index<flann::L2<float>>(dataset, flann::KDTreeIndexParams(m_nTrees));
		m_index->buildIndex();

		std::cout << "FLANN index created." << std::endl;
	}

	std::vector<Match> queryMatches(const std::vector<Vector3f>& transformedPoints) {
		if (!m_index) {
			std::cout << "FLANN index needs to be build before querying any matches." << std::endl;
			return {};
		}

		// FLANN requires that all the points be flat. Therefore we copy the points to a separate flat array.
		float* queryPoints = new float[transformedPoints.size() * 3];
		for (size_t pointIndex = 0; pointIndex < transformedPoints.size(); pointIndex++) {
			for (size_t dim = 0; dim < 3; dim++) {
				queryPoints[pointIndex * 3 + dim] = transformedPoints[pointIndex][dim];
			}
		}

		flann::Matrix<float> query(queryPoints, transformedPoints.size(), 3);
		flann::Matrix<int> indices(new int[query.rows * 1], query.rows, 1);
		flann::Matrix<float> distances(new float[query.rows * 1], query.rows, 1);
		
		// Do a knn search, searching for 1 nearest point and using 16 checks.
		flann::SearchParams searchParams{ 16 };
		searchParams.cores = 0;
		m_index->knnSearch(query, indices, distances, 1, searchParams);

		// Filter the matches.
		const unsigned nMatches = transformedPoints.size();
		std::vector<Match> matches;
		matches.reserve(nMatches);

		for (int i = 0; i < nMatches; ++i) {
			if (*distances[i] <= m_maxDistance)
				matches.push_back(Match{ *indices[i], 1.f });
			else
				matches.push_back(Match{ -1, 0.f });
		}

		// Release the memory.
		delete[] query.ptr();
		delete[] indices.ptr();
		delete[] distances.ptr();

		return matches;
	}

	void setDepthIntrinsicsAndRes(Matrix3f depthIntrinsics, unsigned width, unsigned height) {
		m_depthIntrinsics = depthIntrinsics;
		m_width = width;
		m_height = height;
	}

	void setSourceIndices(std::vector<Vector2i> indices){
		m_indices = indices;
	}

private:

	unsigned m_width = 0;
	unsigned m_height = 0;	
	Matrix3f m_depthIntrinsics = Matrix3f::Zero();
	std::vector<Vector2i> m_indices;

	int m_nTrees;
	flann::Index<flann::L2<float>>* m_index;
	float* m_flatPoints;
};


