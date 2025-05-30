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

//  SMESH SMESH : implementation of SMESH idl descriptions
// File      : StdMeshers_CompositeHexa_3D.cxx
// Module    : SMESH
// Created   : Tue Nov 25 11:04:59 2008
// Author    : Edward AGAPOV (eap)

#include "StdMeshers_CompositeHexa_3D.hxx"

#include "SMDS_Mesh.hxx"
#include "SMDS_MeshNode.hxx"
#include "SMDS_SetIterator.hxx"
#include "SMESHDS_Mesh.hxx"
#include "SMESHDS_SubMesh.hxx"
#include "SMESH_Block.hxx"
#include "SMESH_Comment.hxx"
#include "SMESH_ComputeError.hxx"
#include "SMESH_HypoFilter.hxx"
#include "SMESH_Mesh.hxx"
#include "SMESH_MeshAlgos.hxx"
#include "SMESH_MesherHelper.hxx"
#include "SMESH_subMesh.hxx"
#include "StdMeshers_BlockRenumber.hxx"
#include "StdMeshers_FaceSide.hxx"
#include "StdMeshers_ViscousLayers.hxx"

#include <BRepAdaptor_Surface.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_B3d.hxx>
#include <Standard_ErrorHandler.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_MapIteratorOfMapOfShape.hxx>
#include <TopTools_MapOfShape.hxx>
#include <TopTools_SequenceOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Vec.hxx>
#include <gp_XYZ.hxx>

#include <list>
#include <numeric>
#include <set>
#include <vector>

using namespace std;

#ifdef _DEBUG_
// #define DEB_FACES
// #define DEB_GRID
// #define DUMP_VERT(msg,V) { TopoDS_Vertex v = V; gp_Pnt p = BRep_Tool::Pnt(v); cout << msg << "( "<< p.X()<<", "<<p.Y()<<", "<<p.Z()<<" )"<<endl; }
#endif

#ifndef DUMP_VERT
#define DUMP_VERT(msg,v)
#endif

//================================================================================
// text for message about an internal error
#define ERR_LI(txt) SMESH_Comment(txt) << ":" << __LINE__

// order corresponds to right order of edges in CASCADE face
enum EQuadSides{ Q_BOTTOM=0, Q_RIGHT, Q_TOP, Q_LEFT,   Q_CHILD, Q_PARENT };

enum EBoxSides{ B_BOTTOM=0, B_RIGHT, B_TOP, B_LEFT, B_FRONT, B_BACK, B_UNDEFINED };

enum EAxes{ COO_X=1, COO_Y, COO_Z };

//================================================================================
/*!
 * \brief Converter of a pair of integers to a sole index
 */
struct _Indexer
{
  int _xSize, _ySize;
  _Indexer( int xSize, int ySize ): _xSize(xSize), _ySize(ySize) {}
  int size() const { return _xSize * _ySize; }
  int operator()(const int x, const int y) const { return y * _xSize + x; }
};

//================================================================================
/*!
 * \brief Wrapper of a composite or an ordinary edge.
 */
class _FaceSide
{
public:
  _FaceSide(const _FaceSide& other);
  _FaceSide(const TopoDS_Edge& edge=TopoDS_Edge());
  _FaceSide(const list<TopoDS_Edge>& edges);
  _FaceSide* GetSide(const int i);
  const _FaceSide* GetSide(const int i) const;
  int size() const { return myChildren.size(); }
  int NbVertices() const;
  int NbCommonVertices( const TopTools_MapOfShape& VV ) const;
  TopoDS_Vertex FirstVertex() const;
  TopoDS_Vertex LastVertex() const;
  TopoDS_Vertex Vertex(int i) const;
  TopoDS_Edge   Edge(int i) const;
  bool Contain( const _FaceSide& side, int* which=0 ) const;
  bool Contain( const TopoDS_Vertex& vertex ) const;
  void AppendSide( const _FaceSide& side );
  void SetBottomSide( int i );
  smIdType GetNbSegments(SMESH_ProxyMesh& mesh, const SMESHDS_SubMesh* smToCheckEdges=0) const;
  bool StoreNodes(SMESH_ProxyMesh& mesh, vector<const SMDS_MeshNode*>& myGrid,
                  bool reverse, bool isProxy, const SMESHDS_SubMesh* smToCheckEdges=0 );
  void SetID(EQuadSides id) { myID = id; }
  static inline const TopoDS_TShape* ptr(const TopoDS_Shape& theShape)
  { return theShape.TShape().operator->(); }
  void Dump() const;

private:


  TopoDS_Edge       myEdge;
  list< _FaceSide > myChildren;
  int               myNbChildren;

  TopTools_MapOfShape myVertices;

  EQuadSides        myID; // debug
};
//================================================================================
/*!
 * \brief Class corresponding to a meshed composite face of a box.
 *        Provides simplified access to it's sub-mesh data.
 */
class _QuadFaceGrid
{
  typedef list< _QuadFaceGrid > TChildren;
public:
  _QuadFaceGrid();

public: //** Methods to find and orient faces of 6 sides of the box **//
  
  //!< initialization
  bool Init(const TopoDS_Face& f, SMESH_ProxyMesh& mesh );

  //!< try to unite self with other face
  bool AddContinuousFace( const _QuadFaceGrid& f, const TopTools_MapOfShape& internalEdges );

  //!< Try to set the side as bottom hirizontal side
  bool SetBottomSide(const _FaceSide& side, int* sideIndex=0);

  //!< Return face adjacent to zero-based i-th side of this face
  _QuadFaceGrid* FindAdjacentForSide(int i, list<_QuadFaceGrid>& faces, EBoxSides id) const;

  //!< Reverse edges in order to have the bottom edge going along axes of the unit box
  void ReverseEdges();

  bool IsComplex() const { return !myChildren.empty(); }

  int NbChildren() const { return myChildren.size(); }

  typedef SMDS_SetIterator< const _QuadFaceGrid&,
                            TChildren::const_iterator,
                            SMDS::SimpleAccessor<const _QuadFaceGrid&,TChildren::const_iterator>,
                            SMDS::PassAllValueFilter<_QuadFaceGrid> >
    TChildIterator;

  TChildIterator GetChildren() const
  { return TChildIterator( myChildren.begin(), myChildren.end()); }

  bool Contain( const TopoDS_Vertex& vertex ) const { return mySides.Contain( vertex ); }

public: //** Loading and access to mesh **//

  //!< Load nodes of a mesh
  bool LoadGrid( SMESH_ProxyMesh& mesh );

  //!< Computes normalized parameters of nodes of myGrid
  void ComputeIJK( int i1, int i2, double v3 );

  //!< Return number of segments on the hirizontal sides
  int GetNbHoriSegments(SMESH_ProxyMesh& mesh, bool withBrothers=false) const;

  //!< Return number of segments on the vertical sides
  int GetNbVertSegments(SMESH_ProxyMesh& mesh, bool withBrothers=false) const;

  //!< Return edge on the hirizontal bottom sides
  int GetHoriEdges(vector<TopoDS_Edge> & edges) const;

  //!< Return a node by its position
  const SMDS_MeshNode* GetNode(int iHori, int iVert) const;

  //!< Return node coordinates by its position
  gp_XYZ GetXYZ(int iHori, int iVert) const;

  //!< Return normalized parameters of nodes within the unitary cube
  gp_XYZ& GetIJK(int iCol, int iRow) { return myIJK[ myIndexer( iCol, iRow )]; }

public: //** Access to member fields **//

  //!< Return i-th face side (0<i<4)
  const _FaceSide& GetSide(int i) const;

  //!< Return it's face, NULL if it is composite
  TopoDS_Face GetFace() const { return myFace; }

  //!< Return normal to the face at vertex v
  bool GetNormal( const TopoDS_Vertex& v, gp_Vec& n ) const;

  SMESH_ComputeErrorPtr GetError() const { return myError; }

  void SetID(EBoxSides id) { myID = id; }

  void DumpGrid() const;

  void DumpVertices() const;

private:

  bool error(const std::string& text, int code = COMPERR_ALGO_FAILED)
  { myError = SMESH_ComputeError::New( code, text ); return false; }

  bool error(const SMESH_ComputeErrorPtr& err)
  { myError = err; return ( !myError || myError->IsOK() ); }

  bool loadCompositeGrid(SMESH_ProxyMesh& mesh);

  bool fillGrid(SMESH_ProxyMesh&               theMesh,
                vector<const SMDS_MeshNode*> & theGrid,
                const _Indexer&                theIndexer,
                int                            theX,
                int                            theY);

  bool locateChildren();

  void setBrothers( set< _QuadFaceGrid* >& notLocatedBrothers );

  TopoDS_Face myFace;
  _FaceSide   mySides;
  bool        myReverse;

  TChildren   myChildren;

  _QuadFaceGrid* myLeftBottomChild;
  _QuadFaceGrid* myRightBrother;
  _QuadFaceGrid* myUpBrother;

  _Indexer                      myIndexer;
  vector<const SMDS_MeshNode*>  myGrid;
  vector<gp_XYZ>                myIJK; // normalized parameters of nodes

  SMESH_ComputeErrorPtr         myError;

  EBoxSides   myID; // debug
};

//================================================================================
/*!
 * \brief Constructor
 */
//================================================================================

StdMeshers_CompositeHexa_3D::StdMeshers_CompositeHexa_3D(int hypId, SMESH_Gen* gen)
  :SMESH_3D_Algo(hypId, gen), _blockRenumberHyp( nullptr )
{
  _name = "CompositeHexa_3D";
  _shapeType = (1 << TopAbs_SHELL) | (1 << TopAbs_SOLID);       // 1 bit /shape type
}

//================================================================================
/*!
 * \brief always return true
 */
//================================================================================

bool StdMeshers_CompositeHexa_3D::CheckHypothesis(SMESH_Mesh&         /*aMesh*/,
                                                  const TopoDS_Shape& /*aShape*/,
                                                  Hypothesis_Status&  aStatus)
{
  _blockRenumberHyp = nullptr;
  aStatus = HYP_OK;
  return true;
}

namespace
{

  //================================================================================
  /*!
   * \brief Checks structure of a quadrangular mesh at the common VERTEX of two EDGEs.
   *        Returns true if there are two quadrangles near the VERTEX.
   */
  //================================================================================

  bool isContinuousMesh(TopoDS_Edge            E1,
                        TopoDS_Edge            E2,
                        const TopoDS_Face&     F,
                        const SMESH_ProxyMesh& mesh)
  {
    if (E1.Orientation() > TopAbs_REVERSED) // INTERNAL
      E1.Orientation( TopAbs_FORWARD );
    if (E2.Orientation() > TopAbs_REVERSED) // INTERNAL
      E2.Orientation( TopAbs_FORWARD );

    TopoDS_Vertex V;
    if ( !TopExp::CommonVertex( E1, E2, V )) return false;

    const SMDS_MeshNode* n = SMESH_Algo::VertexNode( V, mesh.GetMeshDS() );
    if ( !n ) return SMESH_Algo::IsContinuous( E1, E2 ); // meshed by "composite segment"

    n = mesh.GetProxyNode( n );

    const SMESHDS_SubMesh* sm = mesh.GetSubMesh( F );
    if ( !sm ) return false;

    int nbQuads = 0;
    SMDS_ElemIteratorPtr fIt = mesh.GetInverseElementIterator( n, SMDSAbs_Face );
    if ( !fIt->more() )
      return SMESH_Algo::IsContinuous( E1, E2 ); // meshed by "composite segment"
    while ( fIt->more() )
    {
      const SMDS_MeshElement* f = fIt->next();
      if ( !sm->Contains( f )) continue;

      if ( f->NbCornerNodes() == 4 )
        ++nbQuads;
      else
        return false;
    }
    return nbQuads == 2;
  }

  //================================================================================
  /*!
   * \brief Return true if a vertex holds a node and this node is used by some quadrangle
   */
  //================================================================================

  // bool isMeshedVertex( TopoDS_Vertex&     V,
  //                      const SMESH_Mesh&  mesh )
  // {
  //   const SMDS_MeshNode* n = SMESH_Algo::VertexNode( V, mesh.GetMeshDS() );
  //   if ( !n ) return false;
    
  //   SMDS_ElemIteratorPtr fIt = n->GetInverseElementIterator(SMDSAbs_Face);
  //   while ( fIt->more() )
  //   {
  //     const SMDS_MeshElement* f = fIt->next();
  //     if ( f->NbCornerNodes() == 4 )
  //       return true;
  //   }
  //   return false;
  // }

  //================================================================================
  /*!
   * \brief Finds VERTEXes located at block corners
   */
  //================================================================================

  void getBlockCorners( SMESH_ProxyMesh&     mesh,
                        const TopoDS_Shape&  shape,
                        TopTools_MapOfShape& cornerVV)
  {
    std::set<int> faceIDs; // ids of FACEs in the shape
    TopExp_Explorer exp;
    for ( exp.Init( shape, TopAbs_FACE ); exp.More(); exp.Next() )
      faceIDs.insert( mesh.GetMeshDS()->ShapeToIndex( exp.Current() ));

    TopTools_MapOfShape checkedVV;
    for ( exp.Init( shape, TopAbs_VERTEX ); exp.More(); exp.Next() )
    {
      TopoDS_Vertex V = TopoDS::Vertex( exp.Current() );
      if ( !checkedVV.Add( V )) continue;

      const SMDS_MeshNode* n = SMESH_Algo::VertexNode( V, mesh.GetMeshDS() );
      if ( !n ) continue;

      const SMDS_MeshNode* nProxy = mesh.GetProxyNode( n );
      bool isProxy = ( nProxy != n );
      n = nProxy;

      int nbQuads = 0;
      SMDS_ElemIteratorPtr fIt = mesh.GetInverseElementIterator( n, SMDSAbs_Face );
      while ( fIt->more() )
      {
        const SMDS_MeshElement* f = fIt->next();
        if ( !faceIDs.count( f->getshapeId() )) continue;

        if ( isProxy && !mesh.GetSubMesh( f->getshapeId() )->Contains( f ))
          continue;

        if ( f->NbCornerNodes() == 4 )
          ++nbQuads;
        else
          nbQuads = 100;
      }
      if ( nbQuads == 3 )
        cornerVV.Add( V );
    }
  }

  //================================================================================
  /*!
   * \brief Return EDGEs dividing one box side
   */
  //================================================================================

  bool getInternalEdges( SMESH_Mesh&                mesh,
                         const TopoDS_Shape&        shape,
                         const TopTools_MapOfShape& cornerVV,
                         TopTools_MapOfShape&       internEE)
  {
    TopTools_IndexedMapOfShape subEE, subFF;
    TopExp::MapShapes( shape, TopAbs_EDGE, subEE );
    TopExp::MapShapes( shape, TopAbs_FACE, subFF );

    TopoDS_Vertex VV[2];
    TopTools_MapOfShape subChecked, ridgeEE;
    TopTools_MapIteratorOfMapOfShape vIt( cornerVV );
    for ( ; vIt.More(); vIt.Next() )
    {
      TopoDS_Shape V0 = vIt.Key();
      // walk from one corner VERTEX to another along ridge EDGEs
      PShapeIteratorPtr riIt = SMESH_MesherHelper::GetAncestors( V0, mesh, TopAbs_EDGE );
      while ( const TopoDS_Shape* riE = riIt->next() )
      {
        if ( !subEE.Contains( *riE ) || !subChecked.Add( *riE ))
          continue;
        TopoDS_Edge ridgeE = TopoDS::Edge( *riE );
        while ( !ridgeE.IsNull() )
        {
          if ( !ridgeEE.Add( ridgeE ))
            break;
          TopExp::Vertices( ridgeE, VV[0], VV[1] );
          TopoDS_Shape V1 = VV[ V0.IsSame( VV[0] )];
          if ( cornerVV.Contains( V1 ) )
            break; // ridgeE reached a corner VERTEX

          // detect internal EDGEs among those sharing V1. There can be 2, 3 or 4 EDGEs and
          // number of internal EDGEs is N-2
          TopoDS_Shape nextRidgeE;
          PShapeIteratorPtr eIt = SMESH_MesherHelper::GetAncestors( V1, mesh, TopAbs_EDGE );
          while ( const TopoDS_Shape* E = eIt->next() )
          {
            if ( E->IsSame( ridgeE ) || !subEE.Contains( *E ) || !subChecked.Add( *E ))
              continue;
            // look for FACEs sharing both E and ridgeE
            PShapeIteratorPtr fIt = SMESH_MesherHelper::GetAncestors( *E, mesh, TopAbs_FACE );
            while ( const TopoDS_Shape* F = fIt->next() )
            {
              if ( !SMESH_MesherHelper::IsSubShape( ridgeE, *F ))
                continue;
              if ( !subFF.Contains( *F ))
                continue;
              if ( isContinuousMesh( ridgeE, TopoDS::Edge( *E ), TopoDS::Face( *F ), mesh ))
              {
                nextRidgeE = *E;
              }
              else
              {
                internEE.Add( *E );
              }
              break;
            }
          }
          // look for the next ridge EDGE ending at V1
          if ( nextRidgeE.IsNull() )
          {
            eIt = SMESH_MesherHelper::GetAncestors( V1, mesh, TopAbs_EDGE );
            while ( const TopoDS_Shape* E = eIt->next() )
              if ( !ridgeE.IsSame( *E ) && !internEE.Contains( *E ) && subEE.Contains( *E ))
              {
                nextRidgeE = *E;
                break;
              }
          }
          ridgeE = TopoDS::Edge( nextRidgeE );
          V0 = V1;

          if ( ridgeE.IsNull() )
            return false;
        } // check EDGEs around the last VERTEX of ridgeE 
      } // loop on ridge EDGEs around a corner VERTEX
    } // loop on on corner VERTEXes

    if ( subEE.Extent() > ridgeEE.Extent() + internEE.Extent() ) // PAL23269
      for ( int i = 1; i < subEE.Extent(); ++i )
        if ( !ridgeEE.Contains( subEE(i) ))
          internEE.Add( subEE(i) );

    return true;
  } // getInternalEdges()

  //================================================================================
  /*!
   * \brief Find a face including two given nodes
   */
  //================================================================================

  const SMDS_MeshElement* FindFaceByNodes( const SMDS_MeshNode* n1,
                                           const SMDS_MeshNode* n2,
                                           TIDSortedElemSet     avoidSet,
                                           SMESH_ProxyMesh&     mesh)
  {
    SMDS_ElemIteratorPtr faceIt = mesh.GetInverseElementIterator( n1, SMDSAbs_Face );
    while ( faceIt->more() )
    {
      const SMDS_MeshElement* f = faceIt->next();
      if ( !avoidSet.count( f ) && f->GetNodeIndex( n2 ) >= 0 )
        return f;
    }
    return 0;
  }

  //================================================================================
  /*!
   * \brief Check that a segment bounds a face belonging to smOfFaces
   */
  //================================================================================

  bool IsSegmentOnSubMeshBoundary( const SMDS_MeshNode*   n1,
                                   const SMDS_MeshNode*   n2,
                                   const SMESHDS_SubMesh* smOfFaces,
                                   SMESH_ProxyMesh&       mesh)
  {
    TIDSortedElemSet avoidSet;
    bool faceFound = false;

    while ( const SMDS_MeshElement* f = FindFaceByNodes( n1, n2, avoidSet, mesh ))
    {
      if (( faceFound = smOfFaces->Contains( f )))
        break;
      avoidSet.insert( f );
    }
    return faceFound;
  }

    //================================================================================
  /*!
   * \brief Rearrange block sides according to StdMeshers_BlockRenumber hypothesis
   */
  //================================================================================

  bool arrangeForRenumber( list< _QuadFaceGrid >&     blockSides,
                           const TopTools_MapOfShape& cornerVertices,
                           SMESH_Mesh*                mesh,
                           TopoDS_Vertex&             v000,
                           TopoDS_Vertex&             v001 )
  {
    if ( v000.IsNull() )
    {
      // block CS is not defined;
      // renumber only if the block has an edge parallel to an axis of global CS

      v000 = StdMeshers_RenumberHelper::GetVertex000( cornerVertices );
    }

    Bnd_B3d bbox;
    for ( auto it = cornerVertices.cbegin(); it != cornerVertices.cend(); ++it )
      bbox.Add( BRep_Tool::Pnt( TopoDS::Vertex( *it )));
    double tol = 1e-5 * Sqrt( bbox.SquareExtent() );

    // get block edges starting at v000

    std::vector< const _FaceSide* > edgesAtV000;
    std::vector< gp_Vec >           edgeDir;
    std::vector< int >              iParallel; // 0 - none, 1 - X, 2 - Y, 3 - Z
    TopTools_MapOfShape             lastVertices;
    for ( _QuadFaceGrid & quad: blockSides )
    {
      for ( int iS = 0; iS < 4 &&  edgesAtV000.size() < 3; ++iS )
      {
        const _FaceSide* side = & quad.GetSide( iS );
        TopoDS_Vertex v1 = side->FirstVertex(), v2 = side->LastVertex();
        if (( v1.IsSame( v000 ) && !lastVertices.Contains( v2 )) ||
            ( v2.IsSame( v000 ) && !lastVertices.Contains( v1 )))
        {
          bool reverse = v2.IsSame( v000 );
          if ( reverse )
            std::swap( v1, v2 );
          lastVertices.Add( v2 );

          edgesAtV000.push_back( side );

          gp_Pnt pf = BRep_Tool::Pnt( v1 );
          gp_Pnt pl = BRep_Tool::Pnt( v2 );
          gp_Vec vec( pf, pl );
          edgeDir.push_back( vec );

          iParallel.push_back( 0 );
          if ( !v001.IsNull() )
          {
            if ( quad.IsComplex() )
              for ( _QuadFaceGrid::TChildIterator chIt = quad.GetChildren(); chIt.more(); )
              {
                const _QuadFaceGrid& child = chIt.next();
                if ( child.GetSide( iS ).Contain( v001 ))
                {
                  iParallel.back() = 3;
                  break;
                }
              }
            else if ( side->Contain( v001 ))
              iParallel.back() = 3;
          }
          else
          {
            bool isStraight = true;
            std::list< TopoDS_Edge > edges;
            for ( int iE = 0; true; ++iE )
            {
              TopoDS_Edge edge = side->Edge( iE );
              if ( edge.IsNull() )
                break;
              edges.push_back( edge );
              if ( isStraight )
                isStraight = SMESH_Algo::IsStraight( edge );
            }
            // is parallel to a GCS axis?
            if ( isStraight )
            {
              int nbDiff = (( Abs( vec.X() ) > tol ) +
                            ( Abs( vec.Y() ) > tol ) +
                            ( Abs( vec.Z() ) > tol ) );
              if ( nbDiff == 1 )
                iParallel.back() = ( Abs( vec.X() ) > tol ) ? 1 : ( Abs( vec.Y() ) > tol ) ? 2 : 3;
            }
            else
            {
              TopoDS_Face nullFace;
              StdMeshers_FaceSide fSide( nullFace, edges, mesh, true, true );
              edgeDir.back() = gp_Vec( pf, fSide.Value3d( reverse ? 0.99 : 0.01 ));
            }
          }
        }
      }
    }
    if ( std::accumulate( iParallel.begin(), iParallel.end(), 0 ) == 0 )
      return false;

    // find edge OZ and edge OX
    const _FaceSide* edgeOZ = nullptr, *edgeOY = nullptr, *edgeOX = nullptr;
    auto iZIt = std::find( iParallel.begin(), iParallel.end(), 3 );
    if ( iZIt != iParallel.end() )
    {
      int i = std::distance( iParallel.begin(), iZIt );
      edgeOZ = edgesAtV000[ i ];
      int iE1 = SMESH_MesherHelper::WrapIndex( i + 1, edgesAtV000.size() );
      int iE2 = SMESH_MesherHelper::WrapIndex( i + 2, edgesAtV000.size() );
      if (( edgeDir[ iE1 ] ^ edgeDir[ iE2 ] ) * edgeDir[ i ] < 0 )
        std::swap( iE1, iE2 );
      edgeOX = edgesAtV000[ iE1 ];
      edgeOY = edgesAtV000[ iE2 ];
    }
    else
    {
      for ( size_t i = 0; i < edgesAtV000.size(); ++i )
      {
        if ( !iParallel[ i ] )
          continue;
        int iE1 = SMESH_MesherHelper::WrapIndex( i + 1, edgesAtV000.size() );
        int iE2 = SMESH_MesherHelper::WrapIndex( i + 2, edgesAtV000.size() );
        if (( edgeDir[ iE1 ] ^ edgeDir[ iE2 ] ) * edgeDir[ i ] < 0 )
          std::swap( iE1, iE2 );
        edgeOZ = edgesAtV000[ iParallel[i] == 1 ? iE2 : iE1 ];
        edgeOX = edgesAtV000[ iParallel[i] == 1 ? i : iE1 ];
        edgeOY = edgesAtV000[ iParallel[i] == 1 ? iE1 : i ];
        break;
      }
    }

    if ( !edgeOZ || !edgeOX || !edgeOY )
      return false;

    TopoDS_Vertex v100 = edgeOX->LastVertex();
    if ( v100.IsSame( v000 ))
      v100 = edgeOX->FirstVertex();

    // Find the left quad, one including v000 but not v100

    for ( auto quad = blockSides.begin(); quad != blockSides.end(); ++quad )
    {
      if ( quad->Contain( v000 ) && !quad->Contain( v100 )) // it's a left quad
      {
        if ( quad != blockSides.begin() )
          blockSides.splice( blockSides.begin(), blockSides, quad );
        blockSides.front().SetBottomSide( *edgeOZ ); // edgeOY

        return true;
      }
    }
    return false;
  }

} // namespace

//================================================================================
/*!
 * \brief Tries to find 6 sides of a box
 */
//================================================================================

bool StdMeshers_CompositeHexa_3D::findBoxFaces( const TopoDS_Shape&    shape,
                                                list< _QuadFaceGrid >& boxFaces,
                                                SMESH_Mesh&            mesh,
                                                SMESH_ProxyMesh&       proxyMesh,
                                                bool&                  toRenumber,
                                                _QuadFaceGrid * &      fBottom,
                                                _QuadFaceGrid * &      fTop,
                                                _QuadFaceGrid * &      fFront,
                                                _QuadFaceGrid * &      fBack,
                                                _QuadFaceGrid * &      fLeft,
                                                _QuadFaceGrid * &      fRight)
{
  TopTools_MapOfShape cornerVertices;
  getBlockCorners( proxyMesh, shape, cornerVertices );
  if ( cornerVertices.Extent() != 8 )
    return error( COMPERR_BAD_INPUT_MESH, "Can't find 8 corners of a block by 2D mesh" );
  TopTools_MapOfShape internalEdges;
  if ( !getInternalEdges( mesh, shape, cornerVertices, internalEdges ))
    return error( COMPERR_BAD_INPUT_MESH, "2D mesh is not suitable for i,j,k hexa meshing" );

  list< _QuadFaceGrid >::iterator boxFace;
  TopExp_Explorer exp;
  int nbFaces = 0;
  for ( exp.Init( shape, TopAbs_FACE ); exp.More(); exp.Next(), ++nbFaces )
  {
    _QuadFaceGrid f;
    if ( !f.Init( TopoDS::Face( exp.Current() ), proxyMesh ))
      return error (COMPERR_BAD_SHAPE);

    _QuadFaceGrid* prevContinuous = 0;
    for ( boxFace = boxFaces.begin(); boxFace != boxFaces.end(); ++boxFace )
    {
      if ( prevContinuous )
      {
        if ( prevContinuous->AddContinuousFace( *boxFace, internalEdges ))
          boxFace = --boxFaces.erase( boxFace );
      }
      else if ( boxFace->AddContinuousFace( f, internalEdges ))
      {
        prevContinuous = & (*boxFace);
      }
    }
    if ( !prevContinuous )
      boxFaces.push_back( f );
  }
  // Check what we have
  if ( boxFaces.size() != 6 && nbFaces != 6)
    return error
      (COMPERR_BAD_SHAPE,
       SMESH_Comment("Can't find 6 sides of a box. Number of found sides - ")<<boxFaces.size());

  if ( boxFaces.size() != 6 && nbFaces == 6 ) { // strange ordinary box with continuous faces
    boxFaces.resize( 6 );
    boxFace = boxFaces.begin();
    for ( exp.Init( shape, TopAbs_FACE); exp.More(); exp.Next(), ++boxFace )
      boxFace->Init( TopoDS::Face( exp.Current() ), proxyMesh );
  }

  toRenumber = _blockRenumberHyp;
  if ( toRenumber )
  {
    TopoDS_Vertex v000, v001;
    _blockRenumberHyp->IsSolidIncluded( mesh, shape, v000, v001 );

    toRenumber = arrangeForRenumber( boxFaces, cornerVertices, &mesh, v000, v001 );

    if ( toRenumber )
    {
      mesh.GetMeshDS()->Modified();
      mesh.GetMeshDS()->CompactMesh(); // remove numbering holes
    }
  }
  
  // ----------------------------------------
  // Find out position of faces within a box
  // ----------------------------------------
  // start from a bottom face
  fBottom = &boxFaces.front();
  fBottom->SetID( B_BOTTOM );
  // find vertical faces
  fFront = fBottom->FindAdjacentForSide( Q_BOTTOM, boxFaces, B_FRONT );
  fLeft  = fBottom->FindAdjacentForSide( Q_RIGHT,  boxFaces, B_LEFT  );
  fBack  = fBottom->FindAdjacentForSide( Q_TOP,    boxFaces, B_BACK  );
  fRight = fBottom->FindAdjacentForSide( Q_LEFT,   boxFaces, B_RIGHT );
  // check the found
  if ( !fFront || !fBack || !fLeft || !fRight )
    return error(COMPERR_BAD_SHAPE);
  // find a top face
  fTop = 0;
  for ( boxFace = ++boxFaces.begin(); boxFace != boxFaces.end() && !fTop; ++boxFace )
  {
    fTop = & (*boxFace);
    fTop->SetID( B_TOP );
    if ( fTop==fFront || fTop==fLeft || fTop==fBack || fTop==fRight )
      fTop = 0;
  }
  // set bottom of the top side
  if ( !fTop->SetBottomSide( fFront->GetSide( Q_TOP ) )) {
    if ( !fFront->IsComplex() )
      return error( ERR_LI("Error in StdMeshers_CompositeHexa_3D::Compute()"));
    else {
      _QuadFaceGrid::TChildIterator chIt = fFront->GetChildren();
      while ( chIt.more() ) {
        const _QuadFaceGrid& frontChild = chIt.next();
        if ( fTop->SetBottomSide( frontChild.GetSide( Q_TOP )))
          break;
      }
    }
  }
  if ( !fTop )
    return error(COMPERR_BAD_SHAPE);

  // orient bottom edge of faces along axes of the unit box
  fBottom->ReverseEdges();
  fBack  ->ReverseEdges();
  fLeft  ->ReverseEdges();

  return true;
}

//================================================================================
/*!
 * \brief Computes hexahedral mesh on a box with composite sides
 *  \param aMesh - mesh to compute
 *  \param aShape - shape to mesh
 *  \retval bool - success sign
 */
//================================================================================

bool StdMeshers_CompositeHexa_3D::Compute(SMESH_Mesh&         theMesh,
                                          const TopoDS_Shape& theShape)
{
  SMESH_MesherHelper helper( theMesh );
  _quadraticMesh = helper.IsQuadraticSubMesh( theShape );
  helper.SetElementsOnShape( true );

  // get Viscous Mesh
  SMESH_ProxyMesh::Ptr proxyMesh;
  SMESH_HypoFilter vlFilter( SMESH_HypoFilter::HasName( StdMeshers_ViscousLayers::GetHypType() ));
  const SMESH_Hypothesis *          hyp = theMesh.GetHypothesis( theShape, vlFilter, true );
  const StdMeshers_ViscousLayers* vlHyp = static_cast< const StdMeshers_ViscousLayers* >( hyp );
  if ( vlHyp )
    proxyMesh = vlHyp->Compute( theMesh, theShape, /*toMakeN2NMap=*/true );
  else
    proxyMesh.reset( new SMESH_ProxyMesh( theMesh ));

  // -------------------------
  // Try to find 6 side faces
  // -------------------------
  list< _QuadFaceGrid > boxFaceContainer;
  bool toRenumber = false;
  _QuadFaceGrid *fBottom, *fTop, *fFront, *fBack, *fLeft, *fRight;
  if ( ! findBoxFaces( theShape, boxFaceContainer, theMesh, *proxyMesh, toRenumber,
                       fBottom, fTop, fFront, fBack, fLeft, fRight))
    return false;

  // ------------------------------------------
  // Fill columns of nodes with existing nodes
  // ------------------------------------------

  // let faces load their grids
  if ( !fBottom->LoadGrid( *proxyMesh )) return error( fBottom->GetError() );
  if ( !fBack  ->LoadGrid( *proxyMesh )) return error( fBack  ->GetError() );
  if ( !fLeft  ->LoadGrid( *proxyMesh )) return error( fLeft  ->GetError() );
  if ( !fFront ->LoadGrid( *proxyMesh )) return error( fFront ->GetError() );
  if ( !fRight ->LoadGrid( *proxyMesh )) return error( fRight ->GetError() );
  if ( !fTop   ->LoadGrid( *proxyMesh )) return error( fTop   ->GetError() );

  // compute normalized parameters of nodes on sides (PAL23189)
  fBottom->ComputeIJK( COO_X, COO_Y, /*z=*/0. );
  fBack  ->ComputeIJK( COO_X, COO_Z, /*y=*/1. );
  fLeft  ->ComputeIJK( COO_Y, COO_Z, /*x=*/0. );
  fFront ->ComputeIJK( COO_X, COO_Z, /*y=*/0. );
  fRight ->ComputeIJK( COO_Y, COO_Z, /*x=*/1. );
  fTop   ->ComputeIJK( COO_X, COO_Y, /*z=*/1. );

  StdMeshers_RenumberHelper renumHelper( theMesh, _blockRenumberHyp );

  int x, xSize = fBottom->GetNbHoriSegments(*proxyMesh) + 1, X = xSize - 1;
  int y, ySize = fBottom->GetNbVertSegments(*proxyMesh) + 1, Y = ySize - 1;
  int z, zSize = fFront ->GetNbVertSegments(*proxyMesh) + 1, Z = zSize - 1;
  _Indexer colIndex( xSize, ySize );
  vector< vector< const SMDS_MeshNode* > > columns( colIndex.size() );

  // fill node columns by front and back box sides
  for ( x = 0; x < xSize; ++x ) {
    vector< const SMDS_MeshNode* >& column0 = columns[ colIndex( x, 0 )];
    vector< const SMDS_MeshNode* >& column1 = columns[ colIndex( x, Y )];
    column0.resize( zSize );
    column1.resize( zSize );
    for ( z = 0; z < zSize; ++z ) {
      column0[ z ] = fFront->GetNode( x, z );
      column1[ z ] = fBack ->GetNode( x, z );
    }
  }
  // fill node columns by left and right box sides
  for ( y = 1; y < ySize-1; ++y ) {
    vector< const SMDS_MeshNode* >& column0 = columns[ colIndex( 0, y )];
    vector< const SMDS_MeshNode* >& column1 = columns[ colIndex( X, y )];
    column0.resize( zSize );
    column1.resize( zSize );
    for ( z = 0; z < zSize; ++z ) {
      column0[ z ] = fLeft ->GetNode( y, z );
      column1[ z ] = fRight->GetNode( y, z );
    }
  }
  // get nodes from top and bottom box sides
  for ( x = 1; x < xSize-1; ++x ) {
    for ( y = 1; y < ySize-1; ++y ) {
      vector< const SMDS_MeshNode* >& column = columns[ colIndex( x, y )];
      column.resize( zSize );
      column.front() = fBottom->GetNode( x, y );
      column.back()  = fTop   ->GetNode( x, y );
    }
  }

  // ----------------------------
  // Add internal nodes of a box
  // ----------------------------
  // projection points of internal nodes on box sub-shapes by which
  // coordinates of internal nodes are computed
  vector<gp_XYZ> pointsOnShapes( SMESH_Block::ID_Shell );

  // projections on vertices are constant
  pointsOnShapes[ SMESH_Block::ID_V000 ] = fBottom->GetXYZ( 0, 0 );
  pointsOnShapes[ SMESH_Block::ID_V100 ] = fBottom->GetXYZ( X, 0 );
  pointsOnShapes[ SMESH_Block::ID_V010 ] = fBottom->GetXYZ( 0, Y );
  pointsOnShapes[ SMESH_Block::ID_V110 ] = fBottom->GetXYZ( X, Y );
  pointsOnShapes[ SMESH_Block::ID_V001 ] = fTop->GetXYZ( 0, 0 );
  pointsOnShapes[ SMESH_Block::ID_V101 ] = fTop->GetXYZ( X, 0 );
  pointsOnShapes[ SMESH_Block::ID_V011 ] = fTop->GetXYZ( 0, Y );
  pointsOnShapes[ SMESH_Block::ID_V111 ] = fTop->GetXYZ( X, Y );

  gp_XYZ params; // normalized parameters of an internal node within the unit box

  if ( toRenumber )
    for ( y = 0; y < ySize; ++y )
    {
      vector< const SMDS_MeshNode* >& columnXy = columns[ colIndex( X, y )];
      for ( z = 0; z < zSize; ++z )
        renumHelper.AddReplacingNode( columnXy[ z ] );
    }

  for ( x = X-1; x > 0; --x )
  {
    if ( toRenumber )
    {
      vector< const SMDS_MeshNode* >& columnX0 = columns[ colIndex( x, 0 )];
      for ( z = 0; z < zSize; ++z )
        renumHelper.AddReplacingNode( columnX0[ z ] );
    }

    const double rX = x / double(X);
    for ( y = 1; y < ySize-1; ++y )
    {
      const double rY = y / double(Y);
      // column to fill during z loop
      vector< const SMDS_MeshNode* >& column = columns[ colIndex( x, y )];
      // points projections on horizontal edges
      pointsOnShapes[ SMESH_Block::ID_Ex00 ] = fBottom->GetXYZ( x, 0 );
      pointsOnShapes[ SMESH_Block::ID_Ex10 ] = fBottom->GetXYZ( x, Y );
      pointsOnShapes[ SMESH_Block::ID_E0y0 ] = fBottom->GetXYZ( 0, y );
      pointsOnShapes[ SMESH_Block::ID_E1y0 ] = fBottom->GetXYZ( X, y );
      pointsOnShapes[ SMESH_Block::ID_Ex01 ] = fTop->GetXYZ( x, 0 );
      pointsOnShapes[ SMESH_Block::ID_Ex11 ] = fTop->GetXYZ( x, Y );
      pointsOnShapes[ SMESH_Block::ID_E0y1 ] = fTop->GetXYZ( 0, y );
      pointsOnShapes[ SMESH_Block::ID_E1y1 ] = fTop->GetXYZ( X, y );
      // points projections on horizontal faces
      pointsOnShapes[ SMESH_Block::ID_Fxy0 ] = fBottom->GetXYZ( x, y );
      pointsOnShapes[ SMESH_Block::ID_Fxy1 ] = fTop   ->GetXYZ( x, y );

      if ( toRenumber )
        renumHelper.AddReplacingNode( column[ 0 ] );

      for ( z = 1; z < zSize-1; ++z ) // z loop
      {
        // compute normalized parameters of an internal node within the unit box
        const double   rZ = z / double(Z);
        const gp_XYZ& pBo = fBottom->GetIJK( x, y );
        const gp_XYZ& pTo = fTop   ->GetIJK( x, y );
        const gp_XYZ& pFr = fFront ->GetIJK( x, z );
        const gp_XYZ& pBa = fBack  ->GetIJK( x, z );
        const gp_XYZ& pLe = fLeft  ->GetIJK( y, z );
        const gp_XYZ& pRi = fRight ->GetIJK( y, z );
        params.SetCoord( 1, 0.5 * ( pBo.X() * ( 1. - rZ ) + pTo.X() * rZ  +
                                    pFr.X() * ( 1. - rY ) + pBa.X() * rY ));
        params.SetCoord( 2, 0.5 * ( pBo.Y() * ( 1. - rZ ) + pTo.Y() * rZ  +
                                    pLe.Y() * ( 1. - rX ) + pRi.Y() * rX ));
        params.SetCoord( 3, 0.5 * ( pFr.Z() * ( 1. - rY ) + pBa.Z() * rY  +
                                    pLe.Z() * ( 1. - rX ) + pRi.Z() * rX ));

        // point projections on vertical edges
        pointsOnShapes[ SMESH_Block::ID_E00z ] = fFront->GetXYZ( 0, z );
        pointsOnShapes[ SMESH_Block::ID_E10z ] = fFront->GetXYZ( X, z );
        pointsOnShapes[ SMESH_Block::ID_E01z ] = fBack->GetXYZ( 0, z );
        pointsOnShapes[ SMESH_Block::ID_E11z ] = fBack->GetXYZ( X, z );
        // point projections on vertical faces
        pointsOnShapes[ SMESH_Block::ID_Fx0z ] = fFront->GetXYZ( x, z );
        pointsOnShapes[ SMESH_Block::ID_Fx1z ] = fBack ->GetXYZ( x, z );    
        pointsOnShapes[ SMESH_Block::ID_F0yz ] = fLeft ->GetXYZ( y, z );    
        pointsOnShapes[ SMESH_Block::ID_F1yz ] = fRight->GetXYZ( y, z );

        // compute internal node coordinates
        gp_XYZ coords;
        SMESH_Block::ShellPoint( params, pointsOnShapes, coords );
        column[ z ] = helper.AddNode( coords.X(), coords.Y(), coords.Z() );

#ifdef DEB_GRID
        // debug
        //cout << "----------------------------------------------------------------------"<<endl;
        //for ( int id = SMESH_Block::ID_V000; id < SMESH_Block::ID_Shell; ++id)
        //{
        //  gp_XYZ p = pointsOnShapes[ id ];
        //  SMESH_Block::DumpShapeID( id,cout)<<" ( "<<p.X()<<", "<<p.Y()<<", "<<p.Z()<<" )"<<endl;
        //}
        //cout << "Params: ( "<< params.X()<<", "<<params.Y()<<", "<<params.Z()<<" )"<<endl;
        //cout << "coords: ( "<< coords.X()<<", "<<coords.Y()<<", "<<coords.Z()<<" )"<<endl;
#endif
      } // z loop
      if ( toRenumber )
        renumHelper.AddReplacingNode( column[ Z ] );

    } // y loop
    if ( toRenumber )
    {
      vector< const SMDS_MeshNode* >& columnXY = columns[ colIndex( x, Y )];
      for ( z = 0; z < zSize; ++z )
        renumHelper.AddReplacingNode( columnXY[ z ] );
    }
  } // for ( x = X-1; x > 0; --x )

  if ( toRenumber )
    for ( y = 0; y < ySize; ++y )
    {
      vector< const SMDS_MeshNode* >& column0Y = columns[ colIndex( 0, y )];
      for ( z = 0; z < zSize; ++z )
        renumHelper.AddReplacingNode( column0Y[ z ] );
    }


  // faces no more needed, free memory
  boxFaceContainer.clear();

  // ----------------
  // Add hexahedrons
  // ----------------
  for ( x = xSize-2; true; --x ) {
    for ( y = 0; y < ySize-1; ++y ) {
      vector< const SMDS_MeshNode* >& col00 = columns[ colIndex( x, y )];
      vector< const SMDS_MeshNode* >& col10 = columns[ colIndex( x+1, y )];
      vector< const SMDS_MeshNode* >& col01 = columns[ colIndex( x, y+1 )];
      vector< const SMDS_MeshNode* >& col11 = columns[ colIndex( x+1, y+1 )];
      for ( z = 0; z < zSize-1; ++z )
      {
        // bottom face normal of a hexa mush point outside the volume
        helper.AddVolume(col10[z], col11[z], col11[z+1], col10[z+1],
                         col00[z], col01[z], col01[z+1], col00[z+1]);
      }
    }
    if ( x == 0)
      break;

  }
  if ( toRenumber )
    renumHelper.DoReplaceNodes();

  if ( _blockRenumberHyp )
  {
    return error( _blockRenumberHyp->CheckHypothesis( theMesh, theShape ));
  }

  return true;
}

//================================================================================
/*!
 *  Evaluate
 */
//================================================================================

bool StdMeshers_CompositeHexa_3D::Evaluate(SMESH_Mesh&         theMesh,
                                           const TopoDS_Shape& theShape,
                                           MapShapeNbElems&    aResMap)
{
  SMESH_ProxyMesh::Ptr proxyMesh( new SMESH_ProxyMesh( theMesh ));

  // -------------------------
  // Try to find 6 side faces
  // -------------------------
  list< _QuadFaceGrid > boxFaceContainer;
  _QuadFaceGrid *fBottom, *fTop, *fFront, *fBack, *fLeft, *fRight;
  bool toRenumber = false;
  if ( ! findBoxFaces( theShape, boxFaceContainer, theMesh, *proxyMesh, toRenumber,
                       fBottom, fTop, fFront, fBack, fLeft, fRight))
    return false;

  // Find a less complex side
  _QuadFaceGrid * lessComplexSide = & boxFaceContainer.front();
  list< _QuadFaceGrid >::iterator face = boxFaceContainer.begin();
  for ( ++face; face != boxFaceContainer.end() && lessComplexSide->IsComplex(); ++face )
    if ( face->NbChildren() < lessComplexSide->NbChildren() )
      lessComplexSide = & *face;

  // Get an 1D size of lessComplexSide
  smIdType nbSeg1 = 0;
  vector<TopoDS_Edge> edges;
  if ( !lessComplexSide->GetHoriEdges(edges) )
    return false;
  for ( size_t i = 0; i < edges.size(); ++i )
  {
    const vector<smIdType>& nbElems = aResMap[ theMesh.GetSubMesh( edges[i] )];
    if ( !nbElems.empty() )
      nbSeg1 += std::max( nbElems[ SMDSEntity_Edge ], nbElems[ SMDSEntity_Quad_Edge ]);
  }

  // Get an 1D size of a box side orthogonal to lessComplexSide
  smIdType nbSeg2 = 0;
  _QuadFaceGrid* ortoSide =
    lessComplexSide->FindAdjacentForSide( Q_LEFT, boxFaceContainer, B_UNDEFINED );
  edges.clear();
  if ( !ortoSide || !ortoSide->GetHoriEdges(edges) ) return false;
  for ( size_t i = 0; i < edges.size(); ++i )
  {
    const vector<smIdType>& nbElems = aResMap[ theMesh.GetSubMesh( edges[i] )];
    if ( !nbElems.empty() )
      nbSeg2 += std::max( nbElems[ SMDSEntity_Edge ], nbElems[ SMDSEntity_Quad_Edge ]);
  }

  // Get an 2D size of a box side orthogonal to lessComplexSide
  smIdType nbFaces = 0, nbQuadFace = 0;
  list< TopoDS_Face > sideFaces;
  if ( ortoSide->IsComplex() )
    for ( _QuadFaceGrid::TChildIterator child = ortoSide->GetChildren(); child.more(); )
      sideFaces.push_back( child.next().GetFace() );
  else
    sideFaces.push_back( ortoSide->GetFace() );
  //
  list< TopoDS_Face >::iterator f = sideFaces.begin();
  for ( ; f != sideFaces.end(); ++f )
  {
    const vector<smIdType>& nbElems = aResMap[ theMesh.GetSubMesh( *f )];
    if ( !nbElems.empty() )
    {
      nbFaces    = nbElems[ SMDSEntity_Quadrangle ];
      nbQuadFace = nbElems[ SMDSEntity_Quad_Quadrangle ];
    }
  }

  // Fill nb of elements
  vector<smIdType> aResVec(SMDSEntity_Last,0);
  smIdType nbSeg3 = ( nbFaces + nbQuadFace ) / nbSeg2;
  aResVec[SMDSEntity_Node]       = (nbSeg1-1) * (nbSeg2-1) * (nbSeg3-1);
  aResVec[SMDSEntity_Hexa]       = nbSeg1 * nbFaces;
  aResVec[SMDSEntity_Quad_Hexa]  = nbSeg1 * nbQuadFace;

  aResMap.insert( make_pair( theMesh.GetSubMesh(theShape), aResVec ));

  return true;
}


//================================================================================
/*!
 * \brief constructor of non-initialized _QuadFaceGrid
 */
//================================================================================

_QuadFaceGrid::_QuadFaceGrid():
  myReverse(false), myRightBrother(0), myUpBrother(0), myIndexer(0,0), myID(B_UNDEFINED)
{
}

//================================================================================
/*!
 * \brief Initialization
 */
//================================================================================

bool _QuadFaceGrid::Init(const TopoDS_Face& f, SMESH_ProxyMesh& mesh)
{
  myFace         = f;
  mySides        = _FaceSide();
  myReverse      = false;
  myLeftBottomChild = myRightBrother = myUpBrother = 0;
  myChildren.clear();
  myGrid.clear();
  //if ( myFace.Orientation() != TopAbs_FORWARD )
    //myFace.Reverse();

  list< TopoDS_Edge > edges;
  list< int > nbEdgesInWire;
  int nbWire = SMESH_Block::GetOrderedEdges (myFace, edges, nbEdgesInWire);
  if ( nbWire != 1 )
    return false;

  list< TopoDS_Edge >::iterator edgeIt = edges.begin();
  if ( nbEdgesInWire.front() == 4 ) // exactly 4 edges
  {
    for ( ; edgeIt != edges.end(); ++edgeIt )
      mySides.AppendSide( _FaceSide( *edgeIt ));
  }
  else if ( nbEdgesInWire.front() > 4 ) { // more than 4 edges - try to unite some
    list< TopoDS_Edge > sideEdges;
    while ( !edges.empty()) {
      sideEdges.clear();
      sideEdges.splice( sideEdges.end(), edges, edges.begin());// edges.front()->sideEdges.back()
      if ( SMESH_Algo::isDegenerated( sideEdges.back() ))
        continue;
      while ( !edges.empty() ) {
        if ( isContinuousMesh( sideEdges.back(), edges.front(), f, mesh )) {
          sideEdges.splice( sideEdges.end(), edges, edges.begin());
        }
        else if ( isContinuousMesh( sideEdges.front(), edges.back(), f, mesh )) {
          sideEdges.splice( sideEdges.begin(), edges, --edges.end());
        }
        else {
          break;
        }
      }
      mySides.AppendSide( _FaceSide( sideEdges ));
    }
  }
  if (mySides.size() != 4)
    return false;

if (SALOME::VerbosityActivated())
{
  mySides.GetSide( Q_BOTTOM )->SetID( Q_BOTTOM );
  mySides.GetSide( Q_RIGHT  )->SetID( Q_RIGHT );
  mySides.GetSide( Q_TOP    )->SetID( Q_TOP );
  mySides.GetSide( Q_LEFT   )->SetID( Q_LEFT );
}

  return true;
}

//================================================================================
/*!
 * \brief Try to unite self with other ordinary face
 */
//================================================================================

bool _QuadFaceGrid::AddContinuousFace( const _QuadFaceGrid&       other,
                                       const TopTools_MapOfShape& internalEdges)
{
  for ( int i = 0; i < 4; ++i )
  {
    const _FaceSide& otherSide = other.GetSide( i );
    int iMyCommon;
    if ( mySides.Contain( otherSide, &iMyCommon ))
    {
      if ( internalEdges.Contains( otherSide.Edge( 0 )))
      {
        DUMP_VERT("Cont 1", mySides.GetSide(iMyCommon)->FirstVertex());
        DUMP_VERT("Cont 2", mySides.GetSide(iMyCommon)->LastVertex());
        DUMP_VERT("Cont 3", otherSide.FirstVertex());
        DUMP_VERT("Cont 4", otherSide.LastVertex());

        if ( myChildren.empty() )
        {
          myChildren.push_back( *this );
          myFace.Nullify();
        }
        else // find iMyCommon in myChildren
        {
          for ( TChildIterator children = GetChildren(); children.more(); ) {
            const _QuadFaceGrid& child = children.next();
            if ( child.mySides.Contain( otherSide, &iMyCommon ))
              break;
          }
        }

        // orient new children equally
        int otherBottomIndex = SMESH_MesherHelper::WrapIndex( i - iMyCommon + 2, 4 );
        if ( other.IsComplex() )
          for ( TChildIterator children = other.GetChildren(); children.more(); ) {
            myChildren.push_back( children.next() );
            myChildren.back().SetBottomSide( myChildren.back().GetSide( otherBottomIndex ));
          }
        else {
          myChildren.push_back( other );
          myChildren.back().SetBottomSide( myChildren.back().GetSide( otherBottomIndex ));
        }

        myLeftBottomChild = 0;

        // collect vertices in mySides
        if ( other.IsComplex() )
          for ( TChildIterator children = other.GetChildren(); children.more(); )
          {
            const _QuadFaceGrid& child = children.next();
            for ( int i = 0; i < 4; ++i )
              mySides.AppendSide( child.GetSide(i) );
          }
        else
          for ( int i = 0; i < 4; ++i )
            mySides.AppendSide( other.GetSide(i) );

        return true;
      }
    }
  }
  return false;
}

//================================================================================
/*!
 * \brief Try to set the side as bottom hirizontal side
 */
//================================================================================

bool _QuadFaceGrid::SetBottomSide(const _FaceSide& bottom, int* sideIndex)
{
  myLeftBottomChild = myRightBrother = myUpBrother = 0;

  int myBottomIndex;
  if ( myChildren.empty() )
  {
    if ( mySides.Contain( bottom, &myBottomIndex )) {
      mySides.SetBottomSide( myBottomIndex );
      if ( sideIndex )
        *sideIndex = myBottomIndex;
      return true;
    }
  }
  else
  {
    TChildren::iterator childFace = myChildren.begin(), childEnd = myChildren.end();
    for ( ; childFace != childEnd; ++childFace )
    {
      if ( childFace->SetBottomSide( bottom, &myBottomIndex ))
      {
        TChildren::iterator orientedChild = childFace;
        for ( childFace = myChildren.begin(); childFace != childEnd; ++childFace ) {
          if ( childFace != orientedChild )
            childFace->SetBottomSide( childFace->GetSide( myBottomIndex ));
        }
        if ( sideIndex )
          *sideIndex = myBottomIndex;
        return true;
      }
    }
  }
  return false;
}

//================================================================================
/*!
 * \brief Return face adjacent to i-th side of this face, (0<i<4)
 */
//================================================================================

_QuadFaceGrid* _QuadFaceGrid::FindAdjacentForSide(int                  i,
                                                  list<_QuadFaceGrid>& faces,
                                                  EBoxSides            id) const
{
  const _FaceSide & iSide = GetSide( i );
  list< _QuadFaceGrid >::iterator boxFace = faces.begin();
  for ( ; boxFace != faces.end(); ++boxFace )
  {
    _QuadFaceGrid* f  = & (*boxFace);
    if ( f != this && f->SetBottomSide( iSide ))
      return f->SetID( id ), f;
  }
  return (_QuadFaceGrid*) 0;
}

//================================================================================
/*!
 * \brief Return i-th side
 */
//================================================================================

const _FaceSide& _QuadFaceGrid::GetSide(int i) const
{
  if ( myChildren.empty() )
    return *mySides.GetSide(i);

  _QuadFaceGrid* me = const_cast<_QuadFaceGrid*>(this);
  if ( !me->locateChildren() || !myLeftBottomChild )
    return *mySides.GetSide(i);

  const _QuadFaceGrid* child = myLeftBottomChild;
  switch ( i ){
  case Q_BOTTOM:
  case Q_LEFT:
    break;
  case Q_RIGHT:
    while ( child->myRightBrother )
      child = child->myRightBrother;
    break;
  case Q_TOP:
    while ( child->myUpBrother )
      child = child->myUpBrother;
    break;
  default: ;
  }
  return child->GetSide( i );
}

//================================================================================
/*!
 * \brief Reverse edges in order to have them oriented along axes of the unit box
 */
//================================================================================

void _QuadFaceGrid::ReverseEdges()
{
  myReverse = !myReverse;

// #ifdef DEB_FACES
//   if ( !myFace.IsNull() )
//     TopAbs::Print(myFace.Orientation(), cout);
// #endif

  if ( myChildren.empty() )
  {
    DumpVertices();
  }
  else
  {
    DumpVertices();
    TChildren::iterator child = myChildren.begin(), childEnd = myChildren.end();
    for ( ; child != childEnd; ++child )
      child->ReverseEdges();
  }
}

//================================================================================
/*!
 * \brief Load nodes of a mesh
 */
//================================================================================

bool _QuadFaceGrid::LoadGrid( SMESH_ProxyMesh& mesh )
{
  if ( !myChildren.empty() )
  {
    // Let child faces load their grids
    TChildren::iterator child = myChildren.begin(), childEnd = myChildren.end();
    for ( ; child != childEnd; ++child ) {
      child->SetID( myID );
      if ( !child->LoadGrid( mesh ) )
        return error( child->GetError() );
    }
    // Fill myGrid with nodes of patches
    return loadCompositeGrid( mesh );
  }

  // ---------------------------------------
  // Fill myGrid with nodes bound to myFace
  // ---------------------------------------

  if ( !myGrid.empty() )
    return true;

  const SMESHDS_SubMesh* faceSubMesh = mesh.GetSubMesh( myFace );

  // check that all faces are quadrangular
  SMDS_ElemIteratorPtr fIt = faceSubMesh->GetElements();
  while ( fIt->more() )
    if ( fIt->next()->NbNodes() % 4 > 0 )
      return error("Non-quadrangular mesh faces are not allowed on sides of a composite block");

  bool isProxy = false, isTmpElem = false;
  if ( faceSubMesh && faceSubMesh->NbElements() > 0 )
  {
    isProxy   = dynamic_cast< const SMESH_ProxyMesh::SubMesh* >( faceSubMesh );
    isTmpElem = mesh.IsTemporary( faceSubMesh->GetElements()->next() );
  }
  const SMESHDS_SubMesh* smToCheckEdges = ( isProxy && !isTmpElem ) ? faceSubMesh : 0;

  myIndexer._xSize = 1 + mySides.GetSide( Q_BOTTOM )->GetNbSegments( mesh, smToCheckEdges );
  myIndexer._ySize = 1 + mySides.GetSide( Q_LEFT   )->GetNbSegments( mesh, smToCheckEdges );

  myGrid.resize( myIndexer.size() );

  // store nodes bound to the bottom edge
  mySides.GetSide( Q_BOTTOM )->StoreNodes( mesh, myGrid, myReverse, isProxy, smToCheckEdges );

  // store the rest nodes row by row

  TIDSortedElemSet avoidSet;
  const SMDS_MeshElement* firstQuad = 0; // most left face above the last row of found nodes

  size_t nbFoundNodes = myIndexer._xSize;
  while ( nbFoundNodes != myGrid.size() )
  {
    // first and last nodes of the last filled row of nodes
    const SMDS_MeshNode* n1down = myGrid[ nbFoundNodes - myIndexer._xSize ];
    const SMDS_MeshNode* n2down = myGrid[ nbFoundNodes - myIndexer._xSize + 1];
    const SMDS_MeshNode* n1downLast = myGrid[ nbFoundNodes-1 ];

    // find the first face above the row by the first two left nodes
    //
    // n1up     n2up
    //     o---o
    //     |   |
    //     o---o  o  o  o  o
    //n1down    n2down
    //
    firstQuad = FindFaceByNodes( n1down, n2down, avoidSet, mesh );
    while ( firstQuad && !faceSubMesh->Contains( firstQuad )) {
      avoidSet.insert( firstQuad );
      firstQuad = FindFaceByNodes( n1down, n2down, avoidSet, mesh);
    }
    if ( !firstQuad || !faceSubMesh->Contains( firstQuad ))
      return error(ERR_LI("Error in _QuadFaceGrid::LoadGrid()"));

    // find the node of quad bound to the left geom edge
    int i2down = firstQuad->GetNodeIndex( n2down );
    const SMDS_MeshNode* n1up = firstQuad->GetNode(( i2down+2 ) % 4 );
    myGrid[ nbFoundNodes++ ] = n1up;
    // the 4-the node of the first quad
    int i1down = firstQuad->GetNodeIndex( n1down );
    const SMDS_MeshNode* n2up = firstQuad->GetNode(( i1down+2 ) % 4 );
    myGrid[ nbFoundNodes++ ] = n2up;

    n1down = n2down;
    n1up   = n2up;
    const SMDS_MeshElement* quad = firstQuad;

    // find the rest nodes by remaining faces above the row
    //
    //             n1up
    //     o---o--o
    //     |   |  | ->
    //     o---o--o  o  o  o
    //                      n1downLast
    //
    while ( n1down != n1downLast )
    {
      // next face
      avoidSet.clear(); avoidSet.insert( quad );
      quad = FindFaceByNodes( n1down, n1up, avoidSet, mesh );
      if ( !quad || quad->NbNodes() % 4 > 0)
        return error(ERR_LI("Error in _QuadFaceGrid::LoadGrid()"));

      // next node
      if ( quad->GetNode( i1down ) != n1down ) // check already found index
        i1down = quad->GetNodeIndex( n1down );
      n2up = quad->GetNode(( i1down+2 ) % 4 );
      myGrid[ nbFoundNodes++ ] = n2up;

      n1down = myGrid[ nbFoundNodes - myIndexer._xSize - 1 ];
      n1up   = n2up;
    }
    avoidSet.clear(); avoidSet.insert( firstQuad );
  }
  DumpGrid(); // debug

  return true;
}

//================================================================================
/*!
 * \brief Fill myIJK with normalized parameters of nodes in myGrid
 *  \param [in] i1 - coordinate index along rows of myGrid
 *  \param [in] i2 - coordinate index along columns of myGrid
 *  \param [in] v3 - value of the constant parameter
 */
//================================================================================

void _QuadFaceGrid::ComputeIJK( int i1, int i2, double v3 )
{
  gp_XYZ ijk( v3, v3, v3 );
  myIJK.resize( myIndexer.size(), ijk );

  const size_t nbCol = myIndexer._xSize;
  const size_t nbRow = myIndexer._ySize;

  vector< double > len( nbRow );
  len[0] = 0;
  for ( size_t i = 0; i < nbCol; ++i )
  {
    gp_Pnt pPrev = GetXYZ( i, 0 );
    for ( size_t j = 1; j < nbRow; ++j )
    {
      gp_Pnt p = GetXYZ( i, j );
      len[ j ] = len[ j-1 ] + p.Distance( pPrev );
      pPrev = p;
    }
    for ( size_t j = 0; j < nbRow; ++j )
      GetIJK( i, j ).SetCoord( i2, len[ j ]/len.back() );
  }

  len.resize( nbCol );
  for ( size_t j = 0; j < nbRow; ++j )
  {
    gp_Pnt pPrev = GetXYZ( 0, j );
    for ( size_t i = 1; i < nbCol; ++i )
    {
      gp_Pnt p = GetXYZ( i, j );
      len[ i ] = len[ i-1 ] + p.Distance( pPrev );
      pPrev = p;
    }
    for ( size_t i = 0; i < nbCol; ++i )
      GetIJK( i, j ).SetCoord( i1, len[ i ]/len.back() );
  }
}

//================================================================================
/*!
 * \brief Find out mutual location of children: find their right and up brothers
 */
//================================================================================

bool _QuadFaceGrid::locateChildren()
{
  if ( myLeftBottomChild )
    return true;

  TChildren::iterator child = myChildren.begin(), childEnd = myChildren.end();

  // find a child sharing it's first bottom vertex with no other brother
  myLeftBottomChild = 0;
  for ( ; !myLeftBottomChild && child != childEnd; ++child )
  {
    TopoDS_Vertex leftVertex = child->GetSide( Q_BOTTOM ).FirstVertex();
    bool sharedVertex = false;
    TChildren::iterator otherChild = myChildren.begin();
    for ( ; otherChild != childEnd && !sharedVertex; ++otherChild )
      if ( otherChild != child )
        sharedVertex = otherChild->mySides.Contain( leftVertex );
    if ( !sharedVertex ) {
      myLeftBottomChild = & (*child);
      DUMP_VERT("0 left bottom Vertex: ",leftVertex );
    }
  }
  if (!myLeftBottomChild)
    return error(ERR_LI("Error in locateChildren()"));

  set< _QuadFaceGrid* > notLocatedChilren;
  for (child = myChildren.begin() ; child != childEnd; ++child )
    notLocatedChilren.insert( & (*child));

  // connect myLeftBottomChild to it's right and upper brothers
  notLocatedChilren.erase( myLeftBottomChild );
  myLeftBottomChild->setBrothers( notLocatedChilren );
  if ( !notLocatedChilren.empty() )
    return error(ERR_LI("Error in locateChildren()"));

  return true;
}

//================================================================================
/*!
 * \brief Fill myGrid with nodes of patches
 */
//================================================================================

bool _QuadFaceGrid::loadCompositeGrid(SMESH_ProxyMesh& mesh)
{
  // Find out mutual location of children: find their right and up brothers
  if ( !locateChildren() )
    return false;

  // Load nodes according to mutual location of children

  // grid size
  myIndexer._xSize = 1 + myLeftBottomChild->GetNbHoriSegments( mesh, /*withBrothers=*/true );
  myIndexer._ySize = 1 + myLeftBottomChild->GetNbVertSegments( mesh, /*withBrothers=*/true );

  myGrid.resize( myIndexer.size() );

  int fromX = myReverse ? myIndexer._xSize : 0;
  if ( !myLeftBottomChild->fillGrid( mesh, myGrid, myIndexer, fromX, 0 ))
    return error( myLeftBottomChild->GetError() );

  DumpGrid();

  return true;
}

//================================================================================
/*!
 * \brief Find right an upper brothers among notLocatedBrothers
 */
//================================================================================

void _QuadFaceGrid::setBrothers( set< _QuadFaceGrid* >& notLocatedBrothers )
{
  if ( !notLocatedBrothers.empty() )
  {
    // find right brother
    TopoDS_Vertex rightVertex = GetSide( Q_BOTTOM ).LastVertex();
    DUMP_VERT("1 right bottom Vertex: ",rightVertex );
    set< _QuadFaceGrid* >::iterator brIt, brEnd = notLocatedBrothers.end();
    for ( brIt = notLocatedBrothers.begin(); brIt != brEnd; ++brIt )
    {
      _QuadFaceGrid* brother = *brIt;
      TopoDS_Vertex brotherLeftVertex = brother->GetSide( Q_BOTTOM ).FirstVertex();
      DUMP_VERT( "brother left bottom: ", brotherLeftVertex );
      if ( rightVertex.IsSame( brotherLeftVertex )) {
        myRightBrother = brother;
        notLocatedBrothers.erase( brIt );
        break;
      }
    }
    // find upper brother
    TopoDS_Vertex upVertex = GetSide( Q_LEFT ).FirstVertex();
    DUMP_VERT("1 left up Vertex: ",upVertex);
    brIt = notLocatedBrothers.begin(), brEnd = notLocatedBrothers.end();
    for ( ; brIt != brEnd; ++brIt )
    {
      _QuadFaceGrid* brother = *brIt;
      TopoDS_Vertex brotherLeftVertex = brother->GetSide( Q_BOTTOM ).FirstVertex();
      DUMP_VERT("brother left bottom: ", brotherLeftVertex);
      if ( upVertex.IsSame( brotherLeftVertex )) {
        myUpBrother = brother;
        notLocatedBrothers.erase( myUpBrother );
        break;
      }
    }
    // recursive call
    if ( myRightBrother )
      myRightBrother->setBrothers( notLocatedBrothers );
    if ( myUpBrother )
      myUpBrother->setBrothers( notLocatedBrothers );
  }
}

//================================================================================
/*!
 * \brief Store nodes of a simple face into grid starting from (x,y) position
 */
//================================================================================

bool _QuadFaceGrid::fillGrid(SMESH_ProxyMesh&               theMesh,
                             vector<const SMDS_MeshNode*> & theGrid,
                             const _Indexer&                theIndexer,
                             int                            theX,
                             int                            theY)
{
  if ( myGrid.empty() && !LoadGrid( theMesh ))
    return false;

  // store my own grid in the global grid

  int fromX = myReverse ? theX - myIndexer._xSize: theX;

  for ( int i = 0, x = fromX; i < myIndexer._xSize; ++i, ++x )
    for ( int j = 0, y = theY; j < myIndexer._ySize; ++j, ++y )
      theGrid[ theIndexer( x, y )] = myGrid[ myIndexer( i, j )];

  // store grids of my right and upper brothers

  if ( myRightBrother )
  {
    if ( myReverse )
      fromX += 1;
    else
      fromX += myIndexer._xSize - 1;
    if ( !myRightBrother->fillGrid( theMesh, theGrid, theIndexer, fromX, theY ))
      return error( myRightBrother->GetError() );
  }
  if ( myUpBrother )
  {
    if ( !myUpBrother->fillGrid( theMesh, theGrid, theIndexer,
                                 theX, theY + myIndexer._ySize - 1))
      return error( myUpBrother->GetError() );
  }
  return true;
}

//================================================================================
/*!
 * \brief Return number of segments on the hirizontal sides
 */
//================================================================================

int _QuadFaceGrid::GetNbHoriSegments(SMESH_ProxyMesh& mesh, bool withBrothers) const
{
  int nbSegs = 0;
  if ( myLeftBottomChild )
  {
    nbSegs += myLeftBottomChild->GetNbHoriSegments( mesh, true );
  }
  else
  {
    nbSegs = mySides.GetSide( Q_BOTTOM )->GetNbSegments( mesh );
    if ( withBrothers && myRightBrother )
      nbSegs += myRightBrother->GetNbHoriSegments( mesh, withBrothers );
  }
  return nbSegs;
}

//================================================================================
/*!
 * \brief Return number of segments on the vertical sides
 */
//================================================================================

int _QuadFaceGrid::GetNbVertSegments(SMESH_ProxyMesh& mesh, bool withBrothers) const
{
  int nbSegs = 0;
  if ( myLeftBottomChild )
  {
    nbSegs += myLeftBottomChild->GetNbVertSegments( mesh, true );
  }
  else
  {
    nbSegs = mySides.GetSide( Q_LEFT )->GetNbSegments(mesh,0);
    if ( withBrothers && myUpBrother )
      nbSegs += myUpBrother->GetNbVertSegments( mesh, withBrothers );
  }
  return nbSegs;
}

//================================================================================
/*!
 * \brief Return edge on the hirizontal bottom sides
 */
//================================================================================

int _QuadFaceGrid::GetHoriEdges(vector<TopoDS_Edge> & edges) const
{
  if ( myLeftBottomChild )
  {
    return myLeftBottomChild->GetHoriEdges( edges );
  }
  else
  {
    const _FaceSide* bottom  = mySides.GetSide( Q_BOTTOM );
    int i = 0;
    while ( true ) {
      TopoDS_Edge e = bottom->Edge( i++ );
      if ( e.IsNull() )
        break;
      else
        edges.push_back( e );
    }
    if ( myRightBrother )
      myRightBrother->GetHoriEdges( edges );
  }
  return edges.size();
}

//================================================================================
/*!
 * \brief Return a node by its position
 */
//================================================================================

const SMDS_MeshNode* _QuadFaceGrid::GetNode(int iHori, int iVert) const
{
  return myGrid[ myIndexer( iHori, iVert )];
}

//================================================================================
/*!
 * \brief Return node coordinates by its position
 */
//================================================================================

gp_XYZ _QuadFaceGrid::GetXYZ(int iHori, int iVert) const
{
  SMESH_TNodeXYZ xyz = myGrid[ myIndexer( iHori, iVert )];
  return xyz;
}

//================================================================================
/*!
 * \brief Return normal to the face at vertex v
 */
//================================================================================

bool _QuadFaceGrid::GetNormal( const TopoDS_Vertex& v, gp_Vec& n ) const
{
  if ( myChildren.empty() )
  {
    if ( mySides.Contain( v )) {
      try {
        gp_Pnt2d uv = BRep_Tool::Parameters( v, myFace );
        BRepAdaptor_Surface surface( myFace );
        gp_Pnt p; gp_Vec d1u, d1v;
        surface.D1( uv.X(), uv.Y(), p, d1u, d1v );
        n = d1u.Crossed( d1v );
        return true;
      }
      catch (Standard_Failure&) {
        return false;
      }
    }
  }
  else
  {
    TChildren::const_iterator child = myChildren.begin(), childEnd = myChildren.end();
    for ( ; child != childEnd; ++child )
      if ( child->GetNormal( v, n ))
        return true;
  }
  return false;
}

//================================================================================
/*!
 * \brief Dumps coordinates of grid nodes
 */
//================================================================================

void _QuadFaceGrid::DumpGrid() const
{
#ifdef DEB_GRID
  const char* names[] = { "B_BOTTOM", "B_RIGHT", "B_TOP", "B_LEFT", "B_FRONT", "B_BACK" };
  cout << "****** Face " << names[ myID ] << endl;

  if ( myChildren.empty() || !myGrid.empty() )
  {
    cout << "x size: " << myIndexer._xSize << "; y size: " << myIndexer._ySize << endl;
    for ( int y = 0; y < myIndexer._ySize; ++y ) {
      cout << "-- row " << y << endl;
      for ( int x = 0; x < myIndexer._xSize; ++x ) {
        const SMDS_MeshNode* n = myGrid[ myIndexer( x, y ) ];
        cout << x << " ( " << n->X() << ", " << n->Y() << ", " << n->Z() << " )" << endl;
      }
    }
  }
  else
  {
    cout << "Nb children: " << myChildren.size() << endl;
    TChildren::const_iterator child = myChildren.begin(), childEnd = myChildren.end();
    for ( int i=0; child != childEnd; ++child, ++i ) {
      cout << "   *** SUBFACE " << i+1 << endl;
      ((_QuadFaceGrid&)(*child)).SetID( myID );
      child->DumpGrid();
    }
  }
#endif
}

//================================================================================
/*!
 * \brief Dump vertices
 */
//================================================================================

void _QuadFaceGrid::DumpVertices() const
{
#ifdef DEB_FACES
  cout << "****** Face ";
  const char* names[] = { "B_BOTTOM", "B_RIGHT", "B_TOP", "B_LEFT", "B_FRONT", "B_BACK" };
  if ( myID >= B_BOTTOM && myID < B_BACK )
    cout << names[ myID ] << endl;
  else
    cout << "UNDEFINED" << endl;

  if ( myChildren.empty() )
  {
    for ( int i = 0; i < 4; ++i )
    {
      cout << "  Side "; mySides.GetSide( i )->Dump();
    }
  }
  else
  {
    cout << "-- Nb children: " << myChildren.size() << endl;
    TChildren::const_iterator child = myChildren.begin(), childEnd = myChildren.end();
    for ( int i=0; child != childEnd; ++child, ++i ) {
      cout << "   *** SUBFACE " << i+1 << endl;
      ((_QuadFaceGrid&)(*child)).SetID( myID );
      child->DumpVertices();
    }
  }
#endif
}

//=======================================================================
//function : _FaceSide
//purpose  : copy constructor
//=======================================================================

_FaceSide::_FaceSide(const _FaceSide& other)
{
  myEdge = other.myEdge;
  myChildren = other.myChildren;
  myNbChildren = other.myNbChildren;
  myVertices.Assign( other.myVertices );
  myID = other.myID;
}

//================================================================================
/*!
 * \brief Construct a face side of one edge
 */
//================================================================================

_FaceSide::_FaceSide(const TopoDS_Edge& edge):
  myEdge( edge ), myNbChildren(0)
{
  if ( !edge.IsNull() )
    for ( TopExp_Explorer exp( edge, TopAbs_VERTEX ); exp.More(); exp.Next() )
      //myVertices.insert( ptr ( exp.Current() ));
      myVertices.Add( exp.Current() );
}

//================================================================================
/*!
 * \brief Construct a face side of several edges
 */
//================================================================================

_FaceSide::_FaceSide(const list<TopoDS_Edge>& edges):
  myNbChildren(0)
{
  list<TopoDS_Edge>::const_iterator edge = edges.begin(), eEnd = edges.end();
  for ( ; edge != eEnd; ++edge ) {
    myChildren.push_back( _FaceSide( *edge ));
    myNbChildren++;
    myVertices.Add( myChildren.back().FirstVertex() );
    myVertices.Add( myChildren.back().LastVertex() );
    myChildren.back().SetID( Q_CHILD ); // not to splice them
  }
}

//=======================================================================
//function : GetSide
//purpose  :
//=======================================================================

_FaceSide* _FaceSide::GetSide(const int i)
{
  if ( i >= myNbChildren )
    return 0;

  list< _FaceSide >::iterator side = myChildren.begin();
  if ( i )
    std::advance( side, i );
  return & (*side);
}

//=======================================================================
//function : GetSide
//purpose  : 
//=======================================================================

const _FaceSide* _FaceSide::GetSide(const int i) const
{
  return const_cast< _FaceSide* >(this)->GetSide(i);
}

//=======================================================================
//function : NbVertices
//purpose  : return nb of vertices in the side
//=======================================================================

int _FaceSide::NbVertices() const
{
  if ( myChildren.empty() )
    return myVertices.Extent();

  return myNbChildren + 1;
}

//=======================================================================
//function : NbCommonVertices
//purpose  : Returns number of my vertices common with the given ones
//=======================================================================

int _FaceSide::NbCommonVertices( const TopTools_MapOfShape& VV ) const
{
  int nbCommon = 0;
  TopTools_MapIteratorOfMapOfShape vIt ( myVertices );
  for ( ; vIt.More(); vIt.Next() )
    nbCommon += ( VV.Contains( vIt.Key() ));

  return nbCommon;
}

//=======================================================================
//function : FirstVertex
//purpose  :
//=======================================================================

TopoDS_Vertex _FaceSide::FirstVertex() const
{
  if ( myChildren.empty() )
    return TopExp::FirstVertex( myEdge, Standard_True );

  return myChildren.front().FirstVertex();
}

//=======================================================================
//function : LastVertex
//purpose  : 
//=======================================================================

TopoDS_Vertex _FaceSide::LastVertex() const
{
  if ( myChildren.empty() )
    return TopExp::LastVertex( myEdge, Standard_True );

  return myChildren.back().LastVertex();
}

//=======================================================================
//function : Vertex
//purpose  : 
//=======================================================================

TopoDS_Vertex _FaceSide::Vertex(int i) const
{
  if ( myChildren.empty() )
    return i ? LastVertex() : FirstVertex();
      
  if ( i >= myNbChildren )
    return myChildren.back().LastVertex();
  
  return GetSide(i)->FirstVertex();
}

//================================================================================
/*!
 * \brief Return i-the zero-based edge of the side
 */
//================================================================================

TopoDS_Edge _FaceSide::Edge(int i) const
{
  if ( i == 0 && !myEdge.IsNull() )
    return myEdge;

  if ( const _FaceSide* iSide = GetSide( i ))
    return iSide->myEdge;

  return TopoDS_Edge();
}

//=======================================================================
//function : Contain
//purpose  : 
//=======================================================================

bool _FaceSide::Contain( const _FaceSide& side, int* which ) const
{
  if ( !which || myChildren.empty() )
  {
    if ( which )
      *which = 0;
    int nbCommon = 0;
    TopTools_MapIteratorOfMapOfShape vIt ( side.myVertices );
    for ( ; vIt.More(); vIt.Next() )
      nbCommon += ( myVertices.Contains( vIt.Key() ));
    return (nbCommon > 1);
  }
  list< _FaceSide >::const_iterator mySide = myChildren.begin(), sideEnd = myChildren.end();
  for ( int i = 0; mySide != sideEnd; ++mySide, ++i ) {
    if ( mySide->Contain( side )) {
      *which = i;
      return true;
    }
  }
  return false;
}

//=======================================================================
//function : Contain
//purpose  : 
//=======================================================================

bool _FaceSide::Contain( const TopoDS_Vertex& vertex ) const
{
  return myVertices.Contains( vertex );
}

//=======================================================================
//function : AppendSide
//purpose  : 
//=======================================================================

void _FaceSide::AppendSide( const _FaceSide& side )
{
  if ( !myEdge.IsNull() )
  {
    myChildren.push_back( *this );
    myNbChildren = 1;
    myEdge.Nullify();
  }
  myChildren.push_back( side );
  myNbChildren++;
  TopTools_MapIteratorOfMapOfShape vIt ( side.myVertices );
  for ( ; vIt.More(); vIt.Next() )
    myVertices.Add( vIt.Key() );

  myID = Q_PARENT;
  myChildren.back().SetID( EQuadSides( myNbChildren-1 ));
}

//=======================================================================
//function : SetBottomSide
//purpose  : 
//=======================================================================

void _FaceSide::SetBottomSide( int i )
{
  if ( i > 0 && myID == Q_PARENT ) {
    list< _FaceSide >::iterator sideEnd, side = myChildren.begin();
    std::advance( side, i );
    myChildren.splice( myChildren.begin(), myChildren, side, myChildren.end() );

    side = myChildren.begin(), sideEnd = myChildren.end();
    for ( int i = 0; side != sideEnd; ++side, ++i ) {
      side->SetID( EQuadSides(i) );
      side->SetBottomSide(i);
    }
  }
}

//=======================================================================
//function : GetNbSegments
//purpose  : 
//=======================================================================

smIdType _FaceSide::GetNbSegments(SMESH_ProxyMesh& mesh, const SMESHDS_SubMesh* smToCheckEdges) const
{
  smIdType nb = 0;
  if ( myChildren.empty() )
  {
    nb = mesh.GetSubMesh(myEdge)->NbElements();

    if ( smToCheckEdges )
    {
      // check that segments bound faces belonging to smToCheckEdges
      SMDS_ElemIteratorPtr segIt = mesh.GetSubMesh(myEdge)->GetElements();
      while ( segIt->more() )
      {
        const SMDS_MeshElement* seg = segIt->next();
        if ( !IsSegmentOnSubMeshBoundary( mesh.GetProxyNode( seg->GetNode(0) ),
                                          mesh.GetProxyNode( seg->GetNode(1) ),
                                          smToCheckEdges, mesh ))
          --nb;
      }
    }
  }
  else
  {
    list< _FaceSide >::const_iterator side = myChildren.begin(), sideEnd = myChildren.end();
    for ( ; side != sideEnd; ++side )
      nb += side->GetNbSegments( mesh, smToCheckEdges );
  }
  return nb;
}

//=======================================================================
//function : StoreNodes
//purpose  :
//=======================================================================

bool _FaceSide::StoreNodes(SMESH_ProxyMesh&              mesh,
                           vector<const SMDS_MeshNode*>& myGrid,
                           bool                          reverse,
                           bool                          isProxy,
                           const SMESHDS_SubMesh*        smToCheckEdges)
{
  list< TopoDS_Edge > edges;
  if ( myChildren.empty() )
  {
    edges.push_back( myEdge );
  }
  else
  {
    list< _FaceSide >::const_iterator side = myChildren.begin(), sideEnd = myChildren.end();
    for ( ; side != sideEnd; ++side )
      if ( reverse )
        edges.push_front( side->myEdge );
      else
        edges.push_back ( side->myEdge );
  }
  int nbNodes = 0;
  list< TopoDS_Edge >::iterator edge = edges.begin(), eEnd = edges.end();
  for ( ; edge != eEnd; ++edge )
  {
    typedef map< double, const SMDS_MeshNode* > TParamNodeMap;
    TParamNodeMap nodes;
    bool ok = SMESH_Algo::GetSortedNodesOnEdge( mesh.GetMeshDS(),
                                                *edge,
                                                /*ignoreMediumNodes=*/true,
                                                nodes);
    if ( !ok ) return false;

    // skip nodes on VERTEXes not included in faces
    if ( !nodes.begin()->second->GetInverseElementIterator(SMDSAbs_Face)->more() )
      nodes.erase( nodes.begin() );
    if ( !nodes.empty() &&
         !nodes.rbegin()->second->GetInverseElementIterator(SMDSAbs_Face)->more() )
      nodes.erase( --nodes.end() );

    if ( isProxy )
    {
      TParamNodeMap::iterator u_node, nEnd = nodes.end();
      for ( u_node = nodes.begin(); u_node != nEnd; ++u_node )
        u_node->second = mesh.GetProxyNode( u_node->second );
    }

    if ( smToCheckEdges ) // erase nodes of segments not bounding faces of smToCheckEdges
    {
      {
        TParamNodeMap::iterator u_node1, u_node2 = nodes.begin(), nEnd = nodes.end();
        for ( u_node1 = u_node2++; u_node2 != nEnd; u_node1 = u_node2++ )
          if ( IsSegmentOnSubMeshBoundary( u_node1->second, u_node2->second,
                                           smToCheckEdges, mesh ))
            break;
          else
            nodes.erase( u_node1 );
      }
      {
        TParamNodeMap::reverse_iterator u_node1, u_node2 = nodes.rbegin(), nEnd = nodes.rend();
        for ( u_node1 = u_node2++; u_node2 != nEnd; u_node1 = u_node2++ )
          if ( IsSegmentOnSubMeshBoundary( u_node1->second, u_node2->second,
                                           smToCheckEdges, mesh ))
            break;
          else
            nodes.erase( --(( u_node2 = u_node1 ).base() ));
      }
    }

    bool forward = ( edge->Orientation() == TopAbs_FORWARD );
    if ( reverse ) forward = !forward;
    if ( forward )
    {
      TParamNodeMap::iterator u_node, nEnd = nodes.end();
      for ( u_node = nodes.begin(); u_node != nEnd; ++u_node )
        myGrid[ nbNodes++ ] = u_node->second;
    }
    else
    {
      TParamNodeMap::reverse_iterator u_node, nEnd = nodes.rend();
      for ( u_node = nodes.rbegin(); u_node != nEnd; ++u_node )
        myGrid[ nbNodes++ ] = u_node->second;
    }
    nbNodes--; // node on vertex present in two adjacent edges
  }
  return nbNodes > 0;
}

//=======================================================================
//function : Dump
//purpose  : dump end vertices
//=======================================================================

void _FaceSide::Dump() const
{
  if ( myChildren.empty() )
  {
    const char* sideNames[] = { "Q_BOTTOM", "Q_RIGHT", "Q_TOP", "Q_LEFT", "Q_CHILD", "Q_PARENT" };
    if ( myID >= Q_BOTTOM && myID < Q_PARENT )
      cout << sideNames[ myID ] << endl;
    else
      cout << "<UNDEFINED ID>" << endl;
    TopoDS_Vertex f = FirstVertex();
    TopoDS_Vertex l = LastVertex();
    gp_Pnt pf = BRep_Tool::Pnt(f);
    gp_Pnt pl = BRep_Tool::Pnt(l);
    cout << "\t ( "<< ptr( f ) << " - " << ptr( l )<< " )"
         << "\t ( "<< pf.X()<<", "<<pf.Y()<<", "<<pf.Z()<<" ) - "
         << " ( "<< pl.X()<<", "<<pl.Y()<<", "<<pl.Z()<<" )"<<endl;
  }
  else
  {
    list< _FaceSide >::const_iterator side = myChildren.begin(), sideEnd = myChildren.end();
    for ( ; side != sideEnd; ++side ) {
      side->Dump();
      cout << "\t";
    }
  }
}
