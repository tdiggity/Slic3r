#ifndef slic3r_TriangleMesh_hpp_
#define slic3r_TriangleMesh_hpp_

#include "libslic3r.h"
#include <admesh/stl.h>
#include <vector>
#include <boost/thread.hpp>
#include "BoundingBox.hpp"
#include "Line.hpp"
#include "Point.hpp"
#include "Polygon.hpp"
#include "ExPolygon.hpp"

namespace Slic3r {

class TriangleMesh;
class TriangleMeshSlicer;
typedef std::vector<TriangleMesh*> TriangleMeshPtrs;

class TriangleMesh
{
public:
    TriangleMesh();
    TriangleMesh(const Pointf3s &points, const std::vector<Point3> &facets);
    TriangleMesh(const TriangleMesh &other);
    TriangleMesh(TriangleMesh &&other);
    TriangleMesh& operator=(const TriangleMesh &other);
    TriangleMesh& operator=(TriangleMesh &&other);
    void swap(TriangleMesh &other);
    ~TriangleMesh();
    void ReadSTLFile(const char* input_file);
    void write_ascii(const char* output_file);
    void write_binary(const char* output_file);
    void repair();
    float volume();
    void check_topology();
    bool is_manifold() const;
    void WriteOBJFile(char* output_file);
    void scale(float factor);
    void scale(const Pointf3 &versor);
    void translate(float x, float y, float z);
    void rotate(float angle, const Axis &axis);
    void rotate(float angle, Pointf3 axis);
    void rotate_x(float angle);
    void rotate_y(float angle);
    void rotate_z(float angle);
    void mirror(const Axis &axis);
    void mirror_x();
    void mirror_y();
    void mirror_z();
    void transform(const float* matrix3x4);
    void align_to_origin();
    void rotate(double angle, Point* center);
    TriangleMeshPtrs split() const;
    void merge(const TriangleMesh &mesh);
    ExPolygons horizontal_projection() const;
    const float* first_vertex() const;
    Polygon convex_hull();
    BoundingBoxf3 bounding_box() const;
    // Returns the bbox of this TriangleMesh transformed by the given matrix
    BoundingBoxf3 transformed_bounding_box(const std::vector<float>& matrix) const;
    // Returns the convex hull of this TriangleMesh
    TriangleMesh convex_hull_3d() const;
    void reset_repair_stats();
    bool needed_repair() const;
    size_t facets_count() const;

    // Returns true, if there are two and more connected patches in the mesh.
    // Returns false, if one or zero connected patch is in the mesh.
    bool has_multiple_patches() const;

    // Count disconnected triangle patches.
    size_t number_of_patches() const;

    mutable stl_file stl;
    bool repaired;
    
private:
    void require_shared_vertices();
    friend class TriangleMeshSlicer;
};

enum FacetEdgeType { 
    // A general case, the cutting plane intersect a face at two different edges.
    feGeneral,
    // Two vertices are aligned with the cutting plane, the third vertex is below the cutting plane.
    feTop,
    // Two vertices are aligned with the cutting plane, the third vertex is above the cutting plane.
    feBottom,
    // All three vertices of a face are aligned with the cutting plane.
    feHorizontal
};

class IntersectionReference
{
public:
    IntersectionReference() : point_id(-1), edge_id(-1) {};
    IntersectionReference(int point_id, int edge_id) : point_id(point_id), edge_id(edge_id) {}
    // Where is this intersection point located? On mesh vertex or mesh edge?
    // Only one of the following will be set, the other will remain set to -1.
    // Index of the mesh vertex.
    int point_id;
    // Index of the mesh edge.
    int edge_id;
};

class IntersectionPoint : public Point, public IntersectionReference
{
public:
    IntersectionPoint() {};
    IntersectionPoint(int point_id, int edge_id, const Point &pt) : IntersectionReference(point_id, edge_id), Point(pt) {}
    IntersectionPoint(const IntersectionReference &ir, const Point &pt) : IntersectionReference(ir), Point(pt) {}
    // Inherits coord_t x, y
};

class IntersectionLine : public Line
{
public:
    IntersectionLine() : a_id(-1), b_id(-1), edge_a_id(-1), edge_b_id(-1), edge_type(feGeneral), flags(0) {}

    bool skip() const { return (this->flags & SKIP) != 0; }
    void set_skip() { this->flags |= SKIP; }

    bool is_seed_candidate() const { return (this->flags & NO_SEED) == 0 && ! this->skip(); }
    void set_no_seed(bool set) { if (set) this->flags |= NO_SEED; else this->flags &= ~NO_SEED; }
    
    // Inherits Point a, b
    // For each line end point, either {a,b}_id or {a,b}edge_a_id is set, the other is left to -1.
    // Vertex indices of the line end points.
    int             a_id;
    int             b_id;
    // Source mesh edges of the line end points.
    int             edge_a_id;
    int             edge_b_id;
    // feGeneral, feTop, feBottom, feHorizontal
    FacetEdgeType   edge_type;
    // Used by TriangleMeshSlicer::slice() to skip duplicate edges.
    enum {
        // Triangle edge added, because it has no neighbor.
        EDGE0_NO_NEIGHBOR   = 0x001,
        EDGE1_NO_NEIGHBOR   = 0x002,
        EDGE2_NO_NEIGHBOR   = 0x004,
        // Triangle edge added, because it makes a fold with another horizontal edge.
        EDGE0_FOLD          = 0x010,
        EDGE1_FOLD          = 0x020,
        EDGE2_FOLD          = 0x040,
        // The edge cannot be a seed of a greedy loop extraction (folds are not safe to become seeds).
        NO_SEED             = 0x100,
        SKIP                = 0x200,
    };
    uint32_t        flags;
};
typedef std::vector<IntersectionLine> IntersectionLines;
typedef std::vector<IntersectionLine*> IntersectionLinePtrs;

class TriangleMeshSlicer
{
public:
    TriangleMeshSlicer(TriangleMesh* _mesh);
    void slice(const std::vector<float> &z, std::vector<Polygons>* layers) const;
    void slice(const std::vector<float> &z, std::vector<ExPolygons>* layers) const;
    enum FacetSliceType {
        NoSlice = 0,
        Slicing = 1,
        Cutting = 2
    };
    FacetSliceType slice_facet(float slice_z, const stl_facet &facet, const int facet_idx,
        const float min_z, const float max_z, IntersectionLine *line_out) const;
    void cut(float z, TriangleMesh* upper, TriangleMesh* lower) const;
    
    double safety_offset = scale_(0.0499);
private:
    const TriangleMesh      *mesh;
    // Map from a facet to an edge index.
    std::vector<int>         facets_edges;
    // Scaled copy of this->mesh->stl.v_shared
    std::vector<stl_vertex>  v_scaled_shared;

    void _slice_do(size_t facet_idx, std::vector<IntersectionLines>* lines, boost::mutex* lines_mutex, const std::vector<float> &z) const;
    void make_loops(std::vector<IntersectionLine> &lines, Polygons* loops) const;
    void make_expolygons(const Polygons &loops, ExPolygons* slices) const;
    void make_expolygons_simple(std::vector<IntersectionLine> &lines, ExPolygons* slices) const;
    void make_expolygons(std::vector<IntersectionLine> &lines, ExPolygons* slices) const;
};

TriangleMesh make_cube(double x, double y, double z);

// Generate a TriangleMesh of a cylinder
TriangleMesh make_cylinder(double r, double h, double fa=(2*PI/360));

TriangleMesh make_sphere(double rho, double fa=(2*PI/360));

}

#endif
