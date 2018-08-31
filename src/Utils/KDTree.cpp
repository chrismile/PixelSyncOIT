//
// Created by christoph on 28.08.18.
//

#include "KDTree.hpp"

bool Rectangle::contains(const glm::vec3 &pt) const
{
    if (pt.x >= min.x && pt.y >= min.y && pt.z >= min.z
        && pt.x <= max.x && pt.y <= max.y && pt.z <= max.z)
        return true;
    return false;
}

void KDTree::build(const std::vector<Point*> &points)
{
    root = _build(points, 0);
}

std::shared_ptr<KDNode> KDTree::_build(std::vector<Point*> points, int depth)
{
    const int k = 3; // Number of dimensions

    if (points.size() == 0) {
        return std::shared_ptr<KDNode>();
    }

    int axis = depth % k;
    std::sort(points.begin(), points.end(), [axis](Point *a, Point *b) {
        if (axis == 0) return a->position.x < b->position.x;
        if (axis == 1) return a->position.y < b->position.y;
        return a->position.z < b->position.z;
    });
    size_t medianIndex = points.size() / 2;
    std::vector<Point*> leftPoints = std::vector<Point*>(points.begin(), points.begin() + medianIndex);
    std::vector<Point*> rightPoints = std::vector<Point*>(points.begin() + medianIndex + 1, points.end());

    std::shared_ptr<KDNode> node = std::shared_ptr<KDNode>(new KDNode);
    node->axis = axis;
    node->location = points.at(medianIndex);
    node->left = _build(leftPoints, depth + 1);
    node->right = _build(rightPoints, depth + 1);
}


std::vector<Point*> KDTree::findPointsInRectangle(const Rectangle &rect)
{
    std::vector<Point*> points;
    _findPointsInRectangle(rect, points, root);
    return points;
}

void KDTree::_findPointsInRectangle(const Rectangle &rect, std::vector<Point*> &points, std::shared_ptr<KDNode> node)
{
    if (rect.contains(node->location->position)) {
        points.push_back(node->location);
    }

    if (node->axis == 0 && rect.min.x <= node->location->position.x
        || node->axis == 1 && rect.min.y <= node->location->position.y
        || node->axis == 2 && rect.min.z <= node->location->position.z) {
        _findPointsInRectangle(rect, points, node->left);
    }
    if (node->axis == 0 && rect.max.x >= node->location->position.x
        || node->axis == 1 && rect.max.y >= node->location->position.y
        || node->axis == 2 && rect.max.z >= node->location->position.z) {
        _findPointsInRectangle(rect, points, node->right);
    }
};


Point *KDTree::findCloseIndexedPoint(Point *centerPoint, float maxDistance)
{
    // Find all points in bounding rectangle and test if their distance is less or equal than searched one
    Rectangle rect;
    rect.min = glm::vec3(
            centerPoint->position.x - maxDistance,
            centerPoint->position.y - maxDistance,
            centerPoint->position.z - maxDistance);
    rect.max = glm::vec3(
            centerPoint->position.x + maxDistance,
            centerPoint->position.y + maxDistance,
            centerPoint->position.z + maxDistance);

    std::vector<Point*> rectPoints = findPointsInRectangle(rect);

    Point *closestPoint = NULL;
    float closestDistance = FLT_MAX;
    for (Point *point : rectPoints) {
        float currDistance = (centerPoint->position - point->position).length();
        if (currDistance < closestDistance && currDistance < maxDistance) {
            closestPoint = point;
            closestDistance = currDistance;
        }
    }
    return closestPoint;
}
