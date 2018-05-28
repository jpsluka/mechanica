/*
 * MxMeshCore.cpp
 *
 *  Created on: Oct 3, 2017
 *      Author: andy
 */

#include "MxMeshCore.h"
#include "MxMesh.h"
#include "MxTriangle.h"
#include "MxDebug.h"

float MxVertex::minForceDivergence;
float MxVertex::maxForceDivergence;

struct MxVertexType : MxType {

};

static MxVertexType type;

MxType *MxVertex_Type = &type;

MxVertex::MxVertex(MxType* derivedType) : MxObject{derivedType}
{
}

std::set<VertexPtr> MxVertex::link() const {
    std::set<VertexPtr> lnk;

    for(TrianglePtr tri : _triangles) {
        for(VertexPtr v : tri->vertices) {
            if(v != this) {lnk.insert(v);}
        }
    }
    return lnk;
}

MxVertex::MxVertex() : MxVertex{MxVertex_Type}
{
}

MxVertex::MxVertex(float mass, float area, const Magnum::Vector3 &pos) :
        MxObject{MxVertex_Type},
        mass{mass}, area{area}, position{pos} {
};

HRESULT MxVertex::removeTriangle(const TrianglePtr tri) {
    auto iter = std::find(_triangles.begin(), _triangles.end(), tri);
    MeshPtr m = mesh(); assert(m);

    if(iter != _triangles.end()) {
        _triangles.erase(iter);
        rebuildCells();
        m->valenceChanged(this);
        return S_OK;
    }
    return E_FAIL;
}

HRESULT MxVertex::appendTriangle(TrianglePtr tri) {
    if(!contains(_triangles, tri)) {
        _triangles.push_back(tri);
        rebuildCells();
        MeshPtr m = mesh();
        if(m) m->valenceChanged(this);
        return S_OK;
    }
    return E_FAIL;
}

void MxVertex::rebuildCells() {
    _cells.clear();
    for(TrianglePtr tri : _triangles) {
        if(tri->cells[0] && !contains(_cells, tri->cells[0])) {
            _cells.push_back(tri->cells[0]);
        }
        if(tri->cells[1] && !contains(_cells, tri->cells[1])) {
            _cells.push_back(tri->cells[1]);
        }
    }
}

TrianglePtr MxVertex::triangleForCell(CCellPtr cell) const {
    for(TrianglePtr tri : _triangles) {
        if(tri->cells[0] == cell || tri->cells[1] == cell) {
            return tri;
        }
    }
    return nullptr;
}

std::ostream& operator <<(std::ostream& os, CVertexPtr v)
{
    os << "{id:" << v->id << ", pos:" << v->position << "}";
    return os;
}

MeshPtr MxVertex::mesh()
{
    if(_cells.size()) {
        CellPtr cell = _cells[0];
        return cell->mesh;
    }
    return nullptr;
}

HRESULT MxVertex::triangleCellChanged(TrianglePtr tri) {

    MeshPtr m = mesh();

    rebuildCells();

    if(!m) {
        m = mesh();
    }

    if(m) m->valenceChanged(this);

    return S_OK;
}

Magnum::Vector3 MxVertex::areaWeightedNormal(CCellPtr cell) const
{
    Vector3 result;

    for(TrianglePtr tri : _triangles) {
        if(tri->cells[0] == cell) {
            result +=  1. / 3. * tri->area * tri->normal;
        }
        else if(tri->cells[1] == cell) {
            result += -1. / 3. * tri->area * tri->normal;
        }
    }
    return result;
}


