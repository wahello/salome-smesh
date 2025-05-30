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

//  SMESH SMESH : implementation of SMESH idl descriptions
//  File   : StdMeshers_Hexa_3D.cxx
//           Moved here from SMESH_Hexa_3D.cxx
//  Author : Paul RASCLE, EDF
//  Module : SMESH
//
#include "StdMeshers_Hexa_3D.hxx"

#include "SMDS_MeshNode.hxx"
#include "SMESH_Comment.hxx"
#include "SMESH_Gen.hxx"
#include "SMESH_Mesh.hxx"
#include "SMESH_MesherHelper.hxx"
#include "SMESH_subMesh.hxx"
#include "StdMeshers_BlockRenumber.hxx"
#include "StdMeshers_CompositeHexa_3D.hxx"
#include "StdMeshers_FaceSide.hxx"
#include "StdMeshers_HexaFromSkin_3D.hxx"
#include "StdMeshers_Penta_3D.hxx"
#include "StdMeshers_Prism_3D.hxx"
#include "StdMeshers_Quadrangle_2D.hxx"
#include "StdMeshers_ViscousLayers.hxx"

#include <BRep_Tool.hxx>
#include <Bnd_B3d.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_MapOfShape.hxx>
#include <TopTools_SequenceOfShape.hxx>
#include <TopoDS.hxx>

#include "utilities.h"
#include "Utils_ExceptHandlers.hxx"

#include <cstddef>
#include <numeric>

typedef SMESH_Comment TComm;

using namespace std;

static SMESH_ComputeErrorPtr ComputePentahedralMesh(SMESH_Mesh &,
                                                    const TopoDS_Shape &,
                                                    SMESH_ProxyMesh* proxyMesh=0);

static bool EvaluatePentahedralMesh(SMESH_Mesh &, const TopoDS_Shape &,
                                    MapShapeNbElems &);

//=============================================================================
/*!
 * Constructor
 */
//=============================================================================

StdMeshers_Hexa_3D::StdMeshers_Hexa_3D(int hypId, SMESH_Gen * gen)
  :SMESH_3D_Algo(hypId, gen)
{
  _name = "Hexa_3D";
  _shapeType = (1 << TopAbs_SHELL) | (1 << TopAbs_SOLID);       // 1 bit /shape type
  _requireShape = false;
  _compatibleHypothesis.push_back("ViscousLayers");
  _compatibleHypothesis.push_back("BlockRenumber");
  _quadAlgo = new StdMeshers_Quadrangle_2D( gen->GetANewId(), _gen );
}

//=============================================================================
/*!
 * Destructor
 */
//=============================================================================

StdMeshers_Hexa_3D::~StdMeshers_Hexa_3D()
{
  delete _quadAlgo;
  _quadAlgo = 0;
}

//=============================================================================
/*!
 * Retrieves defined hypotheses
 */
//=============================================================================

bool StdMeshers_Hexa_3D::CheckHypothesis
                         (SMESH_Mesh&                          aMesh,
                          const TopoDS_Shape&                  aShape,
                          SMESH_Hypothesis::Hypothesis_Status& aStatus)
{
  // check nb of faces in the shape
/*  PAL16229
  aStatus = SMESH_Hypothesis::HYP_BAD_GEOMETRY;
  int nbFaces = 0;
  for (TopExp_Explorer exp(aShape, TopAbs_FACE); exp.More(); exp.Next())
    if ( ++nbFaces > 6 )
      break;
  if ( nbFaces != 6 )
    return false;
*/

  _viscousLayersHyp = nullptr;
  _blockRenumberHyp = nullptr;

  const list<const SMESHDS_Hypothesis*>& hyps =
    GetUsedHypothesis(aMesh, aShape, /*ignoreAuxiliary=*/false);
  list <const SMESHDS_Hypothesis* >::const_iterator h = hyps.begin();
  if ( h == hyps.end())
  {
    aStatus = SMESH_Hypothesis::HYP_OK;
    return true;
  }

  // only StdMeshers_ViscousLayers can be used
  aStatus = HYP_OK;
  for ( ; h != hyps.end(); ++h )
  {
    if ( !_viscousLayersHyp &&
         (_viscousLayersHyp = dynamic_cast< const StdMeshers_ViscousLayers*> ( *h )))
      continue;
    if ( !_blockRenumberHyp &&
         (_blockRenumberHyp = dynamic_cast< const StdMeshers_BlockRenumber*> ( *h )))
      continue;
    break;
  }
  if ((int) hyps.size() != (bool)_viscousLayersHyp + (bool)_blockRenumberHyp )
    aStatus = HYP_INCOMPATIBLE;
  else
  {
    if ( _viscousLayersHyp )
      if ( !error( _viscousLayersHyp->CheckHypothesis( aMesh, aShape, aStatus )))
        aStatus = HYP_BAD_PARAMETER;

    if ( _blockRenumberHyp && aStatus == HYP_OK )
      error( _blockRenumberHyp->CheckHypothesis( aMesh, aShape ));
  }

  return aStatus == HYP_OK;
}

namespace
{
  //=============================================================================

  typedef boost::shared_ptr< FaceQuadStruct > FaceQuadStructPtr;
  typedef std::vector<gp_XYZ>                 TXYZColumn;

  // symbolic names of box sides
  enum EBoxSides{ B_BOTTOM=0, B_RIGHT, B_TOP, B_LEFT, B_FRONT, B_BACK, B_NB_SIDES };

  // symbolic names of sides of quadrangle
  enum EQuadSides{ Q_BOTTOM=0, Q_RIGHT, Q_TOP, Q_LEFT, Q_NB_SIDES };

  enum EAxes{ COO_X=1, COO_Y, COO_Z };

  //=============================================================================
  /*!
   * \brief Container of nodes of structured mesh on a qudrangular geom FACE
   */
  struct _FaceGrid
  {
    // face sides
    FaceQuadStructPtr _quad;

    // map of (node parameter on EDGE) to (column (vector) of nodes)
    TParam2ColumnMap _u2nodesMap;

    // node column's taken from _u2nodesMap taking into account sub-shape orientation
    vector<TNodeColumn> _columns;

    // columns of normalized parameters of nodes within the unitary cube
    vector<TXYZColumn> _ijkColumns;

    // geometry of a cube side
    TopoDS_Face _sideF;

    const SMDS_MeshNode* GetNode(int iCol, int iRow) const
    {
      return _columns[iCol][iRow];
    }
    gp_XYZ GetXYZ(int iCol, int iRow) const
    {
      return SMESH_TNodeXYZ( GetNode( iCol, iRow ));
    }
    gp_XYZ& GetIJK(int iCol, int iRow)
    {
      return _ijkColumns[iCol][iRow];
    }
  };

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
   * \brief Finds FaceQuadStruct having a side equal to a given one and rearranges
   *  the found FaceQuadStruct::side to have the given side at a Q_BOTTOM place
   */
  FaceQuadStructPtr getQuadWithBottom( StdMeshers_FaceSidePtr side,
                                       FaceQuadStructPtr      quad[ 6 ])
  {
    FaceQuadStructPtr foundQuad;
    for ( int i = 1; i < 6; ++i )
    {
      if ( !quad[i] ) continue;
      for ( size_t iS = 0; iS < quad[i]->side.size(); ++iS )
      {
        const StdMeshers_FaceSidePtr side2 = quad[i]->side[iS];
        if (( side->FirstVertex().IsSame( side2->FirstVertex() ) ||
              side->FirstVertex().IsSame( side2->LastVertex() ))
            &&
            ( side->LastVertex().IsSame( side2->FirstVertex() ) ||
              side->LastVertex().IsSame( side2->LastVertex() ))
            )
        {
          if ( iS != Q_BOTTOM )
          {
            vector< FaceQuadStruct::Side > newSides;
            for ( size_t j = iS; j < quad[i]->side.size(); ++j )
              newSides.push_back( quad[i]->side[j] );
            for ( size_t j = 0; j < iS; ++j )
              newSides.push_back( quad[i]->side[j] );
            quad[i]->side.swap( newSides );
          }
          foundQuad.swap(quad[i]);
          return foundQuad;
        }
      }
    }
    return foundQuad;
  }

  //================================================================================
  /*!
   * \brief Put quads to aCubeSide in the order of enum EBoxSides
   */
  //================================================================================

  bool arrangeQuads( FaceQuadStructPtr quad[ 6 ], _FaceGrid aCubeSide[ 6 ], bool reverseBottom )
  {
    swap( aCubeSide[B_BOTTOM]._quad, quad[0] );
    if ( reverseBottom )
      swap( aCubeSide[B_BOTTOM]._quad->side[ Q_RIGHT],// direct the bottom normal inside cube
            aCubeSide[B_BOTTOM]._quad->side[ Q_LEFT ] );

    aCubeSide[B_FRONT]._quad = getQuadWithBottom( aCubeSide[B_BOTTOM]._quad->side[Q_BOTTOM], quad );
    aCubeSide[B_RIGHT]._quad = getQuadWithBottom( aCubeSide[B_BOTTOM]._quad->side[Q_RIGHT ], quad );
    aCubeSide[B_BACK ]._quad = getQuadWithBottom( aCubeSide[B_BOTTOM]._quad->side[Q_TOP   ], quad );
    aCubeSide[B_LEFT ]._quad = getQuadWithBottom( aCubeSide[B_BOTTOM]._quad->side[Q_LEFT  ], quad );
    if ( aCubeSide[B_FRONT ]._quad )
      aCubeSide[B_TOP]._quad = getQuadWithBottom( aCubeSide[B_FRONT ]._quad->side[Q_TOP ], quad );

    for ( int i = 1; i < 6; ++i )
      if ( !aCubeSide[i]._quad )
        return false;
    return true;
  }

  //================================================================================
  /*!
   * \brief Rearrange block sides according to StdMeshers_BlockRenumber hypothesis
   */
  //================================================================================

  bool arrangeForRenumber( _FaceGrid      blockSide[ 6 ],
                           TopoDS_Vertex& v000,
                           TopoDS_Vertex& v001 )
  {
    std::swap( blockSide[B_BOTTOM]._quad->side[ Q_RIGHT],// restore after arrangeQuads()
               blockSide[B_BOTTOM]._quad->side[ Q_LEFT ] );

    // find v000
    TopTools_MapOfShape cornerVertices;
    cornerVertices.Add(  blockSide[B_BOTTOM]._quad->side[Q_BOTTOM].grid->LastVertex()  );
    cornerVertices.Add(  blockSide[B_BOTTOM]._quad->side[Q_BOTTOM].grid->FirstVertex() );
    cornerVertices.Add(  blockSide[B_BOTTOM]._quad->side[Q_TOP   ].grid->LastVertex()  );
    cornerVertices.Add(  blockSide[B_BOTTOM]._quad->side[Q_TOP   ].grid->FirstVertex() );
    cornerVertices.Add(  blockSide[B_TOP   ]._quad->side[Q_BOTTOM].grid->FirstVertex() );
    cornerVertices.Add(  blockSide[B_TOP   ]._quad->side[Q_BOTTOM].grid->LastVertex()  );
    cornerVertices.Add(  blockSide[B_TOP   ]._quad->side[Q_TOP   ].grid->FirstVertex() );
    cornerVertices.Add(  blockSide[B_TOP   ]._quad->side[Q_TOP   ].grid->LastVertex()  );

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

    std::vector< StdMeshers_FaceSidePtr > edgesAtV000;
    std::vector< gp_Vec >                 edgeDir;
    std::vector< int >                    iParallel; // 0 - none, 1 - X, 2 - Y, 3 - Z
    TopTools_MapOfShape                   lastVertices;
    for ( int iQ = 0; iQ < 6; ++iQ )
    {
      FaceQuadStructPtr quad = blockSide[iQ]._quad;
      for ( size_t iS = 0; iS < quad->side.size() &&  edgesAtV000.size() < 3; ++iS )
      {
        StdMeshers_FaceSidePtr edge = quad->side[iS];
        TopoDS_Vertex v1 = edge->FirstVertex(), v2 = edge->LastVertex();
        if (( v1.IsSame( v000 ) && !lastVertices.Contains( v2 )) ||
            ( v2.IsSame( v000 ) && !lastVertices.Contains( v1 )))
        {
          bool reverse = v2.IsSame( v000 );
          if ( reverse )
            std::swap( v1, v2 );
          lastVertices.Add( v2 );

          edgesAtV000.push_back( edge );

          gp_Pnt pf = BRep_Tool::Pnt( v1 );
          gp_Pnt pl = BRep_Tool::Pnt( v2 );
          gp_Vec vec( pf, pl );
          edgeDir.push_back( vec );

          iParallel.push_back( 0 );
          if ( !v001.IsNull() )
          {
            if ( v001.IsSame( v2 ))
              iParallel.back() = 3;
          }
          else
          {
            bool isStraight = true;
            for ( int iE = 0; iE < edge->NbEdges() &&  isStraight; ++iE )
              isStraight = SMESH_Algo::IsStraight( edge->Edge( iE ));

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
              edgeDir.back() = gp_Vec( pf, edge->Value3d( reverse ? 0.99 : 0.01 ));
            }
          }
        }
      }
    }
    if ( std::accumulate( iParallel.begin(), iParallel.end(), 0 ) == 0 )
      return false;

    // find edge OZ and edge OX
    StdMeshers_FaceSidePtr edgeOZ, edgeOX;
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
        break;
      }
    }

    if ( !edgeOZ || !edgeOX )
      return false;

    TopoDS_Vertex v100 = edgeOX->LastVertex();
    if ( v100.IsSame( v000 ))
      v100 = edgeOX->FirstVertex();

    // Find the left quad, one including v000 but not v100

    for ( int iQ = 0; iQ < 6; ++iQ )
    {
      FaceQuadStructPtr quad = blockSide[iQ]._quad;
      bool hasV000 = false, hasV100 = false;
      for ( size_t iS = 0; iS < quad->side.size(); ++iS )
      {
        StdMeshers_FaceSidePtr edge = quad->side[iS];
        if ( edge->FirstVertex().IsSame( v000 ) || edge->LastVertex().IsSame( v000 ))
          hasV000 = true;
        if ( edge->FirstVertex().IsSame( v100 ) || edge->LastVertex().IsSame( v100 ))
          hasV100 = true;
      }
      if ( hasV000 && !hasV100 )
      {
        // orient the left quad
        for ( int i = 0; i < 4; ++i )
        {
          if ( quad->side[Q_BOTTOM].grid->Edge(0).IsSame( edgeOZ->Edge(0) ))
            break;
          quad->shift( 1, true );
        }

        FaceQuadStructPtr quads[ 6 ];
        quads[0].swap( blockSide[iQ]._quad );
        for ( int i = 1, j = 0; i < 6; ++i, ++j )
          if ( blockSide[ j ]._quad )
            quads[ i ].swap( blockSide[ j ]._quad );
          else
            --i;

        return arrangeQuads( quads, blockSide, false/* true*/ );
      }
    }
    return false;
  }

  //================================================================================
  /*!
   * \brief Returns true if the 1st base node of sideGrid1 belongs to sideGrid2
   */
  //================================================================================

  bool beginsAtSide( const _FaceGrid&     sideGrid1,
                     const _FaceGrid&     sideGrid2,
                     SMESH_ProxyMesh::Ptr proxymesh )
  {
    const TNodeColumn& col0  = sideGrid2._u2nodesMap.begin()->second;
    const TNodeColumn& col1  = sideGrid2._u2nodesMap.rbegin()->second;
    const SMDS_MeshNode* n00 = col0.front();
    const SMDS_MeshNode* n01 = col0.back();
    const SMDS_MeshNode* n10 = col1.front();
    const SMDS_MeshNode* n11 = col1.back();
    const SMDS_MeshNode* n = (sideGrid1._u2nodesMap.begin()->second)[0];
    if ( proxymesh )
    {
      n00 = proxymesh->GetProxyNode( n00 );
      n10 = proxymesh->GetProxyNode( n10 );
      n01 = proxymesh->GetProxyNode( n01 );
      n11 = proxymesh->GetProxyNode( n11 );
      n   = proxymesh->GetProxyNode( n );
    }
    return ( n == n00 || n == n01 || n == n10 || n == n11 );
  }

  //================================================================================
  /*!
   * \brief Fill in _FaceGrid::_ijkColumns
   *  \param [in,out] fg - a _FaceGrid
   *  \param [in] i1 - coordinate index along _columns
   *  \param [in] i2 - coordinate index along _columns[i]
   *  \param [in] v3 - value of the constant parameter
   */
  //================================================================================

  void computeIJK( _FaceGrid& fg, int i1, int i2, double v3 )
  {
    gp_XYZ ijk( v3, v3, v3 );
    const size_t nbCol = fg._columns.size();
    const size_t nbRow = fg._columns[0].size();

    fg._ijkColumns.resize( nbCol );
    for ( size_t i = 0; i < nbCol; ++i )
      fg._ijkColumns[ i ].resize( nbRow, ijk );

    vector< double > len( nbRow );
    len[0] = 0;
    for ( size_t i = 0; i < nbCol; ++i )
    {
      gp_Pnt pPrev = fg.GetXYZ( i, 0 );
      for ( size_t j = 1; j < nbRow; ++j )
      {
        gp_Pnt p = fg.GetXYZ( i, j );
        len[ j ] = len[ j-1 ] + p.Distance( pPrev );
        pPrev = p;
      }
      for ( size_t j = 0; j < nbRow; ++j )
        fg.GetIJK( i, j ).SetCoord( i2, len[ j ]/len.back() );
    }

    len.resize( nbCol );
    for ( size_t j = 0; j < nbRow; ++j )
    {
      gp_Pnt pPrev = fg.GetXYZ( 0, j );
      for ( size_t i = 1; i < nbCol; ++i )
      {
        gp_Pnt p = fg.GetXYZ( i, j );
        len[ i ] = len[ i-1 ] + p.Distance( pPrev );
        pPrev = p;
      }
      for ( size_t i = 0; i < nbCol; ++i )
        fg.GetIJK( i, j ).SetCoord( i1, len[ i ]/len.back() );
    }
  }
}

//=============================================================================
/*!
 * Generates hexahedron mesh on hexaedron like form using algorithm from
 * "Application de l'interpolation transfinie � la cr�ation de maillages
 *  C0 ou G1 continus sur des triangles, quadrangles, tetraedres, pentaedres
 *  et hexaedres d�form�s."
 * Alain PERONNET - 8 janvier 1999
 */
//=============================================================================

bool StdMeshers_Hexa_3D::Compute(SMESH_Mesh &         aMesh,
                                 const TopoDS_Shape & aShape)
{
  // PAL14921. Enable catching std::bad_alloc and Standard_OutOfMemory outside
  //Unexpect aCatch(SalomeException);
  SMESHDS_Mesh * meshDS = aMesh.GetMeshDS();

  // Shape verification
  // ----------------------

  // shape must be a solid (or a shell) with 6 faces
  TopExp_Explorer exp(aShape,TopAbs_SHELL);
  if ( !exp.More() )
    return error(COMPERR_BAD_SHAPE, "No SHELL in the geometry");
  if ( exp.Next(), exp.More() )
    return error(COMPERR_BAD_SHAPE, "More than one SHELL in the geometry");

  TopTools_IndexedMapOfShape FF, EE;
  TopExp::MapShapes( aShape, TopAbs_FACE, FF);
  if ( FF.Extent() != 6)
  {
    static StdMeshers_CompositeHexa_3D compositeHexa(_gen->GetANewId(), _gen);
    compositeHexa.SetHypothesis( _blockRenumberHyp );
    if ( !compositeHexa.Compute( aMesh, aShape ))
      return error( compositeHexa.GetComputeError() );

    return _blockRenumberHyp ? error( _blockRenumberHyp->CheckHypothesis( aMesh, aShape )) : true;
  }

  // Find sides of a cube
  // ---------------------

  // tool creating quadratic elements if needed
  SMESH_MesherHelper helper (aMesh);
  _quadraticMesh = helper.IsQuadraticSubMesh(aShape);

  TopExp::MapShapes( aShape, TopAbs_EDGE, EE );
  SMESH_MesherHelper* faceHelper = ( EE.Size() == 12 ) ? 0 : &helper;

  FaceQuadStructPtr quad[ 6 ];
  for ( int i = 0; i < 6; ++i )
  {
    if ( faceHelper )
      faceHelper->SetSubShape( FF( i+1 ));
    if ( !( quad[i] = FaceQuadStructPtr( _quadAlgo->CheckNbEdges( aMesh, FF( i+1 ),
                                                                  /*considerMesh=*/true,
                                                                  faceHelper))))
      return error( _quadAlgo->GetComputeError() );
    if ( quad[i]->side.size() != 4 )
      return error( COMPERR_BAD_SHAPE, "Not a quadrangular box side" );
  }

  // put quads in a proper order
  _FaceGrid aCubeSide[ 6 ];
  if ( !arrangeQuads( quad, aCubeSide, true ))
    return error( COMPERR_BAD_SHAPE );


  // Make viscous layers
  // --------------------

  SMESH_ProxyMesh::Ptr proxymesh;
  if ( _viscousLayersHyp )
  {
    proxymesh = _viscousLayersHyp->Compute( aMesh, aShape, /*makeN2NMap=*/ true );
    if ( !proxymesh )
      return false;
  }

  // Check if there are triangles on cube sides
  // -------------------------------------------

  if ( aMesh.NbTriangles() > 0 )
  {
    for ( int i = 0; i < 6; ++i )
    {
      const TopoDS_Face& sideF = aCubeSide[i]._quad->face;
      const SMESHDS_SubMesh* smDS =
        proxymesh ? proxymesh->GetSubMesh( sideF ) : meshDS->MeshElements( sideF );
      if ( !SMESH_MesherHelper::IsSameElemGeometry( smDS, SMDSGeom_QUADRANGLE,
                                                    /*nullSubMeshRes=*/false ))
      {
        SMESH_ComputeErrorPtr err = ComputePentahedralMesh(aMesh, aShape, proxymesh.get());
        return error( err );
      }
    }
  }

  // Arrange sides according to _blockRenumberHyp
  bool toRenumber = _blockRenumberHyp;
  if ( toRenumber )
  {
    TopoDS_Vertex v000, v001;
    _blockRenumberHyp->IsSolidIncluded( aMesh, aShape, v000, v001 );

    toRenumber = arrangeForRenumber( aCubeSide, v000, v001 );

    if ( toRenumber )
    {
      meshDS->Modified();
      meshDS->CompactMesh(); // remove numbering holes
    }
  }

  // Check presence of regular grid mesh on FACEs of the cube
  // ------------------------------------------------------------

  for ( int i = 0; i < 6; ++i )
  {
    const TopoDS_Face& F = aCubeSide[i]._quad->face;
    StdMeshers_FaceSidePtr baseQuadSide = aCubeSide[i]._quad->side[ Q_BOTTOM ];
    list<TopoDS_Edge> baseEdges( baseQuadSide->Edges().begin(), baseQuadSide->Edges().end() );

    // assure correctness of node positions on baseE:
    // helper.GetNodeU() will fix positions if they are wrong
    helper.ToFixNodeParameters( true );
    for ( int iE = 0; iE < baseQuadSide->NbEdges(); ++iE )
    {
      const TopoDS_Edge& baseE = baseQuadSide->Edge( iE );
      if ( SMESHDS_SubMesh* smDS = meshDS->MeshElements( baseE ))
      {
        bool ok;
        helper.SetSubShape( baseE );
        SMDS_ElemIteratorPtr eIt = smDS->GetElements();
        while ( eIt->more() )
        {
          const SMDS_MeshElement* e = eIt->next();
          // expect problems on a composite side
          try { helper.GetNodeU( baseE, e->GetNode(0), e->GetNode(1), &ok); }
          catch (...) {}
          try { helper.GetNodeU( baseE, e->GetNode(1), e->GetNode(0), &ok); }
          catch (...) {}
        }
      }
    }

    // load grid
    bool ok =
      helper.LoadNodeColumns( aCubeSide[i]._u2nodesMap, F, baseEdges, meshDS, proxymesh.get());
    if ( ok )
    {
      // check if the loaded grid corresponds to nb of quadrangles on the FACE
      const SMESHDS_SubMesh* faceSubMesh =
        proxymesh ? proxymesh->GetSubMesh( F ) : meshDS->MeshElements( F );
      const smIdType nbQuads = faceSubMesh->NbElements();
      const int nbHor = aCubeSide[i]._u2nodesMap.size() - 1;
      const int nbVer = aCubeSide[i]._u2nodesMap.begin()->second.size() - 1;
      ok = ( nbQuads == nbHor * nbVer );
    }
    if ( !ok )
    {
      SMESH_ComputeErrorPtr err = ComputePentahedralMesh(aMesh, aShape, proxymesh.get());
      return error( err );
    }
  }

  // Orient loaded grids of cube sides along axis of the unitary cube coord system
  bool isReverse[6];
  isReverse[B_BOTTOM] = beginsAtSide( aCubeSide[B_BOTTOM], aCubeSide[B_RIGHT ], proxymesh );
  isReverse[B_TOP   ] = beginsAtSide( aCubeSide[B_TOP   ], aCubeSide[B_RIGHT ], proxymesh );
  isReverse[B_FRONT ] = beginsAtSide( aCubeSide[B_FRONT ], aCubeSide[B_RIGHT ], proxymesh );
  isReverse[B_BACK  ] = beginsAtSide( aCubeSide[B_BACK  ], aCubeSide[B_RIGHT ], proxymesh );
  isReverse[B_LEFT  ] = beginsAtSide( aCubeSide[B_LEFT  ], aCubeSide[B_BACK  ], proxymesh );
  isReverse[B_RIGHT ] = beginsAtSide( aCubeSide[B_RIGHT ], aCubeSide[B_BACK  ], proxymesh );
  for ( int i = 0; i < 6; ++i )
  {
    aCubeSide[i]._columns.resize( aCubeSide[i]._u2nodesMap.size() );

    size_t iFwd = 0, iRev = aCubeSide[i]._columns.size()-1;
    size_t*  pi = isReverse[i] ? &iRev : &iFwd;
    TParam2ColumnMap::iterator u2nn = aCubeSide[i]._u2nodesMap.begin();
    for ( ; iFwd < aCubeSide[i]._columns.size(); --iRev, ++iFwd, ++u2nn )
      aCubeSide[i]._columns[ *pi ].swap( u2nn->second );

    aCubeSide[i]._u2nodesMap.clear();
  }

  if ( proxymesh )
    for ( int i = 0; i < 6; ++i )
      for ( size_t j = 0; j < aCubeSide[i]._columns.size(); ++j)
        for ( size_t k = 0; k < aCubeSide[i]._columns[j].size(); ++k)
        {
          const SMDS_MeshNode* & n = aCubeSide[i]._columns[j][k];
          n = proxymesh->GetProxyNode( n );
        }

  // 4) Create internal nodes of the cube
  // -------------------------------------

  helper.SetSubShape( aShape );
  helper.SetElementsOnShape(true);

  // shortcuts to sides
  _FaceGrid* fBottom = & aCubeSide[ B_BOTTOM ];
  _FaceGrid* fRight  = & aCubeSide[ B_RIGHT  ];
  _FaceGrid* fTop    = & aCubeSide[ B_TOP    ];
  _FaceGrid* fLeft   = & aCubeSide[ B_LEFT   ];
  _FaceGrid* fFront  = & aCubeSide[ B_FRONT  ];
  _FaceGrid* fBack   = & aCubeSide[ B_BACK   ];

  // cube size measured in nb of nodes
  size_t x, xSize = fBottom->_columns.size() , X = xSize - 1;
  size_t y, ySize = fLeft->_columns.size()   , Y = ySize - 1;
  size_t z, zSize = fLeft->_columns[0].size(), Z = zSize - 1;

  // check sharing of FACEs (IPAL54417)
  if ( fFront ->_columns.size()    != xSize ||
       fBack  ->_columns.size()    != xSize ||
       fTop   ->_columns.size()    != xSize ||

       fRight ->_columns.size()    != ySize ||
       fTop   ->_columns[0].size() != ySize ||
       fBottom->_columns[0].size() != ySize ||

       fRight ->_columns[0].size() != zSize ||
       fFront ->_columns[0].size() != zSize ||
       fBack  ->_columns[0].size() != zSize )
    return error( COMPERR_BAD_SHAPE, "Not sewed faces" );

  // columns of internal nodes "rising" from nodes of fBottom
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

  // compute normalized parameters of nodes on sides (PAL23189)
  computeIJK( *fBottom, COO_X, COO_Y, /*z=*/0. );
  computeIJK( *fRight,  COO_Y, COO_Z, /*x=*/1. );
  computeIJK( *fTop,    COO_X, COO_Y, /*z=*/1. );
  computeIJK( *fLeft,   COO_Y, COO_Z, /*x=*/0. );
  computeIJK( *fFront,  COO_X, COO_Z, /*y=*/0. );
  computeIJK( *fBack,   COO_X, COO_Z, /*y=*/1. );

  StdMeshers_RenumberHelper renumHelper( aMesh, _blockRenumberHyp );

  // projection points of the internal node on cube sub-shapes by which
  // coordinates of the internal node are computed
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
      vector< const SMDS_MeshNode* >& column0y = columns[ colIndex( 0, y )];
      for ( z = 0; z < zSize; ++z )
        renumHelper.AddReplacingNode( column0y[ z ] );
    }

  for ( x = 1; x < xSize-1; ++x )
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

      // a column to fill in during z loop
      vector< const SMDS_MeshNode* >& column = columns[ colIndex( x, y )];
      // projection points on horizontal edges
      pointsOnShapes[ SMESH_Block::ID_Ex00 ] = fBottom->GetXYZ( x, 0 );
      pointsOnShapes[ SMESH_Block::ID_Ex10 ] = fBottom->GetXYZ( x, Y );
      pointsOnShapes[ SMESH_Block::ID_E0y0 ] = fBottom->GetXYZ( 0, y );
      pointsOnShapes[ SMESH_Block::ID_E1y0 ] = fBottom->GetXYZ( X, y );
      pointsOnShapes[ SMESH_Block::ID_Ex01 ] = fTop->GetXYZ( x, 0 );
      pointsOnShapes[ SMESH_Block::ID_Ex11 ] = fTop->GetXYZ( x, Y );
      pointsOnShapes[ SMESH_Block::ID_E0y1 ] = fTop->GetXYZ( 0, y );
      pointsOnShapes[ SMESH_Block::ID_E1y1 ] = fTop->GetXYZ( X, y );
      // projection points on horizontal faces
      pointsOnShapes[ SMESH_Block::ID_Fxy0 ] = fBottom->GetXYZ( x, y );
      pointsOnShapes[ SMESH_Block::ID_Fxy1 ] = fTop   ->GetXYZ( x, y );

      if ( toRenumber )
        renumHelper.AddReplacingNode( column[ 0 ] );

      for ( z = 1; z < zSize-1; ++z ) // z loop
      {
        const double rZ = z / double(Z);

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

        // projection points on vertical edges
        pointsOnShapes[ SMESH_Block::ID_E00z ] = fFront->GetXYZ( 0, z );
        pointsOnShapes[ SMESH_Block::ID_E10z ] = fFront->GetXYZ( X, z );
        pointsOnShapes[ SMESH_Block::ID_E01z ] = fBack->GetXYZ( 0, z );
        pointsOnShapes[ SMESH_Block::ID_E11z ] = fBack->GetXYZ( X, z );
        // projection points on vertical faces
        pointsOnShapes[ SMESH_Block::ID_Fx0z ] = fFront->GetXYZ( x, z );
        pointsOnShapes[ SMESH_Block::ID_Fx1z ] = fBack ->GetXYZ( x, z );
        pointsOnShapes[ SMESH_Block::ID_F0yz ] = fLeft ->GetXYZ( y, z );
        pointsOnShapes[ SMESH_Block::ID_F1yz ] = fRight->GetXYZ( y, z );

        // compute internal node coordinates
        gp_XYZ coords;
        SMESH_Block::ShellPoint( params, pointsOnShapes, coords );
        column[ z ] = helper.AddNode( coords.X(), coords.Y(), coords.Z() );            

      } // z loop
      if ( toRenumber )
        renumHelper.AddReplacingNode( column[ Z ] );

    } // y loop
    if ( toRenumber )
    {
      vector< const SMDS_MeshNode* >& columnX0 = columns[ colIndex( x, Y )];
      for ( z = 0; z < zSize; ++z )
        renumHelper.AddReplacingNode( columnX0[ z ] );
    }
  } // x loop

  if ( toRenumber )
    for ( y = 0; y < ySize; ++y )
    {
      vector< const SMDS_MeshNode* >& columnXy = columns[ colIndex( X, y )];
      for ( z = 0; z < zSize; ++z )
        renumHelper.AddReplacingNode( columnXy[ z ] );
    }

  // side data no more needed, free memory
  for ( int i = 0; i < 6; ++i )
    SMESHUtils::FreeVector( aCubeSide[i]._columns );

  // 5) Create hexahedrons
  // ---------------------
  for ( x = 0; x < xSize-1; ++x ) {
    for ( y = 0; y < ySize-1; ++y ) {
      vector< const SMDS_MeshNode* >& col00 = columns[ colIndex( x, y )];
      vector< const SMDS_MeshNode* >& col10 = columns[ colIndex( x+1, y )];
      vector< const SMDS_MeshNode* >& col01 = columns[ colIndex( x, y+1 )];
      vector< const SMDS_MeshNode* >& col11 = columns[ colIndex( x+1, y+1 )];
      for ( z = 0; z < zSize-1; ++z )
      {
        // bottom face normal of a hexa mush point outside the volume
        if ( toRenumber )    
        {
          helper.AddVolume(col00[z], col01[z], col01[z+1], col00[z+1],
                           col10[z], col11[z], col11[z+1], col10[z+1]);                  
        }              
        else
        {
          helper.AddVolume(col00[z],   col01[z],   col11[z],   col10[z],
                           col00[z+1], col01[z+1], col11[z+1], col10[z+1]);          
        }            
      }
    }
  }

  if ( toRenumber )
    renumHelper.DoReplaceNodes();

  meshDS->SetStructuredGrid( aShape, zSize, ySize, xSize );
  for ( x = 0; x < xSize; ++x )
    for ( y = 0; y < ySize; ++y )
      for ( z = 0; z < zSize; ++z )
        meshDS->SetNodeOnStructuredGrid( aShape, columns[colIndex(x,y)][z], z, y, x );

  if ( _blockRenumberHyp )
  {
    return error( _blockRenumberHyp->CheckHypothesis( aMesh, aShape ));
  }

  return true;
}

//=============================================================================
/*!
 *  Evaluate
 */
//=============================================================================

bool StdMeshers_Hexa_3D::Evaluate(SMESH_Mesh & aMesh,
                                  const TopoDS_Shape & aShape,
                                  MapShapeNbElems& aResMap)
{
  vector < SMESH_subMesh * >meshFaces;
  TopTools_SequenceOfShape aFaces;
  for (TopExp_Explorer exp(aShape, TopAbs_FACE); exp.More(); exp.Next()) {
    aFaces.Append(exp.Current());
    SMESH_subMesh *aSubMesh = aMesh.GetSubMeshContaining(exp.Current());
    ASSERT(aSubMesh);
    meshFaces.push_back(aSubMesh);
  }
  if (meshFaces.size() != 6) {
    //return error(COMPERR_BAD_SHAPE, TComm(meshFaces.size())<<" instead of 6 faces in a block");
    static StdMeshers_CompositeHexa_3D compositeHexa(-10, aMesh.GetGen());
    return compositeHexa.Evaluate(aMesh, aShape, aResMap);
  }
  
  int i = 0;
  for(; i<6; i++) {
    //TopoDS_Shape aFace = meshFaces[i]->GetSubShape();
    TopoDS_Shape aFace = aFaces.Value(i+1);
    SMESH_Algo *algo = _gen->GetAlgo(aMesh, aFace);
    if( !algo ) {
      std::vector<smIdType> aResVec(SMDSEntity_Last);
      for(int i=SMDSEntity_Node; i<SMDSEntity_Last; i++) aResVec[i] = 0;
      SMESH_subMesh * sm = aMesh.GetSubMesh(aShape);
      aResMap.insert(std::make_pair(sm,aResVec));
      SMESH_ComputeErrorPtr& smError = sm->GetComputeError();
      smError.reset( new SMESH_ComputeError(COMPERR_ALGO_FAILED,"Submesh can not be evaluated",this));
      return false;
    }
    string algoName = algo->GetName();
    bool isAllQuad = false;
    if (algoName == "Quadrangle_2D") {
      MapShapeNbElemsItr anIt = aResMap.find(meshFaces[i]);
      if( anIt == aResMap.end() ) continue;
      std::vector<smIdType> aVec = (*anIt).second;
      smIdType nbtri = std::max(aVec[SMDSEntity_Triangle],aVec[SMDSEntity_Quad_Triangle]);
      if( nbtri == 0 )
        isAllQuad = true;
    }
    if ( ! isAllQuad ) {
      return EvaluatePentahedralMesh(aMesh, aShape, aResMap);
    }
  }
  
  // find number of 1d elems for 1 face
  int nb1d = 0;
  TopTools_MapOfShape Edges1;
  bool IsQuadratic = false;
  bool IsFirst = true;
  for (TopExp_Explorer exp(aFaces.Value(1), TopAbs_EDGE); exp.More(); exp.Next()) {
    Edges1.Add(exp.Current());
    SMESH_subMesh *sm = aMesh.GetSubMesh(exp.Current());
    if( sm ) {
      MapShapeNbElemsItr anIt = aResMap.find(sm);
      if( anIt == aResMap.end() ) continue;
      std::vector<smIdType> aVec = (*anIt).second;
      nb1d += std::max(aVec[SMDSEntity_Edge],aVec[SMDSEntity_Quad_Edge]);
      if(IsFirst) {
        IsQuadratic = (aVec[SMDSEntity_Quad_Edge] > aVec[SMDSEntity_Edge]);
        IsFirst = false;
      }
    }
  }
  // find face opposite to 1 face
  int OppNum = 0;
  for(i=2; i<=6; i++) {
    bool IsOpposite = true;
    for(TopExp_Explorer exp(aFaces.Value(i), TopAbs_EDGE); exp.More(); exp.Next()) {
      if( Edges1.Contains(exp.Current()) ) {
        IsOpposite = false;
        break;
      }
    }
    if(IsOpposite) {
      OppNum = i;
      break;
    }
  }
  // find number of 2d elems on side faces
  int nb2d = 0;
  for(i=2; i<=6; i++) {
    if( i == OppNum ) continue;
    MapShapeNbElemsItr anIt = aResMap.find( meshFaces[i-1] );
    if( anIt == aResMap.end() ) continue;
    std::vector<smIdType> aVec = (*anIt).second;
    nb2d += std::max(aVec[SMDSEntity_Quadrangle],aVec[SMDSEntity_Quad_Quadrangle]);
  }
  
  MapShapeNbElemsItr anIt = aResMap.find( meshFaces[0] );
  std::vector<smIdType> aVec = (*anIt).second;
  smIdType nb2d_face0 = std::max(aVec[SMDSEntity_Quadrangle],aVec[SMDSEntity_Quad_Quadrangle]);
  smIdType nb0d_face0 = aVec[SMDSEntity_Node];

  std::vector<smIdType> aResVec(SMDSEntity_Last);
  for(int i=SMDSEntity_Node; i<SMDSEntity_Last; i++) aResVec[i] = 0;
  if(IsQuadratic) {
    aResVec[SMDSEntity_Quad_Hexa] = nb2d_face0 * ( nb2d/nb1d );
    smIdType nb1d_face0_int = ( nb2d_face0*4 - nb1d ) / 2;
    aResVec[SMDSEntity_Node] = nb0d_face0 * ( 2*nb2d/nb1d - 1 ) - nb1d_face0_int * nb2d/nb1d;
  }
  else {
    aResVec[SMDSEntity_Node] = nb0d_face0 * ( nb2d/nb1d - 1 );
    aResVec[SMDSEntity_Hexa] = nb2d_face0 * ( nb2d/nb1d );
  }
  SMESH_subMesh * sm = aMesh.GetSubMesh(aShape);
  aResMap.insert(std::make_pair(sm,aResVec));

  return true;
}

//================================================================================
/*!
 * \brief Computes hexahedral mesh from 2D mesh of block
 */
//================================================================================

bool StdMeshers_Hexa_3D::Compute(SMESH_Mesh & aMesh, SMESH_MesherHelper* aHelper)
{
  static StdMeshers_HexaFromSkin_3D * algo = 0;
  if ( !algo ) {
    SMESH_Gen* gen = aMesh.GetGen();
    algo = new StdMeshers_HexaFromSkin_3D( gen->GetANewId(), gen );
  }
  algo->InitComputeError();
  algo->Compute( aMesh, aHelper );
  return error( algo->GetComputeError());
}

//================================================================================
/*!
 * \brief Return true if the algorithm can mesh this shape
 *  \param [in] aShape - shape to check
 *  \param [in] toCheckAll - if true, this check returns OK if all shapes are OK,
 *              else, returns OK if at least one shape is OK
 */
//================================================================================

bool StdMeshers_Hexa_3D::IsApplicable( const TopoDS_Shape & aShape, bool toCheckAll )
{
  TopExp_Explorer exp0( aShape, TopAbs_SOLID );
  if ( !exp0.More() ) return false;

  for ( ; exp0.More(); exp0.Next() )
  {
    int nbFoundShells = 0;
    TopExp_Explorer exp1( exp0.Current(), TopAbs_SHELL );
    for ( ; exp1.More(); exp1.Next(), ++nbFoundShells)
      if ( nbFoundShells == 2 ) break;
    if ( nbFoundShells != 1 ) {
      if ( toCheckAll ) return false;
      continue;
    }
    exp1.Init( exp0.Current(), TopAbs_FACE );
    int nbEdges = SMESH_MesherHelper::Count( exp1.Current(), TopAbs_EDGE, /*ignoreSame=*/true );
    bool ok = ( nbEdges > 3 );
    if ( toCheckAll && !ok ) return false;
    if ( !toCheckAll && ok ) return true;
  }
  return toCheckAll;
}

//=======================================================================
//function : ComputePentahedralMesh
//purpose  :
//=======================================================================

SMESH_ComputeErrorPtr ComputePentahedralMesh(SMESH_Mesh &          aMesh,
                                             const TopoDS_Shape &  aShape,
                                             SMESH_ProxyMesh*      proxyMesh)
{
  SMESH_ComputeErrorPtr err = SMESH_ComputeError::New();

  bool bOK;
  StdMeshers_Penta_3D anAlgo;
  //
  bOK=anAlgo.Compute(aMesh, aShape);
  //
  err = anAlgo.GetComputeError();
  //
  if ( !bOK && anAlgo.ErrorStatus() == 5 )
  {
    static StdMeshers_Prism_3D * aPrism3D = 0;
    if ( !aPrism3D ) {
      SMESH_Gen* gen = aMesh.GetGen();
      aPrism3D = new StdMeshers_Prism_3D( gen->GetANewId(), gen );
    }
    SMESH_Hypothesis::Hypothesis_Status aStatus;
    if ( aPrism3D->CheckHypothesis( aMesh, aShape, aStatus ) ) {
      aPrism3D->InitComputeError();
      bOK = aPrism3D->Compute( aMesh, aShape );
      err = aPrism3D->GetComputeError();
    }
  }
  if ( !bOK && proxyMesh )
  {
    // check if VL elements are present on block FACEs
    bool hasVLonFace = false;
    for ( TopExp_Explorer exp( aShape, TopAbs_FACE ); exp.More(); exp.Next() )
    {
      const SMESHDS_SubMesh* sm1 = aMesh.GetSubMesh( exp.Current() )->GetSubMeshDS();
      const SMESHDS_SubMesh* sm2 = proxyMesh->GetSubMesh( exp.Current() );
      if (( hasVLonFace = ( sm2 && sm1->NbElements() != sm2->NbElements() )))
        break;
    }
    if ( hasVLonFace )
    {
      err->myName = COMPERR_BAD_INPUT_MESH;
      err->myComment = "Can't build pentahedral mesh on viscous layers";
    }
  }

  return err;
}


//=======================================================================
//function : EvaluatePentahedralMesh
//purpose  :
//=======================================================================

bool EvaluatePentahedralMesh(SMESH_Mesh & aMesh,
                             const TopoDS_Shape & aShape,
                             MapShapeNbElems& aResMap)
{
  StdMeshers_Penta_3D anAlgo;
  bool bOK = anAlgo.Evaluate(aMesh, aShape, aResMap);

  //err = anAlgo.GetComputeError();
  //if ( !bOK && anAlgo.ErrorStatus() == 5 )
  if( !bOK ) {
    static StdMeshers_Prism_3D * aPrism3D = 0;
    if ( !aPrism3D ) {
      SMESH_Gen* gen = aMesh.GetGen();
      aPrism3D = new StdMeshers_Prism_3D( gen->GetANewId(), gen );
    }
    SMESH_Hypothesis::Hypothesis_Status aStatus;
    if ( aPrism3D->CheckHypothesis( aMesh, aShape, aStatus ) ) {
      return aPrism3D->Evaluate(aMesh, aShape, aResMap);
    }
  }

  return bOK;
}
