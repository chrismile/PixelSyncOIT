/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2019, Christoph Neuhauser
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PIXELSYNCOIT_KDTREE_HPP
#define PIXELSYNCOIT_KDTREE_HPP

#include <algorithm>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

/**
 * Class representing a 3D point with an additional index attribute.
 */
class IndexedPoint
{
public:
    glm::vec3 position;
    int index;
};

/**
 * An axis aligned (bounding) box for search queries in the k-d-tree.
 */
class AxisAlignedBox
{
public:
    /// The minimum and maximum coordinate corners of the 3D cuboid.
    glm::vec3 min, max;

    /**
     * Tests whether the axis aligned box contains a point.
     * @param pt The point.
     * @return True if the box contains the point.
     */
    bool contains(const glm::vec3 &pt) const;
};

/**
 * A node in the k-d-tree. It stores in which axis the space is partitioned (x,y,z)
 * as an index, the position of the node, and its left and right children.
 */
class KDNode
{
public:
    KDNode() : axis(0), point(nullptr), left(nullptr), right(nullptr) {}
    int axis;
    IndexedPoint *point;
    KDNode *left;
    KDNode *right;
};

/**
 * The k-d-tree class. Used for searching point sets in space efficiently.
 * NOTE: The ownership of the memory the IndexedPoint objects lies in the responsibility of the user.
 */
class KDTree
{
public:
    KDTree();
    ~KDTree();

    /**
     * Builds a k-d-tree from the passed point array.
     * @param points The point array.
     */
    void build(const std::vector<IndexedPoint*> &points);

    /**
     * Adds a point to the k-d-tree.
     * NOTE: If you want to add more than one point, rather use @see build.
     * The addPoint method can generate a degenerate tree if the points come in in an unfavorable order.
     * @param point The point to add to the tree.
     */
    void addPoint(IndexedPoint *point);

    /**
     * Performs an area search in the k-d-tree and returns all points within a certain bounding box.
     * @param box The bounding box.
     * @return The points of the k-d-tree inside of the bounding box.
     */
    std::vector<IndexedPoint*> findPointsInAxisAlignedBox(const AxisAlignedBox &box);

    /**
     * Performs an area search in the k-d-tree and returns all points within a certain distance to the center point.
     * @param centerPoint The center point.
     * @param radius The search radius.
     * @return The points of the k-d-tree inside of the search radius.
     */
    std::vector<IndexedPoint*> findAllPointsWithinRadius(IndexedPoint *centerPoint, float radius);

    /**
     * Performs an area search in the k-d-tree and returns the closest point to a certain specified point.
     * If no point is found within a specified maximum distance, null is returned.
     * @param {IndexedPoint} centerPoint The point of which to find the closest neighbor (excluding itself).
     * @param {number} maxDistance The maximum distance the two points may have.
     * @return {IndexedPoint} The closest neighbor within the maximum distance.
     */
    IndexedPoint *findClosestIndexedPoint(IndexedPoint *centerPoint, float maxDistance);

private:
    // Internal implementations
    /**
     * Builds a k-d-tree from the passed point array recursively (for internal use only).
     * @param points The point array.
     * @return The parent node of the current sub-tree.
     */
    KDNode *_build(std::vector<IndexedPoint*> points, int depth);

    /**
     * Adds a point to the k-d-tree (for internal use only).
     * @param node The current node to process (or null).
     * @param depth The current depth in the tree.
     * @param point The point to add to the tree.
     * @return The node object above or a new leaf node.
     */
    KDNode *_addPoint(KDNode *node, int depth, IndexedPoint *point);

    /**
     * Performs an area search in the k-d-tree and returns all points within a certain bounding box
     * (for internal use only).
     * @param box The bounding box.
     * @param node The current k-d-tree node that is searched.
     * @param points The points of the k-d-tree inside of the bounding box.
     */
    void _findPointsInAxisAlignedBox(const AxisAlignedBox &box, std::vector<IndexedPoint*> &points, KDNode *node);

    /// Root of the tree
    KDNode *root;

    // For deletion
    void _deleteAllData();
    std::vector<KDNode*> nodes;
};

#endif //PIXELSYNCOIT_KDTREE_HPP
