// Copyright (C) 2007-2025  CEA, EDF, OPEN CASCADE
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
//
// See http://www.salome-platform.org/ or email : webmaster.salome@opencascade.com
//

// File      : SMDS_MeshInfo.hxx
// Created   : Mon Sep 24 18:32:41 2007
// Author    : Edward AGAPOV (eap)
//
#ifndef SMDS_MeshInfo_HeaderFile
#define SMDS_MeshInfo_HeaderFile

#include <vector>

#include "SMESH_SMDS.hxx"

#include "SMDS_MeshElement.hxx"
#include<utilities.h>
#include <smIdType.hxx>

class SMDS_EXPORT SMDS_MeshInfo
{
public:

  inline SMDS_MeshInfo();
  inline SMDS_MeshInfo& operator=(const SMDS_MeshInfo& other);
  inline void Clear();

  inline smIdType NbElements(SMDSAbs_ElementType  type=SMDSAbs_All) const;
  inline smIdType NbElements(SMDSAbs_EntityType   type) const { return NbEntities(type); }
  inline smIdType NbElements(SMDSAbs_GeometryType type) const { return NbElementsOfGeom(type); }

  inline smIdType NbEntities(SMDSAbs_EntityType  type) const;
  inline smIdType NbElementsOfGeom(SMDSAbs_GeometryType geom) const;

  smIdType NbNodes()      const { return myNbNodes; }
  smIdType Nb0DElements() const { return myNb0DElements; }
  smIdType NbBalls()      const { return myNbBalls; }
  inline smIdType NbEdges      (SMDSAbs_ElementOrder order = ORDER_ANY) const;

  inline smIdType NbFaces      (SMDSAbs_ElementOrder order = ORDER_ANY) const;
  inline smIdType NbTriangles  (SMDSAbs_ElementOrder order = ORDER_ANY) const;
  inline smIdType NbQuadrangles(SMDSAbs_ElementOrder order = ORDER_ANY) const;
  smIdType NbBiQuadTriangles() const { return myNbBiQuadTriangles; }
  smIdType NbBiQuadQuadrangles() const { return myNbBiQuadQuadrangles; }
  inline smIdType NbPolygons(SMDSAbs_ElementOrder order = ORDER_ANY) const;

  inline smIdType NbVolumes (SMDSAbs_ElementOrder order = ORDER_ANY) const;
  inline smIdType NbTetras  (SMDSAbs_ElementOrder order = ORDER_ANY) const;
  inline smIdType NbHexas   (SMDSAbs_ElementOrder order = ORDER_ANY) const;
  inline smIdType NbPyramids(SMDSAbs_ElementOrder order = ORDER_ANY) const;
  inline smIdType NbPrisms  (SMDSAbs_ElementOrder order = ORDER_ANY) const;
  inline smIdType NbHexPrisms(SMDSAbs_ElementOrder order = ORDER_ANY) const;
  smIdType NbTriQuadHexas() const { return myNbTriQuadHexas; }
  smIdType NbQuadPrisms() const { return myNbQuadPrisms; }
  smIdType NbBiQuadPrisms() const { return myNbBiQuadPrisms; }
  smIdType NbPolyhedrons() const { return myNbPolyhedrons; }

protected:
  inline void addWithPoly(const SMDS_MeshElement* el);
  inline void setNb(const SMDSAbs_EntityType geomType, const smIdType nb);

private:
  friend class SMDS_Mesh;

  // methods to count NOT POLY elements
  inline void remove(const SMDS_MeshElement* el);
  inline void add   (const SMDS_MeshElement* el);
  inline smIdType  index(SMDSAbs_ElementType type, int nbNodes) const;
  // methods to remove elements of ANY kind
  inline void RemoveEdge(const SMDS_MeshElement* el);
  inline void RemoveFace(const SMDS_MeshElement* el);
  inline void RemoveVolume(const SMDS_MeshElement* el);

  smIdType myNbNodes;

  smIdType myNb0DElements;
  smIdType myNbBalls;
  smIdType myNbEdges      , myNbQuadEdges      ;
  smIdType myNbTriangles  , myNbQuadTriangles,   myNbBiQuadTriangles  ;
  smIdType myNbQuadrangles, myNbQuadQuadrangles, myNbBiQuadQuadrangles;
  smIdType myNbPolygons   , myNbQuadPolygons;

  smIdType myNbTetras  , myNbQuadTetras  ;
  smIdType myNbHexas   , myNbQuadHexas,    myNbTriQuadHexas;
  smIdType myNbPyramids, myNbQuadPyramids;
  smIdType myNbPrisms  , myNbQuadPrisms,   myNbBiQuadPrisms;
  smIdType myNbHexPrism;
  smIdType myNbPolyhedrons;

  std::vector<smIdType*> myNb; // pointers to myNb... fields
  std::vector<int>  myShift; // shift to get an index in myNb by elem->NbNodes()
};

inline SMDS_MeshInfo::SMDS_MeshInfo():
  myNbNodes      (0),
  myNb0DElements (0),
  myNbBalls      (0),
  myNbEdges      (0), myNbQuadEdges      (0),
  myNbTriangles  (0), myNbQuadTriangles  (0), myNbBiQuadTriangles(0),
  myNbQuadrangles(0), myNbQuadQuadrangles(0), myNbBiQuadQuadrangles(0),
  myNbPolygons   (0), myNbQuadPolygons   (0),
  myNbTetras     (0), myNbQuadTetras  (0),
  myNbHexas      (0), myNbQuadHexas   (0), myNbTriQuadHexas(0),
  myNbPyramids   (0), myNbQuadPyramids(0),
  myNbPrisms     (0), myNbQuadPrisms  (0), myNbBiQuadPrisms(0),
  myNbHexPrism   (0),
  myNbPolyhedrons(0)
{
  // Number of nodes in standard element types (. - actual nb, * - after the shift)
  // n   v  f  e  0  n b
  // o   o  a  d  d  o a
  // d   l  c  g     d l
  // e      e  e     e l
  // s
  // ====================
  // 0 ------------------  - DON't USE 0!!!
  // 1            .  * .
  // 2         .       *
  // 3      .  .  *
  // 4   *  .
  // 5   *
  // 6   *  .
  // 7      .
  // 8   *  .
  // 9      .
  // 10  *
  // 11
  // 12  *
  // 13  *
  // 14
  // 15  *
  // 16        *
  // 17        *
  // 18  *
  // 19
  // 20  *   
  // 21
  // 22
  // 23
  // 24
  // 25     *
  // 26     *
  // 27  *
  // 28     *
  // 29     *
  // 30     *
  // 31     *
  //
  // So to have a unique index for each type basing on nb of nodes, we use a shift:
  myShift.resize(SMDSAbs_NbElementTypes, 0);

  myShift[ SMDSAbs_Face      ] = +22;// 3->25, 4->26, etc.
  myShift[ SMDSAbs_Edge      ] = +14;// 2->16, 3->17
  myShift[ SMDSAbs_0DElement ] = +2; // 1->3
  myShift[ SMDSAbs_Ball      ] = +1; // 1->2

  myNb.resize( index( SMDSAbs_Face,9 ) + 1, NULL);

  myNb[ index( SMDSAbs_Node,1 )] = & myNbNodes;
  myNb[ index( SMDSAbs_0DElement,1 )] = & myNb0DElements;
  myNb[ index( SMDSAbs_Ball,1 )] = & myNbBalls;

  myNb[ index( SMDSAbs_Edge,2 )] = & myNbEdges;
  myNb[ index( SMDSAbs_Edge,3 )] = & myNbQuadEdges;

  myNb[ index( SMDSAbs_Face,3 )] = & myNbTriangles;
  myNb[ index( SMDSAbs_Face,4 )] = & myNbQuadrangles;
  myNb[ index( SMDSAbs_Face,6 )] = & myNbQuadTriangles;
  myNb[ index( SMDSAbs_Face,7 )] = & myNbBiQuadTriangles;
  myNb[ index( SMDSAbs_Face,8 )] = & myNbQuadQuadrangles;
  myNb[ index( SMDSAbs_Face,9 )] = & myNbBiQuadQuadrangles;

  myNb[ index( SMDSAbs_Volume, 4)]  = & myNbTetras;
  myNb[ index( SMDSAbs_Volume, 5)]  = & myNbPyramids;
  myNb[ index( SMDSAbs_Volume, 6)]  = & myNbPrisms;
  myNb[ index( SMDSAbs_Volume, 8)]  = & myNbHexas;
  myNb[ index( SMDSAbs_Volume, 10)] = & myNbQuadTetras;  
  myNb[ index( SMDSAbs_Volume, 12)] = & myNbHexPrism;
  myNb[ index( SMDSAbs_Volume, 13)] = & myNbQuadPyramids;
  myNb[ index( SMDSAbs_Volume, 15)] = & myNbQuadPrisms;  
  myNb[ index( SMDSAbs_Volume, 18)] = & myNbBiQuadPrisms;
  myNb[ index( SMDSAbs_Volume, 20)] = & myNbQuadHexas;   
  myNb[ index( SMDSAbs_Volume, 27)] = & myNbTriQuadHexas;   
}

inline SMDS_MeshInfo& // operator=
SMDS_MeshInfo::operator=(const SMDS_MeshInfo& other)
{ for ( size_t i=0; i<myNb.size(); ++i ) if ( myNb[i] ) (*myNb[i])=(*other.myNb[i]);
  myNbPolygons     = other.myNbPolygons;
  myNbQuadPolygons = other.myNbQuadPolygons;
  myNbPolyhedrons  = other.myNbPolyhedrons;
  return *this;
}

inline void // Clear
SMDS_MeshInfo::Clear()
{ for ( size_t i=0; i<myNb.size(); ++i ) if ( myNb[i] ) (*myNb[i])=0;
  myNbPolygons=myNbQuadPolygons=myNbPolyhedrons=0;
}

inline smIdType // index
SMDS_MeshInfo::index(SMDSAbs_ElementType type, int nbNodes) const
{ return nbNodes + myShift[ type ]; }

inline void // remove
SMDS_MeshInfo::remove(const SMDS_MeshElement* el)
{ --(*myNb[ index(el->GetType(), el->NbNodes()) ]); }

inline void // add
SMDS_MeshInfo::add(const SMDS_MeshElement* el)
{ ++(*myNb[ index(el->GetType(), el->NbNodes()) ]); }

inline void // addWithPoly
SMDS_MeshInfo::addWithPoly(const SMDS_MeshElement* el) {
  switch ( el->GetEntityType() ) {
  case SMDSEntity_Polygon:      ++myNbPolygons; break;
  case SMDSEntity_Quad_Polygon: ++myNbQuadPolygons; break;
  case SMDSEntity_Polyhedra:    ++myNbPolyhedrons; break;
  default:                      add(el);
  }
}
inline void // RemoveEdge
SMDS_MeshInfo::RemoveEdge(const SMDS_MeshElement* el)
{ if ( el->IsQuadratic() ) --myNbQuadEdges; else --myNbEdges; }

inline void // RemoveFace
SMDS_MeshInfo::RemoveFace(const SMDS_MeshElement* el) {
  switch ( el->GetEntityType() ) {
  case SMDSEntity_Polygon:      --myNbPolygons; break;
  case SMDSEntity_Quad_Polygon: --myNbQuadPolygons; break;
  default:                      remove(el);
  }
}

inline void // RemoveVolume
SMDS_MeshInfo::RemoveVolume(const SMDS_MeshElement* el)
{ if ( el->IsPoly() ) --myNbPolyhedrons; else remove( el ); }

inline smIdType  // NbEdges
SMDS_MeshInfo::NbEdges      (SMDSAbs_ElementOrder order) const
{ return order == ORDER_ANY ? myNbEdges+myNbQuadEdges : order == ORDER_LINEAR ? myNbEdges : myNbQuadEdges; }

inline smIdType  // NbFaces
SMDS_MeshInfo::NbFaces      (SMDSAbs_ElementOrder order) const
{ return NbTriangles(order)+NbQuadrangles(order)+(order == ORDER_ANY ? myNbPolygons+myNbQuadPolygons : order == ORDER_LINEAR ? myNbPolygons : myNbQuadPolygons ); }

inline smIdType  // NbTriangles
SMDS_MeshInfo::NbTriangles  (SMDSAbs_ElementOrder order) const
{ return order == ORDER_ANY ? myNbTriangles+myNbQuadTriangles+myNbBiQuadTriangles : order == ORDER_LINEAR ? myNbTriangles : myNbQuadTriangles+myNbBiQuadTriangles; }

inline smIdType  // NbQuadrangles
SMDS_MeshInfo::NbQuadrangles(SMDSAbs_ElementOrder order) const
{ return order == ORDER_ANY ? myNbQuadrangles+myNbQuadQuadrangles+myNbBiQuadQuadrangles : order == ORDER_LINEAR ? myNbQuadrangles : myNbQuadQuadrangles+myNbBiQuadQuadrangles; }

inline smIdType  // NbPolygons
SMDS_MeshInfo::NbPolygons(SMDSAbs_ElementOrder order) const
{ return order == ORDER_ANY ? myNbPolygons+myNbQuadPolygons : order == ORDER_LINEAR ? myNbPolygons : myNbQuadPolygons; }

inline smIdType  // NbVolumes
SMDS_MeshInfo::NbVolumes (SMDSAbs_ElementOrder order) const
{ return NbTetras(order) + NbHexas(order) + NbPyramids(order) + NbPrisms(order) + NbHexPrisms(order) + (order == ORDER_QUADRATIC ? 0 : myNbPolyhedrons); }

inline smIdType  // NbTetras
SMDS_MeshInfo::NbTetras  (SMDSAbs_ElementOrder order) const
{ return order == ORDER_ANY ? myNbTetras+myNbQuadTetras : order == ORDER_LINEAR ? myNbTetras : myNbQuadTetras; }

inline smIdType  // NbHexas
SMDS_MeshInfo::NbHexas   (SMDSAbs_ElementOrder order) const
{ return order == ORDER_ANY ? myNbHexas+myNbQuadHexas+myNbTriQuadHexas : order == ORDER_LINEAR ? myNbHexas : myNbQuadHexas+myNbTriQuadHexas; }

inline smIdType  // NbPyramids
SMDS_MeshInfo::NbPyramids(SMDSAbs_ElementOrder order) const
{ return order == ORDER_ANY ? myNbPyramids+myNbQuadPyramids : order == ORDER_LINEAR ? myNbPyramids : myNbQuadPyramids; }

inline smIdType  // NbPrisms
SMDS_MeshInfo::NbPrisms  (SMDSAbs_ElementOrder order) const
{ return order == ORDER_ANY ? myNbPrisms+myNbQuadPrisms+myNbBiQuadPrisms: order == ORDER_LINEAR ? myNbPrisms : myNbQuadPrisms+myNbBiQuadPrisms; }

inline smIdType  // NbHexPrisms
SMDS_MeshInfo::NbHexPrisms  (SMDSAbs_ElementOrder order) const
{ return order == ORDER_ANY ? myNbHexPrism : order == ORDER_LINEAR ? myNbHexPrism : 0; }

inline smIdType  // NbElements
SMDS_MeshInfo::NbElements(SMDSAbs_ElementType type) const
{ 
  smIdType nb = 0;
  switch (type) {
  case SMDSAbs_All:
    for ( size_t i=1+index( SMDSAbs_Node,1 ); i<myNb.size(); ++i ) if ( myNb[i] ) nb += *myNb[i];
    nb += myNbPolygons + myNbQuadPolygons + myNbPolyhedrons;
    break;
  case SMDSAbs_Volume:
    nb = ( myNbTetras+     myNbPyramids+     myNbPrisms+     myNbHexas+     myNbHexPrism+
           myNbQuadTetras+ myNbQuadPyramids+ myNbQuadPrisms+ myNbBiQuadPrisms + myNbQuadHexas+ myNbTriQuadHexas+
           myNbPolyhedrons );
    break;
  case SMDSAbs_Face:
    nb = ( myNbTriangles+       myNbQuadrangles+
           myNbQuadTriangles+   myNbBiQuadTriangles+
           myNbQuadQuadrangles+ myNbBiQuadQuadrangles+ myNbPolygons+ myNbQuadPolygons );
    break;
  case SMDSAbs_Edge:
    nb = myNbEdges + myNbQuadEdges;
    break;
  case SMDSAbs_Node:
    nb = myNbNodes;
    break;
  case SMDSAbs_0DElement:
    nb = myNb0DElements;
    break;
  case SMDSAbs_Ball:
    nb = myNbBalls;
    break;
  default:;
  }
  return nb;
}

inline smIdType  // NbEntities
SMDS_MeshInfo::NbEntities(SMDSAbs_EntityType type) const
{
  switch (type) {
  case SMDSEntity_Node:             return myNbNodes;
  case SMDSEntity_Edge:             return myNbEdges;
  case SMDSEntity_Quad_Edge:        return myNbQuadEdges;
  case SMDSEntity_Triangle:         return myNbTriangles;
  case SMDSEntity_Quad_Triangle:    return myNbQuadTriangles;
  case SMDSEntity_BiQuad_Triangle:  return myNbBiQuadTriangles;
  case SMDSEntity_Quadrangle:       return myNbQuadrangles;
  case SMDSEntity_Quad_Quadrangle:  return myNbQuadQuadrangles;
  case SMDSEntity_BiQuad_Quadrangle:return myNbBiQuadQuadrangles;
  case SMDSEntity_Polygon:          return myNbPolygons;
  case SMDSEntity_Tetra:            return myNbTetras;
  case SMDSEntity_Quad_Tetra:       return myNbQuadTetras;
  case SMDSEntity_Pyramid:          return myNbPyramids;
  case SMDSEntity_Quad_Pyramid:     return myNbQuadPyramids;
  case SMDSEntity_Hexa:             return myNbHexas;
  case SMDSEntity_Quad_Hexa:        return myNbQuadHexas;
  case SMDSEntity_TriQuad_Hexa:     return myNbTriQuadHexas;
  case SMDSEntity_Penta:            return myNbPrisms;
  case SMDSEntity_Quad_Penta:       return myNbQuadPrisms;
  case SMDSEntity_BiQuad_Penta:     return myNbBiQuadPrisms;
  case SMDSEntity_Hexagonal_Prism:  return myNbHexPrism;
  case SMDSEntity_Polyhedra:        return myNbPolyhedrons;
  case SMDSEntity_0D:               return myNb0DElements;
  case SMDSEntity_Ball:             return myNbBalls;
  case SMDSEntity_Quad_Polygon:     return myNbQuadPolygons;
  case SMDSEntity_Quad_Polyhedra:
  case SMDSEntity_Last:
    break;
  }
  return 0;
}

inline smIdType  // NbElementsOfGeom
SMDS_MeshInfo::NbElementsOfGeom(SMDSAbs_GeometryType geom) const
{
  switch ( geom ) {
    // 0D:
  case SMDSGeom_POINT:           return myNb0DElements;
    // 1D:
  case SMDSGeom_EDGE:            return (myNbEdges +
                                         myNbQuadEdges);
    // 2D:
  case SMDSGeom_TRIANGLE:        return (myNbTriangles +
                                         myNbQuadTriangles +
                                         myNbBiQuadTriangles );
  case SMDSGeom_QUADRANGLE:      return (myNbQuadrangles +
                                         myNbQuadQuadrangles +
                                         myNbBiQuadQuadrangles );
  case SMDSGeom_POLYGON:         return (myNbPolygons + myNbQuadPolygons );
    // 3D:
  case SMDSGeom_TETRA:           return (myNbTetras +
                                         myNbQuadTetras);
  case SMDSGeom_PYRAMID:         return (myNbPyramids +
                                         myNbQuadPyramids);
  case SMDSGeom_HEXA:            return (myNbHexas +
                                         myNbQuadHexas +
                                         myNbTriQuadHexas);
  case SMDSGeom_PENTA:           return (myNbPrisms +
                                         myNbQuadPrisms +
                                         myNbBiQuadPrisms);
  case SMDSGeom_HEXAGONAL_PRISM: return myNbHexPrism;
  case SMDSGeom_POLYHEDRA:       return myNbPolyhedrons;
    // Discrete:
  case SMDSGeom_BALL:            return myNbBalls;
    //
  case SMDSGeom_NONE:
  default:;
  }
  return 0;
}

inline void // setNb
SMDS_MeshInfo::setNb(const SMDSAbs_EntityType geomType, const smIdType nb)
{
  switch (geomType) {
  case SMDSEntity_Node:             myNbNodes             = nb; break;
  case SMDSEntity_0D:               myNb0DElements        = nb; break;
  case SMDSEntity_Ball:             myNbBalls             = nb; break;
  case SMDSEntity_BiQuad_Quadrangle:myNbBiQuadQuadrangles = nb; break;
  case SMDSEntity_BiQuad_Triangle:  myNbBiQuadTriangles   = nb; break;
  case SMDSEntity_Edge:             myNbEdges             = nb; break;
  case SMDSEntity_Hexa:             myNbHexas             = nb; break;
  case SMDSEntity_Hexagonal_Prism:  myNbHexPrism          = nb; break;
  case SMDSEntity_Penta:            myNbPrisms            = nb; break;
  case SMDSEntity_Polygon:          myNbPolygons          = nb; break;
  case SMDSEntity_Polyhedra:        myNbPolyhedrons       = nb; break;
  case SMDSEntity_Pyramid:          myNbPyramids          = nb; break;
  case SMDSEntity_Quad_Edge:        myNbQuadEdges         = nb; break;
  case SMDSEntity_Quad_Hexa:        myNbQuadHexas         = nb; break;
  case SMDSEntity_Quad_Penta:       myNbQuadPrisms        = nb; break;
  case SMDSEntity_BiQuad_Penta:     myNbBiQuadPrisms      = nb; break;
  case SMDSEntity_Quad_Pyramid:     myNbQuadPyramids      = nb; break;
  case SMDSEntity_Quad_Quadrangle:  myNbQuadQuadrangles   = nb; break;
  case SMDSEntity_Quad_Tetra:       myNbQuadTetras        = nb; break;
  case SMDSEntity_Quad_Triangle:    myNbQuadTriangles     = nb; break;
  case SMDSEntity_Quadrangle:       myNbQuadrangles       = nb; break;
  case SMDSEntity_Tetra:            myNbTetras            = nb; break;
  case SMDSEntity_TriQuad_Hexa:     myNbTriQuadHexas      = nb; break;
  case SMDSEntity_Triangle:         myNbTriangles         = nb; break;
  case SMDSEntity_Quad_Polygon:     myNbQuadPolygons      = nb; break;
  case SMDSEntity_Quad_Polyhedra:
  case SMDSEntity_Last:
    break;
  }
}

#endif
