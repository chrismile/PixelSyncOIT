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

#include "KDTree.hpp"

bool AxisAlignedBox::contains(const glm::vec3 &pt) const {
    if (pt.x >= min.x && pt.y >= min.y && pt.z >= min.z
        && pt.x <= max.x && pt.y <= max.y && pt.z <= max.z)
        return true;
    return false;
}

KDTree::KDTree() {
    root = nullptr;
}

KDTree::~KDTree() {
    _deleteAllData();
}

void KDTree::_deleteAllData() {
    root = nullptr;
    for (KDNode *node : nodes) {
        delete node;
    }
    nodes.clear();
}

void KDTree::build(const std::vector<IndexedPoint*> &points) {
    root = _build(points, 0);
}

KDNode *KDTree::_build(std::vector<IndexedPoint*> points, int depth) {
    _deleteAllData();
    const int k = 3; // Number of dimensions

    if (points.size() == 0) {
        return nullptr;
    }

    int axis = depth % k;
    std::sort(points.begin(), points.end(), [axis](IndexedPoint *a, IndexedPoint *b) {
        if (axis == 0) return a->position.x < b->position.x;
        if (axis == 1) return a->position.y < b->position.y;
        return a->position.z < b->position.z;
    });
    size_t medianIndex = points.size() / 2;
    std::vector<IndexedPoint*> leftPoints = std::vector<IndexedPoint*>(points.begin(), points.begin() + medianIndex);
    std::vector<IndexedPoint*> rightPoints = std::vector<IndexedPoint*>(points.begin() + medianIndex + 1, points.end());

    KDNode *node = new KDNode;
    node->axis = axis;
    node->point = points.at(medianIndex);
    node->left = _build(leftPoints, depth + 1);
    node->right = _build(rightPoints, depth + 1);
    nodes.push_back(node);
    return node;
}

void KDTree::addPoint(IndexedPoint *point) {
    root = _addPoint(root, 0, point);
}

KDNode *KDTree::_addPoint(KDNode *node, int depth, IndexedPoint *point) {
    const int k = 3; // Number of dimensions
    const int axis = depth % k;

    if (node == nullptr) {
        KDNode *node = new KDNode;
        node->axis = axis;
        node->point = point;
        nodes.push_back(node);
        return node;
    }

    if ((axis == 0 && point->position.x < node->point->position.x)
        || (axis == 1 && point->position.y < node->point->position.y)
        || (axis == 2 && point->position.z < node->point->position.z)) {
        node->left = _addPoint(node->left, depth + 1, point);
    } else {
        node->right = _addPoint(node->right, depth + 1, point);
    }
    return node;
}

std::vector<IndexedPoint*> KDTree::findPointsInAxisAlignedBox(const AxisAlignedBox &box) {
    std::vector<IndexedPoint*> points;
    _findPointsInAxisAlignedBox(box, points, root);
    return points;
}

std::vector<IndexedPoint*> KDTree::findAllPointsWithinRadius(IndexedPoint *centerPoint, float radius) {
    std::vector<IndexedPoint*> pointsWithDistance;
    std::vector<IndexedPoint*> pointsInRect;

    // First, find all points within the bounding box containing the search circle.
    AxisAlignedBox box;
    box.min = centerPoint->position - glm::vec3(radius, radius, radius);
    box.max = centerPoint->position + glm::vec3(radius, radius, radius);
    _findPointsInAxisAlignedBox(box, pointsInRect, root);

    // Now, filter all points out that are not within the search radius.
    float squaredRadius = radius*radius;
    glm::vec3 differenceVector;
    for (IndexedPoint *point : pointsInRect) {
        differenceVector = centerPoint->position - centerPoint->position;
        if (differenceVector.x*differenceVector.x + differenceVector.y*differenceVector.y
                + differenceVector.z*differenceVector.z <= squaredRadius) {
            pointsWithDistance.push_back(point);
        }
    }

    return pointsWithDistance;
}

void KDTree::_findPointsInAxisAlignedBox(const AxisAlignedBox &rect, std::vector<IndexedPoint*> &points, KDNode *node) {
    if (rect.contains(node->point->position)) {
        points.push_back(node->point);
    }

    if ((node->axis == 0 && rect.min.x <= node->point->position.x)
        || (node->axis == 1 && rect.min.y <= node->point->position.y)
        || (node->axis == 2 && rect.min.z <= node->point->position.z)) {
        _findPointsInAxisAlignedBox(rect, points, node->left);
    }
    if ((node->axis == 0 && rect.max.x >= node->point->position.x)
        || (node->axis == 1 && rect.max.y >= node->point->position.y)
        || (node->axis == 2 && rect.max.z >= node->point->position.z)) {
        _findPointsInAxisAlignedBox(rect, points, node->right);
    }
};


IndexedPoint *KDTree::findClosestIndexedPoint(IndexedPoint *centerPoint, float maxDistance) {
    // Find all points in bounding rectangle and test if their distance is less or equal than searched one
    AxisAlignedBox rect;
    rect.min = glm::vec3(
            centerPoint->position.x - maxDistance,
            centerPoint->position.y - maxDistance,
            centerPoint->position.z - maxDistance);
    rect.max = glm::vec3(
            centerPoint->position.x + maxDistance,
            centerPoint->position.y + maxDistance,
            centerPoint->position.z + maxDistance);

    std::vector<IndexedPoint*> rectPoints = findPointsInAxisAlignedBox(rect);

    IndexedPoint *closestPoint = NULL;
    float closestDistance = FLT_MAX;
    for (IndexedPoint *point : rectPoints) {
        float currDistance = (centerPoint->position - point->position).length();
        if (currDistance < closestDistance && currDistance < maxDistance) {
            closestPoint = point;
            closestDistance = currDistance;
        }
    }
    return closestPoint;
}
