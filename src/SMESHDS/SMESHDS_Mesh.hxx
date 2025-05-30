// Copyright (C) 2007-2025  CEA, EDF, OPEN CASCADE
//
// Copyright (C) 2003-2007  OPEN CASCADE, EADS/CCR, LIP6, CEA/DEN,
// CEDRAT, EDF R&D, LEG, PRINCIPIA R&D, BUREAU VERITAS
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

//  SMESH SMESHDS : management of mesh data and SMESH document
//  File   : SMESHDS_Mesh.hxx
//  Module : SMESH
//
#ifndef _SMESHDS_Mesh_HeaderFile
#define _SMESHDS_Mesh_HeaderFile

#include "SMESH_SMESHDS.hxx"

#include "SMDS_Mesh.hxx"
#include "SMESH_Utils.hxx"
#include "SMESH_RegularGrid.hxx"
#include "SMESHDS_SubMesh.hxx"

#include <Basics_OCCTVersion.hxx>

#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Shape.hxx>

#include <map>

class TopoDS_Solid ;
class TopoDS_Shell ;
class TopoDS_Face  ;
class TopoDS_Vertex;
class TopoDS_Edge  ;

class SMESHDS_Script;
class SMESHDS_Hypothesis;
class SMDS_MeshNode     ;
class SMDS_MeshEdge     ;
class SMDS_MeshFace     ;
class SMDS_MeshVolume   ;
class SMDS_Mesh0DElement;
class SMDS_BallElement;

/*
 * Using of native hash_map isn't portable and don't work on WIN32 platform.
 * So this functionality implement on new NCollection_DataMap technology
 */
#include <NCollection_DataMap.hxx>
typedef std::list<const SMESHDS_Hypothesis*>                          THypList;

struct SMESHDS_Hasher
{
#if OCC_VERSION_LARGE < 0x07080000
  static inline Standard_Boolean IsEqual(const TopoDS_Shape& S1,
                                         const TopoDS_Shape& S2)
  {
    return S1.IsSame(S2);
  }
  static inline Standard_Integer HashCode(const TopoDS_Shape& S,
                                          const Standard_Integer Upper)
  {
    return ::HashCode( S, Upper);
  }
#else
  bool operator()(const TopoDS_Shape& S1, const TopoDS_Shape& S2) const
  {
    // for the purpose of ShapeToHypothesis map we don't consider shapes orientation
    return S1.IsSame(S2);
  }
  size_t operator()(const TopoDS_Shape& S) const
  {
    return std::hash<TopoDS_Shape>{}(S);
  }
#endif
};

typedef NCollection_DataMap< TopoDS_Shape, THypList, SMESHDS_Hasher > ShapeToHypothesis;

class SMESHDS_GroupBase;
class DownIdType;

class SMESHDS_EXPORT SMESHDS_Mesh : public SMDS_Mesh
{
 public:
  SMESHDS_Mesh(int theMeshID, bool theIsEmbeddedMode);
  bool IsEmbeddedMode();
  void SetPersistentId(int id);
  int GetPersistentId() const;

  void ShapeToMesh(const TopoDS_Shape & S);
  TopoDS_Shape ShapeToMesh() const;
  bool AddHypothesis(const TopoDS_Shape & SS, const SMESHDS_Hypothesis * H);
  bool RemoveHypothesis(const TopoDS_Shape & S, const SMESHDS_Hypothesis * H);
  
  virtual SMDS_MeshNode* AddNodeWithID(double x, double y, double z, smIdType ID);
  virtual SMDS_MeshNode* AddNode(double x, double y, double z);
  
  virtual SMDS_Mesh0DElement* Add0DElementWithID(smIdType nodeID, smIdType ID);
  virtual SMDS_Mesh0DElement* Add0DElementWithID(const SMDS_MeshNode * node, smIdType ID);
  virtual SMDS_Mesh0DElement* Add0DElement      (const SMDS_MeshNode * node);
  
  virtual SMDS_BallElement* AddBallWithID(smIdType n,                   double diameter, smIdType ID);
  virtual SMDS_BallElement* AddBallWithID(const SMDS_MeshNode * n, double diameter, smIdType ID);
  virtual SMDS_BallElement* AddBall      (const SMDS_MeshNode * n, double diameter);

  virtual SMDS_MeshEdge* AddEdgeWithID(smIdType n1, smIdType n2, smIdType ID);
  virtual SMDS_MeshEdge* AddEdgeWithID(const SMDS_MeshNode * n1,
                                       const SMDS_MeshNode * n2, 
                                       smIdType ID);
  virtual SMDS_MeshEdge* AddEdge(const SMDS_MeshNode * n1,
                                 const SMDS_MeshNode * n2);
  
  // 2d order edge with 3 nodes: n12 - node between n1 and n2
  virtual SMDS_MeshEdge* AddEdgeWithID(smIdType n1, smIdType n2, smIdType n12, smIdType ID);
  virtual SMDS_MeshEdge* AddEdgeWithID(const SMDS_MeshNode * n1,
                                       const SMDS_MeshNode * n2, 
                                       const SMDS_MeshNode * n12, 
                                       smIdType ID);
  virtual SMDS_MeshEdge* AddEdge(const SMDS_MeshNode * n1,
                                 const SMDS_MeshNode * n2,
                                 const SMDS_MeshNode * n12);
  // tria 3
  virtual SMDS_MeshFace* AddFaceWithID(smIdType n1, smIdType n2, smIdType n3, smIdType ID);
  virtual SMDS_MeshFace* AddFaceWithID(const SMDS_MeshNode * n1,
                                       const SMDS_MeshNode * n2,
                                       const SMDS_MeshNode * n3, 
                                       smIdType ID);
  virtual SMDS_MeshFace* AddFace(const SMDS_MeshNode * n1,
                                 const SMDS_MeshNode * n2,
                                 const SMDS_MeshNode * n3);
  // quad 4
  virtual SMDS_MeshFace* AddFaceWithID(smIdType n1, smIdType n2, smIdType n3, smIdType n4, smIdType ID);
  virtual SMDS_MeshFace* AddFaceWithID(const SMDS_MeshNode * n1,
                                       const SMDS_MeshNode * n2,
                                       const SMDS_MeshNode * n3,
                                       const SMDS_MeshNode * n4, 
                                       smIdType ID);
  virtual SMDS_MeshFace* AddFace(const SMDS_MeshNode * n1,
                                 const SMDS_MeshNode * n2,
                                 const SMDS_MeshNode * n3,
                                 const SMDS_MeshNode * n4);

  // 2d order triangle of 6 nodes
  virtual SMDS_MeshFace* AddFaceWithID(smIdType n1, smIdType n2, smIdType n3,
                                       smIdType n12,smIdType n23,smIdType n31, smIdType ID);
  virtual SMDS_MeshFace* AddFaceWithID(const SMDS_MeshNode * n1,
                                       const SMDS_MeshNode * n2,
                                       const SMDS_MeshNode * n3, 
                                       const SMDS_MeshNode * n12,
                                       const SMDS_MeshNode * n23,
                                       const SMDS_MeshNode * n31, 
                                       smIdType ID);
  virtual SMDS_MeshFace* AddFace(const SMDS_MeshNode * n1,
                                 const SMDS_MeshNode * n2,
                                 const SMDS_MeshNode * n3,
                                 const SMDS_MeshNode * n12,
                                 const SMDS_MeshNode * n23,
                                 const SMDS_MeshNode * n31);

  // biquadratic triangle of 7 nodes
  virtual SMDS_MeshFace* AddFaceWithID(smIdType n1, smIdType n2, smIdType n3,
                                       smIdType n12,smIdType n23,smIdType n31, smIdType nCenter, smIdType ID);
  virtual SMDS_MeshFace* AddFaceWithID(const SMDS_MeshNode * n1,
                                       const SMDS_MeshNode * n2,
                                       const SMDS_MeshNode * n3, 
                                       const SMDS_MeshNode * n12,
                                       const SMDS_MeshNode * n23,
                                       const SMDS_MeshNode * n31,
                                       const SMDS_MeshNode * nCenter, 
                                       smIdType ID);
  virtual SMDS_MeshFace* AddFace(const SMDS_MeshNode * n1,
                                 const SMDS_MeshNode * n2,
                                 const SMDS_MeshNode * n3,
                                 const SMDS_MeshNode * n12,
                                 const SMDS_MeshNode * n23,
                                 const SMDS_MeshNode * n31,
                                 const SMDS_MeshNode * nCenter);

  // 2d order quadrangle
  virtual SMDS_MeshFace* AddFaceWithID(smIdType n1, smIdType n2, smIdType n3, smIdType n4,
                                       smIdType n12,smIdType n23,smIdType n34,smIdType n41, smIdType ID);
  virtual SMDS_MeshFace* AddFaceWithID(const SMDS_MeshNode * n1,
                                       const SMDS_MeshNode * n2,
                                       const SMDS_MeshNode * n3,
                                       const SMDS_MeshNode * n4, 
                                       const SMDS_MeshNode * n12,
                                       const SMDS_MeshNode * n23,
                                       const SMDS_MeshNode * n34,
                                       const SMDS_MeshNode * n41, 
                                       smIdType ID);
  virtual SMDS_MeshFace* AddFace(const SMDS_MeshNode * n1,
                                 const SMDS_MeshNode * n2,
                                 const SMDS_MeshNode * n3,
                                 const SMDS_MeshNode * n4,
                                 const SMDS_MeshNode * n12,
                                 const SMDS_MeshNode * n23,
                                 const SMDS_MeshNode * n34,
                                 const SMDS_MeshNode * n41);

  // biquadratic quadrangle of 9 nodes
  virtual SMDS_MeshFace* AddFaceWithID(smIdType n1, smIdType n2, smIdType n3, smIdType n4,
                                       smIdType n12,smIdType n23,smIdType n34,smIdType n41, smIdType nCenter, smIdType ID);
  virtual SMDS_MeshFace* AddFaceWithID(const SMDS_MeshNode * n1,
                                       const SMDS_MeshNode * n2,
                                       const SMDS_MeshNode * n3,
                                       const SMDS_MeshNode * n4, 
                                       const SMDS_MeshNode * n12,
                                       const SMDS_MeshNode * n23,
                                       const SMDS_MeshNode * n34,
                                       const SMDS_MeshNode * n41, 
                                       const SMDS_MeshNode * nCenter, 
                                       smIdType ID);
  virtual SMDS_MeshFace* AddFace(const SMDS_MeshNode * n1,
                                 const SMDS_MeshNode * n2,
                                 const SMDS_MeshNode * n3,
                                 const SMDS_MeshNode * n4,
                                 const SMDS_MeshNode * n12,
                                 const SMDS_MeshNode * n23,
                                 const SMDS_MeshNode * n34,
                                 const SMDS_MeshNode * n41,
                                 const SMDS_MeshNode * nCenter);
  // tetra 4
  virtual SMDS_MeshVolume* AddVolumeWithID(smIdType n1, smIdType n2, smIdType n3, smIdType n4, smIdType ID);
  virtual SMDS_MeshVolume* AddVolumeWithID(const SMDS_MeshNode * n1,
                                           const SMDS_MeshNode * n2,
                                           const SMDS_MeshNode * n3,
                                           const SMDS_MeshNode * n4, 
                                           smIdType ID);
  virtual SMDS_MeshVolume* AddVolume(const SMDS_MeshNode * n1,
                                     const SMDS_MeshNode * n2,
                                     const SMDS_MeshNode * n3,
                                     const SMDS_MeshNode * n4);
  // pyra 5
  virtual SMDS_MeshVolume* AddVolumeWithID(smIdType n1, smIdType n2, smIdType n3, smIdType n4, smIdType n5, smIdType ID);
  virtual SMDS_MeshVolume* AddVolumeWithID(const SMDS_MeshNode * n1,
                                           const SMDS_MeshNode * n2,
                                           const SMDS_MeshNode * n3,
                                           const SMDS_MeshNode * n4,
                                           const SMDS_MeshNode * n5, 
                                           smIdType ID);
  virtual SMDS_MeshVolume* AddVolume(const SMDS_MeshNode * n1,
                                     const SMDS_MeshNode * n2,
                                     const SMDS_MeshNode * n3,
                                     const SMDS_MeshNode * n4,
                                     const SMDS_MeshNode * n5);
  // penta 6
  virtual SMDS_MeshVolume* AddVolumeWithID(smIdType n1, smIdType n2, smIdType n3, smIdType n4, smIdType n5, smIdType n6, smIdType ID);
  virtual SMDS_MeshVolume* AddVolumeWithID(const SMDS_MeshNode * n1,
                                           const SMDS_MeshNode * n2,
                                           const SMDS_MeshNode * n3,
                                           const SMDS_MeshNode * n4,
                                           const SMDS_MeshNode * n5,
                                           const SMDS_MeshNode * n6, 
                                           smIdType ID);
  virtual SMDS_MeshVolume* AddVolume(const SMDS_MeshNode * n1,
                                     const SMDS_MeshNode * n2,
                                     const SMDS_MeshNode * n3,
                                     const SMDS_MeshNode * n4,
                                     const SMDS_MeshNode * n5,
                                     const SMDS_MeshNode * n6);
  // hexa 8
  virtual SMDS_MeshVolume* AddVolumeWithID(smIdType n1, smIdType n2, smIdType n3, smIdType n4, smIdType n5, smIdType n6, smIdType n7, smIdType n8, smIdType ID);
  virtual SMDS_MeshVolume* AddVolumeWithID(const SMDS_MeshNode * n1,
                                           const SMDS_MeshNode * n2,
                                           const SMDS_MeshNode * n3,
                                           const SMDS_MeshNode * n4,
                                           const SMDS_MeshNode * n5,
                                           const SMDS_MeshNode * n6,
                                           const SMDS_MeshNode * n7,
                                           const SMDS_MeshNode * n8, 
                                           smIdType ID);
  virtual SMDS_MeshVolume* AddVolume(const SMDS_MeshNode * n1,
                                     const SMDS_MeshNode * n2,
                                     const SMDS_MeshNode * n3,
                                     const SMDS_MeshNode * n4,
                                     const SMDS_MeshNode * n5,
                                     const SMDS_MeshNode * n6,
                                     const SMDS_MeshNode * n7,
                                     const SMDS_MeshNode * n8);
  // hexagonal prism of 12 nodes
  virtual SMDS_MeshVolume* AddVolumeWithID(smIdType n1, smIdType n2, smIdType n3, smIdType n4, smIdType n5, smIdType n6,
                                           smIdType n7, smIdType n8, smIdType n9, smIdType n10, smIdType n11, smIdType n12, smIdType ID);
  virtual SMDS_MeshVolume* AddVolumeWithID(const SMDS_MeshNode * n1,
                                           const SMDS_MeshNode * n2,
                                           const SMDS_MeshNode * n3,
                                           const SMDS_MeshNode * n4,
                                           const SMDS_MeshNode * n5,
                                           const SMDS_MeshNode * n6,
                                           const SMDS_MeshNode * n7,
                                           const SMDS_MeshNode * n8, 
                                           const SMDS_MeshNode * n9, 
                                           const SMDS_MeshNode * n10, 
                                           const SMDS_MeshNode * n11, 
                                           const SMDS_MeshNode * n12, 
                                           smIdType ID);
  virtual SMDS_MeshVolume* AddVolume(const SMDS_MeshNode * n1,
                                     const SMDS_MeshNode * n2,
                                     const SMDS_MeshNode * n3,
                                     const SMDS_MeshNode * n4,
                                     const SMDS_MeshNode * n5,
                                     const SMDS_MeshNode * n6,
                                     const SMDS_MeshNode * n7,
                                     const SMDS_MeshNode * n8, 
                                     const SMDS_MeshNode * n9, 
                                     const SMDS_MeshNode * n10, 
                                     const SMDS_MeshNode * n11, 
                                     const SMDS_MeshNode * n12);

  // 2d order tetrahedron of 10 nodes
  virtual SMDS_MeshVolume* AddVolumeWithID(smIdType n1, smIdType n2, smIdType n3, smIdType n4,
                                           smIdType n12,smIdType n23,smIdType n31,
                                           smIdType n14,smIdType n24,smIdType n34, smIdType ID);
  virtual SMDS_MeshVolume* AddVolumeWithID(const SMDS_MeshNode * n1,
                                           const SMDS_MeshNode * n2,
                                           const SMDS_MeshNode * n3,
                                           const SMDS_MeshNode * n4, 
                                           const SMDS_MeshNode * n12,
                                           const SMDS_MeshNode * n23,
                                           const SMDS_MeshNode * n31,
                                           const SMDS_MeshNode * n14, 
                                           const SMDS_MeshNode * n24,
                                           const SMDS_MeshNode * n34, 
                                           smIdType ID);
  virtual SMDS_MeshVolume* AddVolume(const SMDS_MeshNode * n1,
                                     const SMDS_MeshNode * n2,
                                     const SMDS_MeshNode * n3,
                                     const SMDS_MeshNode * n4,
                                     const SMDS_MeshNode * n12,
                                     const SMDS_MeshNode * n23,
                                     const SMDS_MeshNode * n31,
                                     const SMDS_MeshNode * n14, 
                                     const SMDS_MeshNode * n24,
                                     const SMDS_MeshNode * n34);

  // 2d order pyramid of 13 nodes
  virtual SMDS_MeshVolume* AddVolumeWithID(smIdType n1, smIdType n2, smIdType n3, smIdType n4, smIdType n5,
                                           smIdType n12,smIdType n23,smIdType n34,smIdType n41,
                                           smIdType n15,smIdType n25,smIdType n35,smIdType n45,
                                           smIdType ID);
  virtual SMDS_MeshVolume* AddVolumeWithID(const SMDS_MeshNode * n1,
                                           const SMDS_MeshNode * n2,
                                           const SMDS_MeshNode * n3,
                                           const SMDS_MeshNode * n4,
                                           const SMDS_MeshNode * n5, 
                                           const SMDS_MeshNode * n12,
                                           const SMDS_MeshNode * n23,
                                           const SMDS_MeshNode * n34,
                                           const SMDS_MeshNode * n41, 
                                           const SMDS_MeshNode * n15,
                                           const SMDS_MeshNode * n25,
                                           const SMDS_MeshNode * n35,
                                           const SMDS_MeshNode * n45, 
                                           smIdType ID);
  virtual SMDS_MeshVolume* AddVolume(const SMDS_MeshNode * n1,
                                     const SMDS_MeshNode * n2,
                                     const SMDS_MeshNode * n3,
                                     const SMDS_MeshNode * n4,
                                     const SMDS_MeshNode * n5,
                                     const SMDS_MeshNode * n12,
                                     const SMDS_MeshNode * n23,
                                     const SMDS_MeshNode * n34,
                                     const SMDS_MeshNode * n41, 
                                     const SMDS_MeshNode * n15,
                                     const SMDS_MeshNode * n25,
                                     const SMDS_MeshNode * n35,
                                     const SMDS_MeshNode * n45);

  // 2d order Pentahedron with 15 nodes
  virtual SMDS_MeshVolume* AddVolumeWithID(smIdType n1, smIdType n2, smIdType n3,
                                           smIdType n4, smIdType n5, smIdType n6,
                                           smIdType n12,smIdType n23,smIdType n31,
                                           smIdType n45,smIdType n56,smIdType n64,
                                           smIdType n14,smIdType n25,smIdType n36,
                                           smIdType ID);
  virtual SMDS_MeshVolume* AddVolumeWithID(const SMDS_MeshNode * n1,
                                           const SMDS_MeshNode * n2,
                                           const SMDS_MeshNode * n3,
                                           const SMDS_MeshNode * n4,
                                           const SMDS_MeshNode * n5,
                                           const SMDS_MeshNode * n6, 
                                           const SMDS_MeshNode * n12,
                                           const SMDS_MeshNode * n23,
                                           const SMDS_MeshNode * n31, 
                                           const SMDS_MeshNode * n45,
                                           const SMDS_MeshNode * n56,
                                           const SMDS_MeshNode * n64, 
                                           const SMDS_MeshNode * n14,
                                           const SMDS_MeshNode * n25,
                                           const SMDS_MeshNode * n36, 
                                           smIdType ID);
  virtual SMDS_MeshVolume* AddVolume(const SMDS_MeshNode * n1,
                                     const SMDS_MeshNode * n2,
                                     const SMDS_MeshNode * n3,
                                     const SMDS_MeshNode * n4,
                                     const SMDS_MeshNode * n5,
                                     const SMDS_MeshNode * n6, 
                                     const SMDS_MeshNode * n12,
                                     const SMDS_MeshNode * n23,
                                     const SMDS_MeshNode * n31, 
                                     const SMDS_MeshNode * n45,
                                     const SMDS_MeshNode * n56,
                                     const SMDS_MeshNode * n64, 
                                     const SMDS_MeshNode * n14,
                                     const SMDS_MeshNode * n25,
                                     const SMDS_MeshNode * n36);

  // 2d order Pentahedron with 18 nodes
  virtual SMDS_MeshVolume* AddVolumeWithID(smIdType n1, smIdType n2, smIdType n3,
                                           smIdType n4, smIdType n5, smIdType n6,
                                           smIdType n12,smIdType n23,smIdType n31,
                                           smIdType n45,smIdType n56,smIdType n64,
                                           smIdType n14,smIdType n25,smIdType n36,
                                           smIdType n1245, smIdType n2356, smIdType n1346,
                                           smIdType ID);
  virtual SMDS_MeshVolume* AddVolumeWithID(const SMDS_MeshNode * n1,
                                           const SMDS_MeshNode * n2,
                                           const SMDS_MeshNode * n3,
                                           const SMDS_MeshNode * n4,
                                           const SMDS_MeshNode * n5,
                                           const SMDS_MeshNode * n6,
                                           const SMDS_MeshNode * n12,
                                           const SMDS_MeshNode * n23,
                                           const SMDS_MeshNode * n31,
                                           const SMDS_MeshNode * n45,
                                           const SMDS_MeshNode * n56,
                                           const SMDS_MeshNode * n64,
                                           const SMDS_MeshNode * n14,
                                           const SMDS_MeshNode * n25,
                                           const SMDS_MeshNode * n36,
                                           const SMDS_MeshNode * n1245,
                                           const SMDS_MeshNode * n2356,
                                           const SMDS_MeshNode * n1346,
                                           smIdType ID);
  virtual SMDS_MeshVolume* AddVolume(const SMDS_MeshNode * n1,
                                     const SMDS_MeshNode * n2,
                                     const SMDS_MeshNode * n3,
                                     const SMDS_MeshNode * n4,
                                     const SMDS_MeshNode * n5,
                                     const SMDS_MeshNode * n6,
                                     const SMDS_MeshNode * n12,
                                     const SMDS_MeshNode * n23,
                                     const SMDS_MeshNode * n31,
                                     const SMDS_MeshNode * n45,
                                     const SMDS_MeshNode * n56,
                                     const SMDS_MeshNode * n64,
                                     const SMDS_MeshNode * n14,
                                     const SMDS_MeshNode * n25,
                                     const SMDS_MeshNode * n36,
                                     const SMDS_MeshNode * n1245,
                                     const SMDS_MeshNode * n2356,
                                     const SMDS_MeshNode * n1346);

  // 2d order Hexahedrons with 20 nodes
  virtual SMDS_MeshVolume* AddVolumeWithID(smIdType n1, smIdType n2, smIdType n3, smIdType n4,
                                           smIdType n5, smIdType n6, smIdType n7, smIdType n8,
                                           smIdType n12,smIdType n23,smIdType n34,smIdType n41,
                                           smIdType n56,smIdType n67,smIdType n78,smIdType n85,
                                           smIdType n15,smIdType n26,smIdType n37,smIdType n48,
                                           smIdType ID);
  virtual SMDS_MeshVolume* AddVolumeWithID(const SMDS_MeshNode * n1,
                                           const SMDS_MeshNode * n2,
                                           const SMDS_MeshNode * n3,
                                           const SMDS_MeshNode * n4,
                                           const SMDS_MeshNode * n5,
                                           const SMDS_MeshNode * n6,
                                           const SMDS_MeshNode * n7,
                                           const SMDS_MeshNode * n8, 
                                           const SMDS_MeshNode * n12,
                                           const SMDS_MeshNode * n23,
                                           const SMDS_MeshNode * n34,
                                           const SMDS_MeshNode * n41, 
                                           const SMDS_MeshNode * n56,
                                           const SMDS_MeshNode * n67,
                                           const SMDS_MeshNode * n78,
                                           const SMDS_MeshNode * n85, 
                                           const SMDS_MeshNode * n15,
                                           const SMDS_MeshNode * n26,
                                           const SMDS_MeshNode * n37,
                                           const SMDS_MeshNode * n48, 
                                           smIdType ID);
  virtual SMDS_MeshVolume* AddVolume(const SMDS_MeshNode * n1,
                                     const SMDS_MeshNode * n2,
                                     const SMDS_MeshNode * n3,
                                     const SMDS_MeshNode * n4,
                                     const SMDS_MeshNode * n5,
                                     const SMDS_MeshNode * n6,
                                     const SMDS_MeshNode * n7,
                                     const SMDS_MeshNode * n8, 
                                     const SMDS_MeshNode * n12,
                                     const SMDS_MeshNode * n23,
                                     const SMDS_MeshNode * n34,
                                     const SMDS_MeshNode * n41, 
                                     const SMDS_MeshNode * n56,
                                     const SMDS_MeshNode * n67,
                                     const SMDS_MeshNode * n78,
                                     const SMDS_MeshNode * n85, 
                                     const SMDS_MeshNode * n15,
                                     const SMDS_MeshNode * n26,
                                     const SMDS_MeshNode * n37,
                                     const SMDS_MeshNode * n48);

  // 2d order Hexahedrons with 27 nodes
  virtual SMDS_MeshVolume* AddVolumeWithID(smIdType n1, smIdType n2, smIdType n3, smIdType n4,
                                           smIdType n5, smIdType n6, smIdType n7, smIdType n8,
                                           smIdType n12,smIdType n23,smIdType n34,smIdType n41,
                                           smIdType n56,smIdType n67,smIdType n78,smIdType n85,
                                           smIdType n15,smIdType n26,smIdType n37,smIdType n48,
                                           smIdType n1234,smIdType n1256,smIdType n2367,smIdType n3478,
                                           smIdType n1458,smIdType n5678,smIdType nCenter,
                                           smIdType ID);
  virtual SMDS_MeshVolume* AddVolumeWithID(const SMDS_MeshNode * n1,
                                           const SMDS_MeshNode * n2,
                                           const SMDS_MeshNode * n3,
                                           const SMDS_MeshNode * n4,
                                           const SMDS_MeshNode * n5,
                                           const SMDS_MeshNode * n6,
                                           const SMDS_MeshNode * n7,
                                           const SMDS_MeshNode * n8, 
                                           const SMDS_MeshNode * n12,
                                           const SMDS_MeshNode * n23,
                                           const SMDS_MeshNode * n34,
                                           const SMDS_MeshNode * n41, 
                                           const SMDS_MeshNode * n56,
                                           const SMDS_MeshNode * n67,
                                           const SMDS_MeshNode * n78,
                                           const SMDS_MeshNode * n85, 
                                           const SMDS_MeshNode * n15,
                                           const SMDS_MeshNode * n26,
                                           const SMDS_MeshNode * n37,
                                           const SMDS_MeshNode * n48, 
                                           const SMDS_MeshNode * n1234,
                                           const SMDS_MeshNode * n1256,
                                           const SMDS_MeshNode * n2367,
                                           const SMDS_MeshNode * n3478,
                                           const SMDS_MeshNode * n1458,
                                           const SMDS_MeshNode * n5678,
                                           const SMDS_MeshNode * nCenter,
                                           smIdType ID);
  virtual SMDS_MeshVolume* AddVolume(const SMDS_MeshNode * n1,
                                     const SMDS_MeshNode * n2,
                                     const SMDS_MeshNode * n3,
                                     const SMDS_MeshNode * n4,
                                     const SMDS_MeshNode * n5,
                                     const SMDS_MeshNode * n6,
                                     const SMDS_MeshNode * n7,
                                     const SMDS_MeshNode * n8, 
                                     const SMDS_MeshNode * n12,
                                     const SMDS_MeshNode * n23,
                                     const SMDS_MeshNode * n34,
                                     const SMDS_MeshNode * n41, 
                                     const SMDS_MeshNode * n56,
                                     const SMDS_MeshNode * n67,
                                     const SMDS_MeshNode * n78,
                                     const SMDS_MeshNode * n85, 
                                     const SMDS_MeshNode * n15,
                                     const SMDS_MeshNode * n26,
                                     const SMDS_MeshNode * n37,
                                     const SMDS_MeshNode * n48,
                                     const SMDS_MeshNode * n1234,
                                     const SMDS_MeshNode * n1256,
                                     const SMDS_MeshNode * n2367,
                                     const SMDS_MeshNode * n3478,
                                     const SMDS_MeshNode * n1458,
                                     const SMDS_MeshNode * n5678,
                                     const SMDS_MeshNode * nCenter);

  virtual SMDS_MeshFace* AddPolygonalFaceWithID (const std::vector<smIdType>& nodes_ids,
                                                 const smIdType               ID);

  virtual SMDS_MeshFace* AddPolygonalFaceWithID (const std::vector<const SMDS_MeshNode*>& nodes,
                                                 const smIdType                                ID);

  virtual SMDS_MeshFace* AddPolygonalFace (const std::vector<const SMDS_MeshNode*>& nodes);

  virtual SMDS_MeshFace* AddQuadPolygonalFaceWithID(const std::vector<smIdType> & nodes_ids,
                                                    const smIdType                ID);

  virtual SMDS_MeshFace* AddQuadPolygonalFaceWithID(const std::vector<const SMDS_MeshNode*> & nodes,
                                                    const smIdType                                 ID);

  virtual SMDS_MeshFace* AddQuadPolygonalFace(const std::vector<const SMDS_MeshNode*> & nodes);

  virtual SMDS_MeshVolume* AddPolyhedralVolumeWithID
    (const std::vector<smIdType>& nodes_ids,
     const std::vector<int>&      quantities,
     const smIdType               ID);

  virtual SMDS_MeshVolume* AddPolyhedralVolumeWithID
    (const std::vector<const SMDS_MeshNode*>& nodes,
     const std::vector<int>&                  quantities,
     const smIdType                           ID);

  virtual SMDS_MeshVolume* AddPolyhedralVolume
    (const std::vector<const SMDS_MeshNode*>& nodes,
     const std::vector<int>&                  quantities);

  virtual void MoveNode(const SMDS_MeshNode *, double x, double y, double z);
  virtual void RemoveNode(const SMDS_MeshNode *);
  void RemoveElement(const SMDS_MeshElement *);

  /*! Remove only the given element/node and only if it is free.
   *  Methods do not work for meshes with descendants.
   *  Implemented for fast cleaning of meshes.
   */
  bool RemoveFreeNode   (const SMDS_MeshNode *,    SMESHDS_SubMesh *, bool fromGroups=true);
  void RemoveFreeElement(const SMDS_MeshElement *, SMESHDS_SubMesh *, bool fromGroups=true);

  void ClearMesh();

  bool ChangeElementNodes(const SMDS_MeshElement * elem,
                          const SMDS_MeshNode    * nodes[],
                          const int                nbnodes);
  bool ChangePolygonNodes(const SMDS_MeshElement *           elem,
                          std::vector<const SMDS_MeshNode*>& nodes);
  bool ChangePolyhedronNodes(const SMDS_MeshElement *                 elem,
                             const std::vector<const SMDS_MeshNode*>& nodes,
                             const std::vector<int>&                  quantities);
  bool ModifyCellNodes(vtkIdType smdsVolId, std::map<int,int> localClonedNodeIds);
  void Renumber (const bool isNodes, const smIdType startID=1, const smIdType deltaID=1);

  void SetNodeInVolume(const SMDS_MeshNode * aNode, const TopoDS_Shell & S);
  void SetNodeInVolume(const SMDS_MeshNode * aNode, const TopoDS_Solid & S);
  void SetNodeOnFace  (const SMDS_MeshNode * aNode, const TopoDS_Face& S, double u=0.,double v=0.);
  void SetNodeOnEdge  (const SMDS_MeshNode * aNode, const TopoDS_Edge& S, double u=0.);
  void SetNodeOnVertex(const SMDS_MeshNode * aNode, const TopoDS_Vertex & S);
  void UnSetNodeOnShape(const SMDS_MeshNode * aNode);
  void UnSetElementOnShape(const SMDS_MeshElement * anElt);
  void SetMeshElementOnShape  (const SMDS_MeshElement * anElt, const TopoDS_Shape & S);
  void UnSetMeshElementOnShape(const SMDS_MeshElement * anElt, const TopoDS_Shape & S);
  void SetNodeInVolume(const SMDS_MeshNode * aNode, int Index);
  void SetNodeOnFace  (const SMDS_MeshNode * aNode, int Index, double u=0., double v=0.);
  void SetNodeOnEdge  (const SMDS_MeshNode * aNode, int Index, double u=0.);
  void SetNodeOnVertex(const SMDS_MeshNode * aNode, int Index);
  void SetMeshElementOnShape(const SMDS_MeshElement * anElt, int Index);
  bool HasMeshElements(const TopoDS_Shape & S) const;
  SMESHDS_SubMesh * MeshElements(const TopoDS_Shape & S) const;
  SMESHDS_SubMesh * MeshElements(const int Index) const;
  std::list<int> SubMeshIndices() const;
  SMESHDS_SubMeshIteratorPtr SubMeshes() const;

  bool HasHypothesis(const TopoDS_Shape & S);
  const std::list<const SMESHDS_Hypothesis*>& GetHypothesis(const TopoDS_Shape & S) const;
  bool IsUsedHypothesis(const SMESHDS_Hypothesis * H) const;
  const ShapeToHypothesis & GetHypotheses() const { return myShapeToHypothesis; }

  SMESHDS_Script * GetScript();
  void ClearScript();

  int ShapeToIndex(const TopoDS_Shape & aShape) const;
  const TopoDS_Shape& IndexToShape(int ShapeIndex) const;
  int MaxShapeIndex() const { return myIndexToShape.Extent(); }
  int MaxSubMeshIndex() const;

  SMESHDS_SubMesh * NewSubMesh(int Index);
  int AddCompoundSubmesh(const TopoDS_Shape& S, TopAbs_ShapeEnum type = TopAbs_SHAPE);

  // Groups. SMESHDS_Mesh is not an owner of groups
  void AddGroup (SMESHDS_GroupBase* theGroup)      { myGroups.insert(theGroup); }
  void RemoveGroup (SMESHDS_GroupBase* theGroup)   { myGroups.erase(theGroup); }
  size_t GetNbGroups() const                      { return myGroups.size(); }
  const std::set<SMESHDS_GroupBase*>& GetGroups() const { return myGroups; }

  bool IsGroupOfSubShapes (const TopoDS_Shape& aSubShape) const;

  virtual void CompactMesh();
  void CleanDownWardConnectivity();
  void BuildDownWardConnectivity(bool withEdges);

  virtual void SetStructuredGrid( const TopoDS_Shape & shape, const int nx, const int ny, const int nz = 1 );
  virtual void SetNodeOnStructuredGrid( const TopoDS_Shape & shape, const std::shared_ptr<gp_Pnt>& P, const int iIndex, const int jIndex, const int kIndex = 0 );
  virtual void SetNodeOnStructuredGrid( const TopoDS_Shape & shape, const SMDS_MeshNode* point, const int iIndex, const int jIndex, const int kIndex = 0 );
  virtual void SetNodeOnStructuredGrid( const TopoDS_Shape & shape, const SMDS_MeshNode* point, const int index );
  virtual bool HasStructuredGridFilled( const TopoDS_Shape & shape ) const;
  virtual bool HasSomeStructuredGridFilled() const;
  virtual const std::shared_ptr<SMESHUtils::SMESH_RegularGrid>& GetTheGrid( const TopoDS_Shape & shape );
  ~SMESHDS_Mesh();
  
 private:

  ShapeToHypothesis          myShapeToHypothesis;

  int                        myPersistentID;
  TopoDS_Shape               myShape;

  class SubMeshHolder;
  SubMeshHolder*             mySubMeshHolder;

  TopTools_IndexedMapOfShape myIndexToShape;

  typedef std::set<SMESHDS_GroupBase*> TGroups;
  TGroups                    myGroups;

  SMESHDS_Script*            myScript;
  bool                       myIsEmbeddedMode;

  int add( const SMDS_MeshElement* elem, SMESHDS_SubMesh* subMesh );
  SMESHDS_SubMesh* getSubmesh( const TopoDS_Shape & shape);

  // Index the regular grid associated to the mesh in the geometry index
  NCollection_DataMap<int,std::shared_ptr<SMESHUtils::SMESH_RegularGrid>> myRegularGrid;
};


#endif
