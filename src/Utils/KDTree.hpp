//
// Created by christoph on 28.08.18.
//

#ifndef PIXELSYNCOIT_KDTREE_HPP
#define PIXELSYNCOIT_KDTREE_HPP

#include <algorithm>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

class Point
{
public:
    glm::vec3 position;
    int index;
};

class Rectangle
{
public:
    glm::vec3 min, max;
    bool contains(const glm::vec3 &pt) const;
};

class KDNode
{
public:
    int axis;
    Point *location;
    std::shared_ptr<KDNode> left;
    std::shared_ptr<KDNode> right;
};

class KDTree
{
    void build(const std::vector<Point*> &points);

    /// Used for e.g. giving points that are approx. the same (i.e. low distance)
    /// the same index.
    Point *findCloseIndexedPoint(Point *centerPoint, float maxDistance);
    std::vector<Point*> findPointsInRectangle(const Rectangle &rect);

private:
    // Internal implementations
    std::shared_ptr<KDNode> _build(std::vector<Point*> points, int depth);
    void _findPointsInRectangle(const Rectangle &rect, std::vector<Point*> &points, std::shared_ptr<KDNode> node);

    std::shared_ptr<KDNode> root;
};

#endif //PIXELSYNCOIT_KDTREE_HPP
