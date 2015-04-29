#include "pclutil/grid3d.hpp"

namespace objsearch {
    namespace pclutil {

	/** 
	 * Simple constructor which creates a cube grid
	 * 
	 * @param _dim length of each dimension
	 * @param _step step for each cell
	 * 
	 * @return 
	 */
	Grid3D::Grid3D(float _dim, float _step) {
	    xDim_ = yDim_ = zDim_ = _dim;
	    xStep_ = yStep_ = zStep_ = _step;

	    // width, height and depth in terms of number of indices assigned to
	    // each need to round up to make sure that if the step does not go
	    // exactly into the dimensions then we do not miss parts of the
	    // space which would end up outside the basic division.
	    width_ = std::ceil(xDim_/xStep_);
	    height_ = std::ceil(zDim_/zStep_); 
	    depth_ = std::ceil(yDim_/yStep_);

	    // number of indices in the grid are all its dimensions multiplied
	    values_.resize(width_ * height_ * depth_);
	}

	/** 
	 * More complex constructor. Z is the height of the grid, y is its
	 * depth, and x its width.
	 *
	 *  z   y
	 *  ^  /|
	 *  |  /
	 *  | /
	 *  |/____> x
	 * 
	 * @param _xDim Length of the x dimension
	 * @param _yDim y dimension
	 * @param _zDim z dimension
	 * @param _xStep Step of the x dimension
	 * @param _yStep y dim
	 * @param _zStep z dim
	 * 
	 * @return 
	 */
	Grid3D::Grid3D(float _xDim, float _yDim, float _zDim, float _xStep, float _yStep, float _zStep){
	    xDim_ = _xDim;
	    yDim_ = _yDim;
	    zDim_ = _zDim;
	    xStep_ = _xStep;
	    yStep_ = _yStep;
	    zStep_ = _zStep;

	    width_ = std::ceil(xDim_/xStep_);
	    height_ = std::ceil(zDim_/zStep_); 
	    depth_ = std::ceil(yDim_/yStep_);
	    
	    values_.resize(width_ * height_ * depth_);
	}

	/** 
	 * Get the index in the array of the point with the given coordinates
	 * 
	 * @param x 
	 * @param y 
	 * @param z 
	 * 
	 * @return 
	 */
	int Grid3D::pointIndex(float x, float y, float z) {
	    // integer truncation down to correct index
	    return indexFromDimIndices(x/xStep_, y/yStep_, z/zStep_);
	}

	/** 
	 * Get the index of a cell in the grid based on the indices of
	 * individual dimensions (like indexing into a 3d array)
	 * 
	 * @param x 
	 * @param y 
	 * @param z 
	 * 
	 * @return 
	 */
	int Grid3D::indexFromDimIndices(int x, int y, int z) {
	    // http://stackoverflow.com/questions/7367770/how-to-flatten-or-index-3d-array-in-1d-array
	    return x + width_ * (y + z * depth_);
	}

	/** 
	 * Get the centre of the cell containing the point specified by the three values
	 * 
	 * @param x 
	 * @param y 
	 * @param z 
	 * 
	 * @return 
	 */
	pcl::PointXYZ Grid3D::cellCentre(float x, float y, float z){
	    pcl::PointXYZ point;
	    // the second part gives the minimum point in the cell on that axis,
	    // centre is where half the step for that axis is added
	    point.x = xStep_/2 + std::floor(x/xStep_) * xStep_;
	    point.y = yStep_/2 + std::floor(y/yStep_) * yStep_;
	    point.z = zStep_/2 + std::floor(z/zStep_) * zStep_;
	    return point;
	}

	/** 
	 * Get the value of the grid cell with the given coordinates.
	 * 
	 * @param x 
	 * @param y 
	 * @param z 
	 * 
	 * @return Reference to the value of the cell
	 */
	int& Grid3D::at(float x, float y, float z) {
	    return values_[pointIndex(x, y, z)];
	}

    } // namespace pclutil
} // namespace objsearch