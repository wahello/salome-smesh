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
// File      : StdMeshers_Projection_2D.cxx
// Module    : SMESH
// Created   : Fri Oct 20 11:37:07 2006
// Author    : Edward AGAPOV (eap)
//
#include "StdMeshers_Projection_2D.hxx"

#include "StdMeshers_FaceSide.hxx"
#include "StdMeshers_NumberOfSegments.hxx"
#include "StdMeshers_ProjectionSource2D.hxx"
#include "StdMeshers_ProjectionUtils.hxx"
#include "StdMeshers_Quadrangle_2D.hxx"
#include "StdMeshers_Regular_1D.hxx"

#include <ObjectPool.hxx>
#include <SMDS_EdgePosition.hxx>
#include <SMDS_FacePosition.hxx>
#include <SMESHDS_Hypothesis.hxx>
#include <SMESHDS_Mesh.hxx>
#include <SMESHDS_SubMesh.hxx>
#include <SMESH_Block.hxx>
#include <SMESH_Comment.hxx>
#include <SMESH_Gen.hxx>
#include <SMESH_Mesh.hxx>
#include <SMESH_SequentialMesh.hxx>
#include <SMESH_MeshAlgos.hxx>
#include <SMESH_MeshEditor.hxx>
#include <SMESH_MesherHelper.hxx>
#include <SMESH_Pattern.hxx>
#include <SMESH_subMesh.hxx>
#include <SMESH_subMeshEventListener.hxx>

#include <utilities.h>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepMesh_Delaun.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_B2d.hxx>
#include <GeomAPI_ExtremaCurveCurve.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <GeomAdaptor_Surface.hxx>

#include <Basics_OCCTVersion.hxx>

#if OCC_VERSION_LARGE < 0x07070000
#include <GeomAdaptor_HCurve.hxx>
#include <GeomAdaptor_HSurface.hxx>
#endif

#include <GeomLib_IsPlanarSurface.hxx>
#include <Geom_Line.hxx>
#include <IntCurveSurface_HInter.hxx>
#include <Precision.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_DataMapIteratorOfDataMapOfShapeShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopTools_MapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Solid.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax3.hxx>
#include <gp_GTrsf.hxx>


using namespace std;

#define RETURN_BAD_RESULT(msg) { MESSAGE(")-: Error: " << msg); return false; }
#ifdef _DEBUG_
// enable printing algo + projection shapes while meshing
//#define PRINT_WHO_COMPUTE_WHAT
#endif

namespace TAssocTool = StdMeshers_ProjectionUtils;
//typedef StdMeshers_ProjectionUtils TAssocTool;

// allow range iteration on NCollection_IndexedMap
template < class IMAP >
typename IMAP::const_iterator begin( const IMAP &  m ) { return m.cbegin(); }
template < class IMAP >
typename IMAP::const_iterator end( const IMAP &  m ) { return m.cend(); }

//=======================================================================
//function : StdMeshers_Projection_2D
//purpose  :
//=======================================================================

StdMeshers_Projection_2D::StdMeshers_Projection_2D(int hypId, SMESH_Gen* gen)
  :SMESH_2D_Algo(hypId, gen)
{
  _name = "Projection_2D";
  _compatibleHypothesis.push_back("ProjectionSource2D");
  _sourceHypo = 0;
}

//================================================================================
/*!
 * \brief Destructor
 */
//================================================================================

StdMeshers_Projection_2D::~StdMeshers_Projection_2D()
{}

//=======================================================================
//function : CheckHypothesis
//purpose  :
//=======================================================================

bool StdMeshers_Projection_2D::CheckHypothesis(SMESH_Mesh&                          theMesh,
                                               const TopoDS_Shape&                  theShape,
                                               SMESH_Hypothesis::Hypothesis_Status& theStatus)
{
  list <const SMESHDS_Hypothesis * >::const_iterator itl;

  const list <const SMESHDS_Hypothesis * >&hyps = GetUsedHypothesis(theMesh, theShape);
  if ( hyps.size() == 0 )
  {
    theStatus = HYP_MISSING;
    return false;  // can't work with no hypothesis
  }

  if ( hyps.size() > 1 )
  {
    theStatus = HYP_ALREADY_EXIST;
    return false;
  }

  const SMESHDS_Hypothesis *theHyp = hyps.front();

  string hypName = theHyp->GetName();

  theStatus = HYP_OK;

  if (hypName == "ProjectionSource2D")
  {
    _sourceHypo = static_cast<const StdMeshers_ProjectionSource2D *>(theHyp);

    // Check hypo parameters

    SMESH_Mesh* srcMesh = _sourceHypo->GetSourceMesh();
    SMESH_Mesh* tgtMesh = & theMesh;
    if ( !srcMesh )
      srcMesh = tgtMesh;

    // check vertices
    if ( _sourceHypo->HasVertexAssociation() )
    {
      // source vertices
      TopoDS_Shape edge = TAssocTool::GetEdgeByVertices
        ( srcMesh, _sourceHypo->GetSourceVertex(1), _sourceHypo->GetSourceVertex(2) );
      if ( edge.IsNull() ||
           !SMESH_MesherHelper::IsSubShape( edge, srcMesh ) ||
           !SMESH_MesherHelper::IsSubShape( edge, _sourceHypo->GetSourceFace() ))
      {
        theStatus = HYP_BAD_PARAMETER;
        error("Invalid source vertices");
        SCRUTE((edge.IsNull()));
        SCRUTE((SMESH_MesherHelper::IsSubShape( edge, srcMesh )));
        SCRUTE((SMESH_MesherHelper::IsSubShape( edge, _sourceHypo->GetSourceFace() )));
      }
      else
      {
        // target vertices
        edge = TAssocTool::GetEdgeByVertices
          ( tgtMesh, _sourceHypo->GetTargetVertex(1), _sourceHypo->GetTargetVertex(2) );
        if ( edge.IsNull() || !SMESH_MesherHelper::IsSubShape( edge, tgtMesh ))
        {
          theStatus = HYP_BAD_PARAMETER;
          error("Invalid target vertices");
          SCRUTE((edge.IsNull()));
          SCRUTE((SMESH_MesherHelper::IsSubShape( edge, tgtMesh )));
        }
        // PAL16203
        else if ( !_sourceHypo->IsCompoundSource() &&
                  !SMESH_MesherHelper::IsSubShape( edge, theShape ))
        {
          theStatus = HYP_BAD_PARAMETER;
          error("Invalid target vertices");
          SCRUTE((SMESH_MesherHelper::IsSubShape( edge, theShape )));
        }
      }
    }
    // check a source face
    if ( !SMESH_MesherHelper::IsSubShape( _sourceHypo->GetSourceFace(), srcMesh ) ||
         ( srcMesh == tgtMesh && theShape == _sourceHypo->GetSourceFace() ))
    {
      theStatus = HYP_BAD_PARAMETER;
      error("Invalid source face");
      SCRUTE((SMESH_MesherHelper::IsSubShape( _sourceHypo->GetSourceFace(), srcMesh )));
      SCRUTE((srcMesh == tgtMesh));
      SCRUTE(( theShape == _sourceHypo->GetSourceFace() ));
    }
  }
  else
  {
    theStatus = HYP_INCOMPATIBLE;
  }
  return ( theStatus == HYP_OK );
}

namespace {

  //================================================================================
  /*!
   * \brief define if a node is new or old
   * \param node - node to check
   * \retval bool - true if the node existed before Compute() is called
   */
  //================================================================================

  bool isOldNode( const SMDS_MeshNode* node )
  {
    // old nodes are shared by edges and new ones are shared
    // only by faces created by mapper
    //if ( is1DComputed )
    {
      bool isOld = node->NbInverseElements(SMDSAbs_Edge) > 0;
      return isOld;
    }
    // else
    // {
    //   SMDS_ElemIteratorPtr invFace = node->GetInverseElementIterator(SMDSAbs_Face);
    //   bool isNew = invFace->more();
    //   return !isNew;
    // }
  }

  //================================================================================
  /*!
   * \brief Class to remove mesh built by pattern mapper on edges
   * and vertices in the case of failure of projection algo.
   * It does it's job at destruction
   */
  //================================================================================

  class MeshCleaner {
    SMESH_subMesh* sm;
  public:
    MeshCleaner( SMESH_subMesh* faceSubMesh ): sm(faceSubMesh) {}
    ~MeshCleaner() { Clean(sm); }
    void Release() { sm = 0; } // mesh will not be removed
    static void Clean( SMESH_subMesh* sm, bool withSub=true )
    {
      if ( !sm || !sm->GetSubMeshDS() ) return;
      // PAL16567, 18920. Remove face nodes as well
//       switch ( sm->GetSubShape().ShapeType() ) {
//       case TopAbs_VERTEX:
//       case TopAbs_EDGE: {
        SMDS_NodeIteratorPtr nIt = sm->GetSubMeshDS()->GetNodes();
        SMESHDS_Mesh* mesh = sm->GetFather()->GetMeshDS();
        while ( nIt->more() ) {
          const SMDS_MeshNode* node = nIt->next();
          if ( !isOldNode( node ) )
            mesh->RemoveNode( node );
        }
        // do not break but iterate over DependsOn()
//       }
//       default:
        if ( !withSub ) return;
        SMESH_subMeshIteratorPtr smIt = sm->getDependsOnIterator(false,false);
        while ( smIt->more() )
          Clean( smIt->next(), false );
//       }
    }
  };

  //================================================================================
  /*!
   * \brief find new nodes belonging to one free border of mesh on face
    * \param sm - submesh on edge or vertex containing nodes to choose from
    * \param face - the face bound by the submesh
    * \param u2nodes - map to fill with nodes
    * \param seamNodes - set of found nodes
    * \retval bool - is a success
   */
  //================================================================================

  bool getBoundaryNodes ( SMESH_subMesh*                        sm,
                          const TopoDS_Face&                    /*face*/,
                          map< double, const SMDS_MeshNode* > & u2nodes,
                          set< const SMDS_MeshNode* > &         seamNodes)
  {
    u2nodes.clear();
    seamNodes.clear();
    if ( !sm || !sm->GetSubMeshDS() )
      RETURN_BAD_RESULT("Null submesh");

    SMDS_NodeIteratorPtr nIt = sm->GetSubMeshDS()->GetNodes();
    switch ( sm->GetSubShape().ShapeType() ) {

    case TopAbs_VERTEX: {
      while ( nIt->more() ) {
        const SMDS_MeshNode* node = nIt->next();
        if ( isOldNode( node ) ) continue;
        u2nodes.insert( make_pair( 0., node ));
        seamNodes.insert( node );
        return true;
      }
      break;
    }
    case TopAbs_EDGE: {

      // Get submeshes of sub-vertices
      const map< int, SMESH_subMesh * >& subSM = sm->DependsOn();
      if ( subSM.size() != 2 )
        RETURN_BAD_RESULT("there must be 2 submeshes of sub-vertices"
                          " but we have " << subSM.size());
      SMESH_subMesh* smV1 = subSM.begin()->second;
      SMESH_subMesh* smV2 = subSM.rbegin()->second;
      if ( !smV1->IsMeshComputed() || !smV2->IsMeshComputed() )
        RETURN_BAD_RESULT("Empty vertex submeshes");

      const SMDS_MeshNode* nV1 = 0;
      const SMDS_MeshNode* nE = 0;

      // Look for nV1 - a new node on V1
      nIt = smV1->GetSubMeshDS()->GetNodes();
      while ( nIt->more() && !nE ) {
        const SMDS_MeshNode* node = nIt->next();
        if ( isOldNode( node ) ) continue;
        nV1 = node;

        // Find nE - a new node connected to nV1 and belonging to edge submesh;
        SMESHDS_SubMesh* smDS = sm->GetSubMeshDS();
        SMDS_ElemIteratorPtr vElems = nV1->GetInverseElementIterator(SMDSAbs_Face);
        while ( vElems->more() && !nE ) {
          const SMDS_MeshElement* elem = vElems->next();
          int nbNodes = elem->NbNodes();
          if ( elem->IsQuadratic() )
            nbNodes /= 2;
          int iV1 = elem->GetNodeIndex( nV1 );
          // try next after nV1
          int iE = SMESH_MesherHelper::WrapIndex( iV1 + 1, nbNodes );
          if ( smDS->Contains( elem->GetNode( iE ) ))
            nE = elem->GetNode( iE );
          if ( !nE ) {
            // try node before nV1
            iE = SMESH_MesherHelper::WrapIndex( iV1 - 1, nbNodes );
            if ( smDS->Contains( elem->GetNode( iE )))
              nE = elem->GetNode( iE );
          }
          if ( nE && elem->IsQuadratic() ) { // find medium node between nV1 and nE
            if ( Abs( iV1 - iE ) == 1 )
              nE = elem->GetNode( Min ( iV1, iE ) + nbNodes );
            else
              nE = elem->GetNode( elem->NbNodes() - 1 );
          }
        }
      }
      if ( !nV1 )
        RETURN_BAD_RESULT("No new node found on V1");
      if ( !nE )
        RETURN_BAD_RESULT("new node on edge not found");

      // Get the whole free border of a face
      list< const SMDS_MeshNode* > bordNodes;
      list< const SMDS_MeshElement* > bordFaces;
      if ( !SMESH_MeshEditor::FindFreeBorder (nV1, nE, nV1, bordNodes, bordFaces ))
        RETURN_BAD_RESULT("free border of a face not found by nodes " <<
                          nV1->GetID() << " " << nE->GetID() );

      // Insert nodes of the free border to the map until node on V2 encountered
      SMESHDS_SubMesh* v2smDS = smV2->GetSubMeshDS();
      list< const SMDS_MeshNode* >::iterator bordIt = bordNodes.begin();
      bordIt++; // skip nV1
      for ( ; bordIt != bordNodes.end(); ++bordIt ) {
        const SMDS_MeshNode* node = *bordIt;
        if ( v2smDS->Contains( node ))
          break;
        if ( node->GetPosition()->GetTypeOfPosition() != SMDS_TOP_EDGE )
          RETURN_BAD_RESULT("Bad node position type: node " << node->GetID() <<
                            " pos type " << node->GetPosition()->GetTypeOfPosition());
        SMDS_EdgePositionPtr pos = node->GetPosition();
        u2nodes.insert( make_pair( pos->GetUParameter(), node ));
        seamNodes.insert( node );
      }
      if ( u2nodes.size() != seamNodes.size() )
        RETURN_BAD_RESULT("Bad node params on edge " << sm->GetId() <<
                          ", " << u2nodes.size() << " != " << seamNodes.size() );
      return true;
    }
    default:;
    }
    RETURN_BAD_RESULT ("Unexpected submesh type");

  } // bool getBoundaryNodes()

  //================================================================================
  /*!
   * \brief Check if two consecutive EDGEs are connected in 2D
   *  \param [in] E1 - a well oriented non-seam EDGE
   *  \param [in] E2 - a possibly well oriented seam EDGE
   *  \param [in] F - a FACE
   *  \return bool - result
   */
  //================================================================================

  bool are2dConnected( const TopoDS_Edge & E1,
                       const TopoDS_Edge & E2,
                       const TopoDS_Face & F )
  {
    double f,l;
    Handle(Geom2d_Curve) c1 = BRep_Tool::CurveOnSurface( E1, F, f, l );
    gp_Pnt2d uvFirst1 = c1->Value( f );
    gp_Pnt2d uvLast1  = c1->Value( l );

    Handle(Geom2d_Curve) c2 = BRep_Tool::CurveOnSurface( E2, F, f, l );
    gp_Pnt2d uvFirst2 = c2->Value( E2.Orientation() == TopAbs_REVERSED ? l : f );
    double tol2 = Max( Precision::PConfusion() * Precision::PConfusion(),
                       1e-5 * uvLast1.SquareDistance( uvFirst1 ));

    return (( uvFirst2.SquareDistance( uvFirst1 ) < tol2 ) ||
            ( uvFirst2.SquareDistance( uvLast1  ) < tol2 ));
  }

  //================================================================================
  /*!
   * \brief Compose TSideVector for both FACEs keeping matching order of EDGEs
   *        and fill src2tgtNodes map
   */
  //================================================================================

  TError getWires(const TopoDS_Face&                 tgtFace,
                  const TopoDS_Face&                 srcFace,
                  SMESH_Mesh *                       tgtMesh,
                  SMESH_Mesh *                       srcMesh,
                  SMESH_MesherHelper*                tgtHelper,
                  const TAssocTool::TShapeShapeMap&  shape2ShapeMap,
                  TSideVector&                       srcWires,
                  TSideVector&                       tgtWires,
                  TAssocTool::TNodeNodeMap&          src2tgtNodes,
                  bool&                              is1DComputed)
  {
    src2tgtNodes.clear();

    // get ordered src EDGEs
    TError err;
    srcWires = StdMeshers_FaceSide::GetFaceWires( srcFace, *srcMesh,/*skipMediumNodes=*/0, err );
    if (( err && !err->IsOK() ) ||
        ( srcWires.empty() ))
      return err;
#ifdef PRINT_WHO_COMPUTE_WHAT
    cout << "Projection_2D" <<  " F "
         << tgtMesh->GetMeshDS()->ShapeToIndex( tgtFace ) << " <- "
         << srcMesh->GetMeshDS()->ShapeToIndex( srcFace ) << endl;
#endif

    // make corresponding sequence of tgt EDGEs
    tgtWires.resize( srcWires.size() );
    for ( size_t iW = 0; iW < srcWires.size(); ++iW )
    {
      StdMeshers_FaceSidePtr srcWire = srcWires[iW];

      list< TopoDS_Edge > tgtEdges;
      TopTools_IndexedMapOfShape edgeMap; // to detect seam edges
      for ( int iE = 0; iE < srcWire->NbEdges(); ++iE )
      {
        TopoDS_Edge     srcE = srcWire->Edge( iE );
        TopoDS_Edge     tgtE = TopoDS::Edge( shape2ShapeMap( srcE, /*isSrc=*/true));
        TopoDS_Shape srcEbis = shape2ShapeMap( tgtE, /*isSrc=*/false );
        if ( srcE.Orientation() != srcEbis.Orientation() )
          tgtE.Reverse();
        // reverse a seam edge encountered for the second time
        const int index = edgeMap.Add( tgtE );
        if ( index < edgeMap.Extent() ) // E is a seam
        {
          // check which of edges to reverse, E or one already being in tgtEdges
          if ( are2dConnected( tgtEdges.back(), tgtE, tgtFace ))
          {
            list< TopoDS_Edge >::iterator eIt = tgtEdges.begin();
            std::advance( eIt, index-1 );
            if ( are2dConnected( tgtEdges.back(), *eIt, tgtFace ))
              eIt->Reverse();
          }
          else
          {
            tgtE.Reverse();
          }
        }
        if ( srcWire->NbEdges() == 1 && tgtMesh == srcMesh ) // circle
        {
          // try to verify ori by propagation
          pair<int,TopoDS_Edge> nE =
            StdMeshers_ProjectionUtils::GetPropagationEdge( srcMesh, tgtE, srcE );
          if ( !nE.second.IsNull() )
            tgtE = nE.second;
        }
        tgtEdges.push_back( tgtE );
      }

      tgtWires[ iW ].reset( new StdMeshers_FaceSide( tgtFace, tgtEdges, tgtMesh,
                                                     /*theIsForward = */ true,
                                                     /*theIgnoreMediumNodes = */false,
                                                     tgtHelper ));
      StdMeshers_FaceSidePtr tgtWire = tgtWires[ iW ];

      // Fill map of src to tgt nodes with nodes on edges

      for ( int iE = 0; iE < srcWire->NbEdges(); ++iE )
      {
#ifdef PRINT_WHO_COMPUTE_WHAT
        if ( tgtMesh->GetSubMesh( tgtWire->Edge(iE) )->IsEmpty() )
          cout << "Projection_2D" <<  " E "
               << tgtWire->EdgeID(iE) << " <- " << srcWire->EdgeID(iE) << endl;
#endif
        if ( srcMesh->GetSubMesh( srcWire->Edge(iE) )->IsEmpty() ||
             tgtMesh->GetSubMesh( tgtWire->Edge(iE) )->IsEmpty() )
        {
          // add nodes on VERTEXes for a case of not meshes EDGEs
          const SMDS_MeshNode* srcN = srcWire->VertexNode( iE );
          const SMDS_MeshNode* tgtN = tgtWire->VertexNode( iE );
          if ( srcN && tgtN )
            src2tgtNodes.insert( make_pair( srcN, tgtN ));
        }
        else
        {
          const bool skipMedium = true, isFwd = true;
          StdMeshers_FaceSide srcEdge( srcFace, srcWire->Edge(iE),
                                       srcMesh, isFwd, skipMedium, srcWires[0]->FaceHelper() );
          StdMeshers_FaceSide tgtEdge( tgtFace, tgtWire->Edge(iE),
                                       tgtMesh, isFwd, skipMedium, tgtHelper);

          vector< const SMDS_MeshNode* > srcNodes = srcEdge.GetOrderedNodes();
          vector< const SMDS_MeshNode* > tgtNodes = tgtEdge.GetOrderedNodes();

          if (( srcNodes.size() != tgtNodes.size() ) && tgtNodes.size() > 0 )
            return SMESH_ComputeError::New( COMPERR_BAD_INPUT_MESH,
                                            "Different number of nodes on edges");
          if ( !tgtNodes.empty() )
          {
            vector< const SMDS_MeshNode* >::iterator tn = tgtNodes.begin();
            //if ( srcWire->Edge(iE).Orientation() == tgtWire->Edge(iE).Orientation() )
            {
              vector< const SMDS_MeshNode* >::iterator sn = srcNodes.begin();
              for ( ; tn != tgtNodes.end(); ++tn, ++sn)
                src2tgtNodes.insert( make_pair( *sn, *tn ));
            }
            // else
            // {
            //   vector< const SMDS_MeshNode* >::reverse_iterator sn = srcNodes.rbegin();
            //   for ( ; tn != tgtNodes.end(); ++tn, ++sn)
            //     src2tgtNodes.insert( make_pair( *sn, *tn ));
            // }
            is1DComputed = true;
          }
        }
      } // loop on EDGEs of a WIRE

    } // loop on WIREs

    return TError();
  }

  //================================================================================
  /*!
   * \brief Perform projection in case if tgtFace.IsPartner( srcFace ) and in case
   * if projection by 3D transformation is possible
   */
  //================================================================================

  bool projectPartner(const TopoDS_Face&                 tgtFace,
                      const TopoDS_Face&                 srcFace,
                      const TSideVector&                 tgtWires,
                      const TSideVector&                 srcWires,
                      const TAssocTool::TShapeShapeMap&  shape2ShapeMap,
                      TAssocTool::TNodeNodeMap&          src2tgtNodes,
                      const bool                         is1DComputed)
  {
    SMESH_Mesh *       tgtMesh = tgtWires[0]->GetMesh();
    SMESH_Mesh *       srcMesh = srcWires[0]->GetMesh();
    SMESHDS_Mesh*    tgtMeshDS = tgtMesh->GetMeshDS();
    SMESHDS_Mesh*    srcMeshDS = srcMesh->GetMeshDS();
    SMESH_MesherHelper* helper = tgtWires[0]->FaceHelper();

    const double tol = 1.e-7 * srcMeshDS->getMaxDim();

    // transformation to get location of target nodes from source ones
    StdMeshers_ProjectionUtils::TrsfFinder3D trsf;
    bool trsfIsOK = false;
    if ( tgtFace.IsPartner( srcFace ))
    {
      gp_GTrsf srcTrsf = srcFace.Location().Transformation();
      gp_GTrsf tgtTrsf = tgtFace.Location().Transformation();
      gp_GTrsf t = srcTrsf.Inverted().Multiplied( tgtTrsf );
      trsf.Set( t );
      // check
      gp_Pnt srcP = BRep_Tool::Pnt( srcWires[0]->FirstVertex() );
      gp_Pnt tgtP = BRep_Tool::Pnt( tgtWires[0]->FirstVertex() );
      trsfIsOK = ( tgtP.Distance( trsf.Transform( srcP )) < tol );
      if ( !trsfIsOK )
      {
        trsf.Set( tgtTrsf.Inverted().Multiplied( srcTrsf ));
        trsfIsOK = ( tgtP.Distance( trsf.Transform( srcP )) < tol );
      }
    }
    if ( !trsfIsOK )
    {
      // Try to find the 3D transformation

      const int totNbSeg = 50;
      vector< gp_XYZ > srcPnts, tgtPnts;
      srcPnts.reserve( totNbSeg );
      tgtPnts.reserve( totNbSeg );
      gp_XYZ srcBC( 0,0,0 ), tgtBC( 0,0,0 );
      for ( size_t iW = 0; iW < srcWires.size(); ++iW )
      {
        const double minSegLen = srcWires[iW]->Length() / totNbSeg;
        for ( int iE = 0; iE < srcWires[iW]->NbEdges(); ++iE )
        {
          size_t nbSeg = Max( 1, int( srcWires[iW]->EdgeLength( iE ) / minSegLen ));
          double srcU  = srcWires[iW]->FirstParameter( iE );
          double tgtU  = tgtWires[iW]->FirstParameter( iE );
          double srcDu = ( srcWires[iW]->LastParameter( iE )- srcU ) / nbSeg;
          double tgtDu = ( tgtWires[iW]->LastParameter( iE )- tgtU ) / nbSeg;
          for ( size_t i = 0; i < nbSeg; ++i  )
          {
            srcPnts.push_back( srcWires[iW]->Value3d( srcU ).XYZ() );
            tgtPnts.push_back( tgtWires[iW]->Value3d( tgtU ).XYZ() );
            srcU += srcDu;
            tgtU += tgtDu;
            srcBC += srcPnts.back();
            tgtBC += tgtPnts.back();
          }
        }
      }
      if ( !trsf.Solve( srcPnts, tgtPnts ))
        return false;

      // check trsf

      const int nbTestPnt = 20;
      const size_t  iStep = Max( 1, int( srcPnts.size() / nbTestPnt ));
      // check boundary
      gp_Pnt trsfTgt = trsf.Transform( srcBC / srcPnts.size() );
      trsfIsOK = ( trsfTgt.SquareDistance( tgtBC / tgtPnts.size() ) < tol*tol );
      for ( size_t i = 0; ( i < srcPnts.size() && trsfIsOK ); i += iStep )
      {
        gp_Pnt trsfTgt = trsf.Transform( srcPnts[i] );
        trsfIsOK = ( trsfTgt.SquareDistance( tgtPnts[i] ) < tol*tol );
      }
      // check an in-FACE point
      if ( trsfIsOK )
      {
        BRepAdaptor_Surface srcSurf( srcFace );
        gp_Pnt srcP =
          srcSurf.Value( 0.321 * ( srcSurf.FirstUParameter() + srcSurf.LastUParameter() ),
                         0.123 * ( srcSurf.FirstVParameter() + srcSurf.LastVParameter() ));
        gp_Pnt tgtTrsfP = trsf.Transform( srcP );
        TopLoc_Location loc;
        GeomAPI_ProjectPointOnSurf& proj = helper->GetProjector( tgtFace, loc, 0.1*tol );
        if ( !loc.IsIdentity() )
          tgtTrsfP.Transform( loc.Transformation().Inverted() );
        proj.Perform( tgtTrsfP );
        trsfIsOK = ( proj.IsDone() &&
                     proj.NbPoints() > 0 &&
                     proj.LowerDistance() < tol );
      }
      if ( !trsfIsOK )
        return false;
    }

    // Make new faces

    // prepare the helper to adding quadratic elements if necessary
    helper->IsQuadraticSubMesh( tgtFace );

    SMESHDS_SubMesh* srcSubDS = srcMeshDS->MeshElements( srcFace );
    if ( !is1DComputed && srcSubDS->NbElements() )
      helper->SetIsQuadratic( srcSubDS->GetElements()->next()->IsQuadratic() );

    SMESH_MesherHelper* srcHelper = srcWires[0]->FaceHelper();
    SMESH_MesherHelper  edgeHelper( *tgtMesh );
    edgeHelper.ToFixNodeParameters( true );

    const SMDS_MeshNode* nullNode = 0;
    TAssocTool::TNodeNodeMap::iterator srcN_tgtN;

    // indices of nodes to create properly oriented faces
    bool isReverse = ( !trsf.IsIdentity() );
    int tri1 = 1, tri2 = 2, quad1 = 1, quad3 = 3;
    if ( isReverse )
      std::swap( tri1, tri2 ), std::swap( quad1, quad3 );

    SMDS_ElemIteratorPtr elemIt = srcSubDS->GetElements();
    vector< const SMDS_MeshNode* > tgtNodes;
    while ( elemIt->more() ) // loop on all mesh faces on srcFace
    {
      const SMDS_MeshElement* elem = elemIt->next();
      const int nbN = elem->NbCornerNodes();
      tgtNodes.resize( nbN );
      helper->SetElementsOnShape( false );
      for ( int i = 0; i < nbN; ++i ) // loop on nodes of the source element
      {
        const SMDS_MeshNode* srcNode = elem->GetNode(i);
        srcN_tgtN = src2tgtNodes.insert( make_pair( srcNode, nullNode )).first;
        if ( srcN_tgtN->second == nullNode )
        {
          // create a new node
          gp_Pnt tgtP = trsf.Transform( SMESH_TNodeXYZ( srcNode ));
          SMDS_MeshNode* n = helper->AddNode( tgtP.X(), tgtP.Y(), tgtP.Z() );
          srcN_tgtN->second = n;
          switch ( srcNode->GetPosition()->GetTypeOfPosition() )
          {
          case SMDS_TOP_FACE:
          {
            gp_Pnt2d srcUV = srcHelper->GetNodeUV( srcFace, srcNode );
            tgtMeshDS->SetNodeOnFace( n, helper->GetSubShapeID(), srcUV.X(), srcUV.Y() );
            break;
          }
          case SMDS_TOP_EDGE:
          {
            const TopoDS_Edge& srcE = TopoDS::Edge( srcMeshDS->IndexToShape( srcNode->getshapeId()));
            const TopoDS_Edge& tgtE = TopoDS::Edge( shape2ShapeMap( srcE, /*isSrc=*/true ));
            double srcU = srcHelper->GetNodeU( srcE, srcNode );
            tgtMeshDS->SetNodeOnEdge( n, tgtE, srcU );
            if ( !tgtFace.IsPartner( srcFace ))
            {
              edgeHelper.SetSubShape( tgtE );
              double tol = BRep_Tool::Tolerance( tgtE );
              bool isOk = edgeHelper.CheckNodeU( tgtE, n, srcU, 2 * tol, /*force=*/true );
              if ( !isOk ) // projection of n to tgtE failed (23395)
              {
                double sF, sL, tF, tL;
                BRep_Tool::Range( srcE, sF, sL );
                BRep_Tool::Range( tgtE, tF, tL );
                double srcR = ( srcU - sF ) / ( sL - sF );
                double tgtU  = tF + srcR * ( tL - tF );
                tgtMeshDS->SetNodeOnEdge( n, tgtE, tgtU );
                gp_Pnt newP = BRepAdaptor_Curve( tgtE ).Value( tgtU );
                double dist = newP.Distance( tgtP );
                if ( tol < dist && dist < 1000*tol )
                  tgtMeshDS->MoveNode( n, newP.X(), newP.Y(), newP.Z() );
              }
            }
            break;
          }
          case SMDS_TOP_VERTEX:
          {
            const TopoDS_Shape & srcV = srcMeshDS->IndexToShape( srcNode->getshapeId() );
            const TopoDS_Shape & tgtV = shape2ShapeMap( srcV, /*isSrc=*/true );
            tgtMeshDS->SetNodeOnVertex( n, TopoDS::Vertex( tgtV ));
            break;
          }
          default:;
          }
        }
        tgtNodes[i] = srcN_tgtN->second;
      }
      // create a new face
      helper->SetElementsOnShape( true );
      switch ( nbN )
      {
      case 3: helper->AddFace(tgtNodes[0], tgtNodes[tri1], tgtNodes[tri2]); break;
      case 4: helper->AddFace(tgtNodes[0], tgtNodes[quad1], tgtNodes[2], tgtNodes[quad3]); break;
      default:
        if ( isReverse ) std::reverse( tgtNodes.begin(), tgtNodes.end() );
        helper->AddPolygonalFace( tgtNodes );
      }
    }

    // check node positions

    // if ( !tgtFace.IsPartner( srcFace ) ) for NETGEN 6 which sets wrong UV
    {
      helper->ToFixNodeParameters( true );

      int nbOkPos = 0;
      const double tol2d = 1e-12;
      srcN_tgtN = src2tgtNodes.begin();
      for ( ; srcN_tgtN != src2tgtNodes.end(); ++srcN_tgtN )
      {
        const SMDS_MeshNode* n = srcN_tgtN->second;
        switch ( n->GetPosition()->GetTypeOfPosition() )
        {
        case SMDS_TOP_FACE:
        {
          if ( nbOkPos > 10 ) break;
          gp_XY uv = helper->GetNodeUV( tgtFace, n ), uvBis = uv;
          if (( helper->CheckNodeUV( tgtFace, n, uv, tol )) &&
              (( uv - uvBis ).SquareModulus() < tol2d ))
            ++nbOkPos;
          else
            nbOkPos = -((int) src2tgtNodes.size() );
          break;
        }
        case SMDS_TOP_EDGE:
        {
          // const TopoDS_Edge & tgtE = TopoDS::Edge( tgtMeshDS->IndexToShape( n->getshapeId() ));
          // edgeHelper.SetSubShape( tgtE );
          // edgeHelper.GetNodeU( tgtE, n, 0, &toCheck );
          break;
        }
        default:;
        }
      }
    }

    return true;

  } //   bool projectPartner()

  //================================================================================
  /*!
   * \brief Perform projection in case if the faces are similar in 2D space
   */
  //================================================================================

  bool projectBy2DSimilarity(const TopoDS_Face&                 tgtFace,
                             const TopoDS_Face&                 srcFace,
                             const TSideVector&                 tgtWires,
                             const TSideVector&                 srcWires,
                             const TAssocTool::TShapeShapeMap&  shape2ShapeMap,
                             TAssocTool::TNodeNodeMap&          src2tgtNodes,
                             const bool                         is1DComputed)
  {
    SMESH_Mesh * tgtMesh = tgtWires[0]->GetMesh();
    SMESH_Mesh * srcMesh = srcWires[0]->GetMesh();

    // WARNING: we can have problems if the FACE is symmetrical in 2D,
    // then the projection can be mirrored relating to what is expected

    // 1) Find 2D transformation

    StdMeshers_ProjectionUtils::TrsfFinder2D trsf;
    {
      // get 2 pairs of corresponding UVs
      gp_Pnt2d srcP0 = srcWires[0]->Value2d(0.0);
      gp_Pnt2d srcP1 = srcWires[0]->Value2d(0.333);
      gp_Pnt2d tgtP0 = tgtWires[0]->Value2d(0.0);
      gp_Pnt2d tgtP1 = tgtWires[0]->Value2d(0.333);

      // make transformation
      gp_Trsf2d fromTgtCS, toSrcCS; // from/to global CS
      gp_Ax2d srcCS( srcP0, gp_Vec2d( srcP0, srcP1 ));
      gp_Ax2d tgtCS( tgtP0, gp_Vec2d( tgtP0, tgtP1 ));
      toSrcCS  .SetTransformation( srcCS );
      fromTgtCS.SetTransformation( tgtCS );
      fromTgtCS.Invert();
      trsf.Set( fromTgtCS * toSrcCS );

      // check transformation
      bool trsfIsOK = true;
      const double tol = 1e-5 * gp_Vec2d( srcP0, srcP1 ).Magnitude();
      for ( double u = 0.12; ( u < 1. && trsfIsOK ); u += 0.1 )
      {
        gp_Pnt2d srcUV  = srcWires[0]->Value2d( u );
        gp_Pnt2d tgtUV  = tgtWires[0]->Value2d( u );
        gp_Pnt2d tgtUV2 = trsf.Transform( srcUV );
        trsfIsOK = ( tgtUV.Distance( tgtUV2 ) < tol );
      }

      // Find trsf using a least-square approximation
      if ( !trsfIsOK )
      {
        // find trsf
        const int totNbSeg = 50;
        vector< gp_XY > srcPnts, tgtPnts;
        srcPnts.reserve( totNbSeg );
        tgtPnts.reserve( totNbSeg );
        for ( size_t iW = 0; iW < srcWires.size(); ++iW )
        {
          const double minSegLen = srcWires[iW]->Length() / totNbSeg;
          for ( int iE = 0; iE < srcWires[iW]->NbEdges(); ++iE )
          {
            size_t nbSeg = Max( 1, int( srcWires[iW]->EdgeLength( iE ) / minSegLen ));
            double srcU  = srcWires[iW]->FirstParameter( iE );
            double tgtU  = tgtWires[iW]->FirstParameter( iE );
            double srcDu = ( srcWires[iW]->LastParameter( iE )- srcU ) / nbSeg;
            double tgtDu = ( tgtWires[iW]->LastParameter( iE )- tgtU ) / nbSeg;
            for ( size_t i = 0; i < nbSeg; ++i, srcU += srcDu, tgtU += tgtDu  )
            {
              srcPnts.push_back( srcWires[iW]->Value2d( srcU ).XY() );
              tgtPnts.push_back( tgtWires[iW]->Value2d( tgtU ).XY() );
            }
          }
        }
        if ( !trsf.Solve( srcPnts, tgtPnts ))
          return false;

        // check trsf

        trsfIsOK = true;
        const int nbTestPnt = 10;
        const size_t  iStep = Max( 1, int( srcPnts.size() / nbTestPnt ));
        for ( size_t i = 0; ( i < srcPnts.size() && trsfIsOK ); i += iStep )
        {
          gp_Pnt2d trsfTgt = trsf.Transform( srcPnts[i] );
          trsfIsOK = ( trsfTgt.Distance( tgtPnts[i] ) < tol );
        }
        if ( !trsfIsOK )
          return false;
      }
    } // "Find transformation" block

    // 2) Projection

    SMESHDS_SubMesh* srcSubDS = srcMesh->GetMeshDS()->MeshElements( srcFace );

    SMESH_MesherHelper* helper = tgtWires[0]->FaceHelper();
    if ( is1DComputed )
      helper->IsQuadraticSubMesh( tgtFace );
    else
      helper->SetIsQuadratic( srcSubDS->GetElements()->next()->IsQuadratic() );
    helper->SetElementsOnShape( true );
    Handle(Geom_Surface) tgtSurface = BRep_Tool::Surface( tgtFace );
    SMESHDS_Mesh* tgtMeshDS = tgtMesh->GetMeshDS();

    SMESH_MesherHelper* srcHelper = srcWires[0]->FaceHelper();

    const SMDS_MeshNode* nullNode = 0;
    TAssocTool::TNodeNodeMap::iterator srcN_tgtN;

    SMDS_ElemIteratorPtr elemIt = srcSubDS->GetElements();
    vector< const SMDS_MeshNode* > tgtNodes;
    bool uvOK;
    while ( elemIt->more() ) // loop on all mesh faces on srcFace
    {
      const SMDS_MeshElement* elem = elemIt->next();
      const int nbN = elem->NbCornerNodes();
      tgtNodes.resize( nbN );
      for ( int i = 0; i < nbN; ++i ) // loop on nodes of the source element
      {
        const SMDS_MeshNode* srcNode = elem->GetNode(i);
        srcN_tgtN = src2tgtNodes.insert( make_pair( srcNode, nullNode )).first;
        if ( srcN_tgtN->second == nullNode )
        {
          // create a new node
          gp_Pnt2d srcUV = srcHelper->GetNodeUV( srcFace, srcNode,
                                                 elem->GetNode( helper->WrapIndex(i+1,nbN)), &uvOK);
          gp_Pnt2d   tgtUV = trsf.Transform( srcUV );
          gp_Pnt      tgtP = tgtSurface->Value( tgtUV.X(), tgtUV.Y() );
          SMDS_MeshNode* n = tgtMeshDS->AddNode( tgtP.X(), tgtP.Y(), tgtP.Z() );
          switch ( srcNode->GetPosition()->GetTypeOfPosition() )
          {
          case SMDS_TOP_FACE: {
            tgtMeshDS->SetNodeOnFace( n, helper->GetSubShapeID(), tgtUV.X(), tgtUV.Y() );
            break;
          }
          case SMDS_TOP_EDGE: {
            TopoDS_Shape srcEdge = srcHelper->GetSubShapeByNode( srcNode, srcHelper->GetMeshDS() );
            TopoDS_Edge  tgtEdge = TopoDS::Edge( shape2ShapeMap( srcEdge, /*isSrc=*/true ));
            double U = Precision::Infinite();
            helper->CheckNodeU( tgtEdge, n, U, Precision::PConfusion());
            tgtMeshDS->SetNodeOnEdge( n, TopoDS::Edge( tgtEdge ), U );
            break;
          }
          case SMDS_TOP_VERTEX: {
            TopoDS_Shape srcV = srcHelper->GetSubShapeByNode( srcNode, srcHelper->GetMeshDS() );
            TopoDS_Shape tgtV = shape2ShapeMap( srcV, /*isSrc=*/true );
            tgtMeshDS->SetNodeOnVertex( n, TopoDS::Vertex( tgtV ));
            break;
          }
          default:;
          }
          srcN_tgtN->second = n;
        }
        tgtNodes[i] = srcN_tgtN->second;
      }
      // create a new face (with reversed orientation)
      switch ( nbN )
      {
      case 3: helper->AddFace(tgtNodes[0], tgtNodes[2], tgtNodes[1]); break;
      case 4: helper->AddFace(tgtNodes[0], tgtNodes[3], tgtNodes[2], tgtNodes[1]); break;
      }
    }  // loop on all mesh faces on srcFace

    return true;

  } // projectBy2DSimilarity()

  //================================================================================
  /*!
   * \brief Quadrangle algorithm computing structured triangle mesh
   */
  //================================================================================

  struct QuadAlgo : public StdMeshers_Quadrangle_2D
  {
    QuadAlgo( int hypId, SMESH_Gen* gen ): StdMeshers_Quadrangle_2D( hypId, gen ) {}

    bool Compute( SMESH_MesherHelper & theHelper, const StdMeshers_FaceSidePtr& theWire )
    {
      SMESH_Mesh& theMesh = *theHelper.GetMesh();

      // set sides of a quad FACE
      FaceQuadStruct::Ptr quad( new FaceQuadStruct );
      quad->side.reserve( 4 );
      quad->face = theWire->Face();
      for ( int i = 0; i < 4; ++i )
        quad->side.push_back
          ( StdMeshers_FaceSide::New( quad->face, theWire->Edge(i), &theMesh, i < QUAD_TOP_SIDE,
                                      /*skipMedium=*/true, theWire->FaceHelper() ));
      if ( !setNormalizedGrid( quad ))
        return false;

      // make internal nodes
      SMESHDS_Mesh *  meshDS = theMesh.GetMeshDS();
      int         geomFaceID = meshDS->ShapeToIndex( quad->face );
      Handle(Geom_Surface) S = BRep_Tool::Surface( quad->face );
      for (   int i = 1; i < quad->iSize - 1; i++)
        for ( int j = 1; j < quad->jSize - 1; j++)
        {
          UVPtStruct& uvPnt = quad->UVPt( i, j );
          gp_Pnt P          = S->Value( uvPnt.u, uvPnt.v );
          uvPnt.node        = meshDS->AddNode(P.X(), P.Y(), P.Z());
          meshDS->SetNodeOnFace( uvPnt.node, geomFaceID, uvPnt.u, uvPnt.v );
        }

      // make triangles
      for (   int i = 0; i < quad->iSize-1; i++) {
        for ( int j = 0; j < quad->jSize-1; j++)
        {
          const SMDS_MeshNode* a = quad->uv_grid[ j      * quad->iSize + i    ].node;
          const SMDS_MeshNode* b = quad->uv_grid[ j      * quad->iSize + i + 1].node;
          const SMDS_MeshNode* c = quad->uv_grid[(j + 1) * quad->iSize + i + 1].node;
          const SMDS_MeshNode* d = quad->uv_grid[(j + 1) * quad->iSize + i    ].node;
          theHelper.AddFace(a, b, c);
          theHelper.AddFace(a, c, d);
        }
      }
      return true;
    }
  };

  //================================================================================
  /*!
   * \brief Local coordinate system of a triangle. Return barycentric coordinates of a point
   */
  //================================================================================

  struct TriaCoordSys
  {
    gp_Pnt myO;   //!< origin
    gp_Vec myX;   //!< X axis
    gp_Vec myY;   //!< Y axis
    gp_XY  myUV1; //!< UV of 2nd node in this CS
    gp_XY  myUV2; //!< UV of 3d  node in this CS

    void Init( const gp_Pnt p1, const gp_Pnt p2, const gp_Pnt p3 )
    {
      myO = p1;

      myX = gp_Vec( p1, p2 );
      myUV1.SetCoord( myX.Magnitude(), 0 );
      myX /= myUV1.X();

      gp_Vec v13( p1, p3 );
      myY = myX.CrossCrossed( myX, v13 );
      myY.Normalize();
      myUV2.SetCoord( v13 * myX, v13 * myY );

      return;
    }

    void GetBaryCoords( const gp_Pnt p, double& bc1, double& bc2, double& bc3 ) const
    {
      gp_Vec op( myO, p );
      gp_XY uv( op * myX, op * myY );

      SMESH_MeshAlgos::GetBarycentricCoords( uv,
                                             gp::Origin2d().XY(), myUV1, myUV2,
                                             bc1, bc2 );
      bc3 = 1 - bc1 - bc2;
    }
  };


  //================================================================================
  /*!
   * \brief Structured 2D mesh of a quadrilateral FACE; is used in projectQuads()
   */
  //================================================================================

  struct QuadMesh : public SMESH_SequentialMesh
  {
    ObjectPool< TriaCoordSys > _traiLCSPool;
    SMESH_ElementSearcher*     _elemSearcher;
    SMESH_Gen                  _sgen;
    SMESH_MesherHelper         _helper;

    QuadMesh(const TopoDS_Face& face):
      _elemSearcher( nullptr ), _helper( *this )
    {
      _meshDS = new SMESHDS_Mesh( 0, true );
      _gen = &_sgen;
      ShapeToMesh( face );
    }
    ~QuadMesh() { delete _elemSearcher; }

    // --------------------------------------------------------------------------------
    /*!
     * \brief Compute quadrangle mesh and prepare for face search
     */
    bool Compute( const TSideVector& wires, int nbSeg1, int nbSeg2, bool isSourceMesh )
    {
      if ( wires.size() > 1 || wires[0]->NbEdges() != 4 )
        return false;

      // compute quadrangle mesh

      SMESH_Hypothesis* algo1D = new StdMeshers_Regular_1D( _sgen.GetANewId(), &_sgen );
      AddHypothesis( GetShapeToMesh(), algo1D->GetID() );

      StdMeshers_NumberOfSegments * nbHyp1, *nbHyp2;
      nbHyp1 = new StdMeshers_NumberOfSegments( _sgen.GetANewId(), &_sgen );
      nbHyp1->SetNumberOfSegments( nbSeg1 );
      AddHypothesis( wires[0]->Edge(0), nbHyp1->GetID() );
      AddHypothesis( wires[0]->Edge(2), nbHyp1->GetID() );

      nbHyp2 = new StdMeshers_NumberOfSegments( _sgen.GetANewId(), &_sgen );
      nbHyp2->SetNumberOfSegments( nbSeg2 );
      AddHypothesis( wires[0]->Edge(1), nbHyp2->GetID() );
      AddHypothesis( wires[0]->Edge(3), nbHyp2->GetID() );

      if ( !_sgen.Compute( *this, GetShapeToMesh(), SMESH_Gen::SHAPE_ONLY_UPWARD ))
        return false;

      QuadAlgo algo2D( _sgen.GetANewId(), &_sgen );
      if ( !algo2D.Compute( _helper, wires[0] ))
        return false;

      // remove edges
      // for ( SMDS_ElemIteratorPtr eIt = _meshDS->elementsIterator( SMDSAbs_Edge ); eIt->more(); )
      //   _meshDS->RemoveFreeElement( eIt->next(), /*sm=*/0, /*groups=*/false );

      // _meshDS->Modified(); // setMyModified();
      // _meshDS->CompactMesh();

      // create TriaCoordSys for every triangle
      if ( isSourceMesh )
      {
        for ( SMDS_ElemIteratorPtr fIt = _meshDS->elementsIterator( SMDSAbs_Face ); fIt->more(); )
        {
          const SMDS_MeshElement* tria = fIt->next();
          TriaCoordSys* triaLCS = _traiLCSPool.getNew();
          triaLCS->Init( SMESH_NodeXYZ( tria->GetNode( 0 )),
                         SMESH_NodeXYZ( tria->GetNode( 1 )),
                         SMESH_NodeXYZ( tria->GetNode( 2 )));
          // int i= tria->GetID() - NbEdges() - 1;
          // cout << "ID from TRIA " << i << " - poolSize " << _traiLCSPool.nbElements() <<
          //   ( _traiLCSPool[i]!= triaLCS ? " KO" : "" )  << endl;
        }
        _elemSearcher = SMESH_MeshAlgos::GetElementSearcher( *_meshDS );
      }
      return true;
    }
    // --------------------------------------------------------------------------------
    /*!
     * \brief Find a source triangle including a point and return its barycentric coordinates
     */
    const SMDS_MeshElement* FindFaceByPoint( const gp_Pnt p,
                                             double & bc1, double & bc2, double & bc3 )
    {
      const SMDS_MeshElement* tria = nullptr;
      gp_XYZ projPnt = _elemSearcher->Project( p, SMDSAbs_Face, &tria );

      int lcsID = tria->GetID() - NbEdges() - 1;
      const TriaCoordSys* triaLCS = _traiLCSPool[ lcsID ];
      triaLCS->GetBaryCoords( projPnt, bc1, bc2, bc3 );

      return tria;
    }
    // --------------------------------------------------------------------------------
    /*!
     * \brief Return a point lying on a corresponding target triangle
     */
    gp_Pnt GetPoint( const SMDS_MeshElement* srcTria, double & bc1, double & bc2, double & bc3 )
    {
      const SMDS_MeshElement* tgtTria = _meshDS->FindElement( srcTria->GetID() );
      gp_Pnt p = ( bc1 * SMESH_NodeXYZ( tgtTria->GetNode(0) ) +
                   bc2 * SMESH_NodeXYZ( tgtTria->GetNode(1) ) +
                   bc3 * SMESH_NodeXYZ( tgtTria->GetNode(2) ) );
      return p;
    }
    // --------------------------------------------------------------------------------
    /*!
     * \brief Return an UV of point lying on a corresponding target triangle
     */
    gp_XY GetUV( const SMDS_MeshElement* srcTria,
                 double & bc1, double & bc2, double & bc3 )
    {
      const SMDS_MeshElement* tgtTria = _meshDS->FindElement( srcTria->GetID() );
      TopoDS_Shape tgtShape = GetShapeToMesh();
      const TopoDS_Face& face = TopoDS::Face( tgtShape );

      gp_XY p = ( bc1 * _helper.GetNodeUV( face, tgtTria->GetNode(0) ) +
                  bc2 * _helper.GetNodeUV( face, tgtTria->GetNode(1) ) +
                  bc3 * _helper.GetNodeUV( face, tgtTria->GetNode(2) ) );
      return p;
    }
  };

  //================================================================================
  /*!
   * \brief Calculate average size of faces
   *         Actually calculate average of min and max face size
   */
  //================================================================================

  double calcAverageFaceSize( SMESHDS_SubMesh* sm )
  {
    double minLen = Precision::Infinite(), maxLen = 0;
    for ( SMDS_ElemIteratorPtr fIt = sm->GetElements(); fIt->more(); )
    {
      const SMDS_MeshElement* face = fIt->next();
      int nbNodes = face->NbCornerNodes();
      gp_XYZ pPrev = SMESH_NodeXYZ( face->GetNode( nbNodes - 1 ));
      for ( int i = 0; i < nbNodes; ++i )
      {
        SMESH_NodeXYZ p( face->GetNode( i ));
        double len = ( p - pPrev ).SquareModulus();
        minLen = Min( len, minLen );
        maxLen = Max( len, maxLen );
        pPrev = p;
      }
    }
    return 0.5 * ( Sqrt( minLen ) + Sqrt( maxLen ));
  }

  //================================================================================
  /*!
   * \brief Perform projection from a quadrilateral FACE to another quadrilateral one
   */
  //================================================================================

  bool projectQuads(const TopoDS_Face&                 tgtFace,
                    const TopoDS_Face&                 srcFace,
                    const TSideVector&                 tgtWires,
                    const TSideVector&                 srcWires,
                    const TAssocTool::TShapeShapeMap&  shape2ShapeMap,
                    TAssocTool::TNodeNodeMap&          src2tgtNodes,
                    const bool                         is1DComputed)
  {
    SMESH_Mesh * tgtMesh = tgtWires[0]->GetMesh();
    SMESH_Mesh * srcMesh = srcWires[0]->GetMesh();
    SMESHDS_Mesh * tgtMeshDS = tgtMesh->GetMeshDS();
    SMESHDS_Mesh * srcMeshDS = srcMesh->GetMeshDS();

    if ( srcWires.size() != 1 || // requirements below can be weaken
         SMESH_MesherHelper::Count( tgtFace, TopAbs_EDGE, /*ignoreSame=*/true) != 4 ||
         SMESH_MesherHelper::Count( srcFace, TopAbs_EDGE, /*ignoreSame=*/true) != 4 )
      return false;

    // make auxiliary structured meshes that will be used to get corresponding
    // points on the target FACE
    QuadMesh srcQuadMesh( srcFace ), tgtQuadMesh( tgtFace );
    double avgSize = calcAverageFaceSize( srcMeshDS->MeshElements( srcFace ));
    int nbSeg1 = (int) Max( 2., Max( srcWires[0]->EdgeLength(0),
                                     srcWires[0]->EdgeLength(2)) / avgSize );
    int nbSeg2 = (int) Max( 2., Max( srcWires[0]->EdgeLength(1),
                                     srcWires[0]->EdgeLength(3)) / avgSize );
    if ( !srcQuadMesh.Compute( srcWires, nbSeg1, nbSeg2, /*isSrc=*/true ) ||
         !tgtQuadMesh.Compute( tgtWires, nbSeg1, nbSeg2, /*isSrc=*/false ))
      return false;

    // Make new faces

    // prepare the helper to adding quadratic elements if necessary
    SMESH_MesherHelper* helper = tgtWires[0]->FaceHelper();
    helper->IsQuadraticSubMesh( tgtFace );

    SMESHDS_SubMesh* srcSubDS = srcMeshDS->MeshElements( srcFace );
    if ( !is1DComputed && srcSubDS->NbElements() )
      helper->SetIsQuadratic( srcSubDS->GetElements()->next()->IsQuadratic() );

    SMESH_MesherHelper* srcHelper = srcWires[0]->FaceHelper();
    SMESH_MesherHelper  edgeHelper( *tgtMesh );
    edgeHelper.ToFixNodeParameters( true );

    const SMDS_MeshNode* nullNode = 0;
    TAssocTool::TNodeNodeMap::iterator srcN_tgtN;

    SMDS_ElemIteratorPtr elemIt = srcSubDS->GetElements();
    vector< const SMDS_MeshNode* > tgtNodes;
    while ( elemIt->more() ) // loop on all mesh faces on srcFace
    {
      const SMDS_MeshElement* elem = elemIt->next();
      const int nbN = elem->NbCornerNodes();
      tgtNodes.resize( nbN );
      helper->SetElementsOnShape( false );
      for ( int i = 0; i < nbN; ++i ) // loop on nodes of the source element
      {
        const SMDS_MeshNode* srcNode = elem->GetNode(i);
        srcN_tgtN = src2tgtNodes.insert( make_pair( srcNode, nullNode )).first;
        if ( srcN_tgtN->second == nullNode )
        {
          // create a new node
          gp_Pnt srcP = SMESH_TNodeXYZ( srcNode );
          double bc[3];
          const SMDS_MeshElement* auxF = srcQuadMesh.FindFaceByPoint( srcP, bc[0], bc[1], bc[2] );
          gp_Pnt                  tgtP = tgtQuadMesh.GetPoint( auxF, bc[0], bc[1], bc[2] );
          SMDS_MeshNode* n = helper->AddNode( tgtP.X(), tgtP.Y(), tgtP.Z() );
          srcN_tgtN->second = n;
          switch ( srcNode->GetPosition()->GetTypeOfPosition() )
          {
          case SMDS_TOP_FACE:
          {
            gp_XY tgtUV = tgtQuadMesh.GetUV( auxF, bc[0], bc[1], bc[2] );
            tgtMeshDS->SetNodeOnFace( n, helper->GetSubShapeID(), tgtUV.X(), tgtUV.Y() );
            break;
          }
          case SMDS_TOP_EDGE:
          {
            const TopoDS_Edge& srcE = TopoDS::Edge( srcMeshDS->IndexToShape( srcNode->GetShapeID()));
            const TopoDS_Edge& tgtE = TopoDS::Edge( shape2ShapeMap( srcE, /*isSrc=*/true ));
            double srcU = srcHelper->GetNodeU( srcE, srcNode );
            tgtMeshDS->SetNodeOnEdge( n, tgtE, srcU );
            edgeHelper.SetSubShape( tgtE );
            double tol = BRep_Tool::MaxTolerance( tgtE, TopAbs_VERTEX ), distXYZ[4];
            /*isOk = */edgeHelper.CheckNodeU( tgtE, n, srcU, 2 * tol, /*force=*/true, distXYZ );
            //if ( isOk )
            tgtMeshDS->MoveNode( n, distXYZ[1], distXYZ[2], distXYZ[3] );
            SMDS_EdgePositionPtr( n->GetPosition() )->SetUParameter( srcU );
            break;
          }
          case SMDS_TOP_VERTEX:
          {
            const TopoDS_Shape & srcV = srcMeshDS->IndexToShape( srcNode->getshapeId() );
            const TopoDS_Shape & tgtV = shape2ShapeMap( srcV, /*isSrc=*/true );
            tgtP = BRep_Tool::Pnt( TopoDS::Vertex( tgtV ));
            tgtMeshDS->MoveNode( n, tgtP.X(), tgtP.Y(), tgtP.Z() );
            tgtMeshDS->SetNodeOnVertex( n, TopoDS::Vertex( tgtV ));
            break;
          }
          default:;
          }
        }
        tgtNodes[i] = srcN_tgtN->second;
      }
      // create a new face
      helper->SetElementsOnShape( true );
      switch ( nbN )
      {
      case 3: helper->AddFace(tgtNodes[0], tgtNodes[1], tgtNodes[2]); break;
      case 4: helper->AddFace(tgtNodes[0], tgtNodes[1], tgtNodes[2], tgtNodes[3]); break;
      default: helper->AddPolygonalFace( tgtNodes );
      }
    } //  // loop on all mesh faces on srcFace

    return true;

    // below is projection of a structured source mesh

    // if ( !is1DComputed )
    //   return false;
    // for ( int iE = 0; iE < 4; ++iE )
    // {
    //   SMESHDS_SubMesh* sm = srcMeshDS->MeshElements( srcWires[0]->Edge( iE ));
    //   if ( !sm ) return false;
    //   if ( sm->NbNodes() + sm->NbElements() == 0 ) return false;
    // }
    // if ( BRepAdaptor_Surface( tgtFace ).GetType() != GeomAbs_Plane )
    //   return false;
    // // if ( BRepAdaptor_Surface( tgtFace ).GetType() == GeomAbs_Plane &&
    // //      BRepAdaptor_Surface( srcFace ).GetType() == GeomAbs_Plane )
    // //   return false; // too easy

    // // load EDGEs to SMESH_Block

    // SMESH_Block block;
    // TopTools_IndexedMapOfOrientedShape blockSubShapes;
    // {
    //   const TopoDS_Solid& box = srcMesh->PseudoShape();
    //   TopoDS_Shell shell = TopoDS::Shell( TopExp_Explorer( box, TopAbs_SHELL ).Current() );
    //   TopoDS_Vertex v;
    //   block.LoadBlockShapes( shell, v, v, blockSubShapes ); // fill all since operator[] is missing
    // }
    // const SMESH_Block::TShapeID srcFaceBID = SMESH_Block::ID_Fxy0;
    // const SMESH_Block::TShapeID tgtFaceBID = SMESH_Block::ID_Fxy1;
    // vector< int > edgeBID;
    // block.GetFaceEdgesIDs( srcFaceBID, edgeBID ); // u0, u1, 0v, 1v
    // blockSubShapes.Substitute( edgeBID[0], srcWires[0]->Edge(0) );
    // blockSubShapes.Substitute( edgeBID[1], srcWires[0]->Edge(2) );
    // blockSubShapes.Substitute( edgeBID[2], srcWires[0]->Edge(3) );
    // blockSubShapes.Substitute( edgeBID[3], srcWires[0]->Edge(1) );
    // block.GetFaceEdgesIDs( tgtFaceBID, edgeBID ); // u0, u1, 0v, 1v
    // blockSubShapes.Substitute( edgeBID[0], tgtWires[0]->Edge(0) );
    // blockSubShapes.Substitute( edgeBID[1], tgtWires[0]->Edge(2) );
    // blockSubShapes.Substitute( edgeBID[2], tgtWires[0]->Edge(3) );
    // blockSubShapes.Substitute( edgeBID[3], tgtWires[0]->Edge(1) );
    // block.LoadFace( srcFace, srcFaceBID, blockSubShapes );
    // block.LoadFace( tgtFace, tgtFaceBID, blockSubShapes );

    // // remember connectivity of new faces in terms of ( node-or-XY )

    // typedef std::pair< const SMDS_MeshNode*, gp_XYZ > TNodeOrXY; // node-or-XY
    // typedef std::vector< TNodeOrXY* >                 TFaceConn; // face connectivity
    // std::vector< TFaceConn >                    newFacesVec;     // connectivity of all faces
    // std::map< const SMDS_MeshNode*, TNodeOrXY > srcNode2tgtNXY;  // src node -> node-or-XY

    // TAssocTool::TNodeNodeMap::iterator                                       srcN_tgtN;
    // std::map< const SMDS_MeshNode*, TNodeOrXY >::iterator                    srcN_tgtNXY;
    // std::pair< std::map< const SMDS_MeshNode*, TNodeOrXY >::iterator, bool > n2n_isNew;
    // TNodeOrXY nullNXY( (SMDS_MeshNode*)NULL, gp_XYZ(0,0,0) );

    // SMESHDS_SubMesh* srcSubDS = srcMeshDS->MeshElements( srcFace );
    // newFacesVec.resize( srcSubDS->NbElements() );
    // int iFaceSrc = 0;

    // SMDS_ElemIteratorPtr elemIt = srcSubDS->GetElements();
    // while ( elemIt->more() ) // loop on all mesh faces on srcFace
    // {
    //   const SMDS_MeshElement* elem = elemIt->next();
    //   TFaceConn& tgtNodes = newFacesVec[ iFaceSrc++ ];

    //   const int nbN = elem->NbCornerNodes();
    //   tgtNodes.resize( nbN );
    //   for ( int i = 0; i < nbN; ++i ) // loop on nodes of the source element
    //   {
    //     const SMDS_MeshNode* srcNode = elem->GetNode(i);
    //     n2n_isNew = srcNode2tgtNXY.insert( make_pair( srcNode, nullNXY ));
    //     TNodeOrXY & tgtNodeOrXY = n2n_isNew.first->second;
    //     if ( n2n_isNew.second ) // new src node encounters
    //     {
    //       srcN_tgtN = src2tgtNodes.find( srcNode );
    //       if ( srcN_tgtN != src2tgtNodes.end() )
    //       {
    //         tgtNodeOrXY.first = srcN_tgtN->second; // tgt node exists
    //       }
    //       else
    //       {
    //         // find XY of src node within the quadrilateral srcFace
    //         if ( !block.ComputeParameters( SMESH_TNodeXYZ( srcNode ),
    //                                        tgtNodeOrXY.second, srcFaceBID ))
    //           return false;
    //       }
    //     }
    //     tgtNodes[ i ] = & tgtNodeOrXY;
    //   }
    // }

    // // as all XY are computed, create tgt nodes and faces

    // SMESH_MesherHelper helper = *tgtWires[0]->FaceHelper();
    // if ( is1DComputed )
    //   helper.IsQuadraticSubMesh( tgtFace );
    // else
    //   helper.SetIsQuadratic( srcSubDS->GetElements()->next()->IsQuadratic() );
    // helper.SetElementsOnShape( true );
    // Handle(Geom_Surface) tgtSurface = BRep_Tool::Surface( tgtFace );

    // SMESH_MesherHelper srcHelper = *srcWires[0]->FaceHelper();

    // vector< const SMDS_MeshNode* > tgtNodes;
    // gp_XY uv;

    // for ( size_t iFaceTgt = 0; iFaceTgt < newFacesVec.size(); ++iFaceTgt )
    // {
    //   TFaceConn& tgtConn = newFacesVec[ iFaceTgt ];
    //   tgtNodes.resize( tgtConn.size() );
    //   for ( size_t iN = 0; iN < tgtConn.size(); ++iN )
    //   {
    //     const SMDS_MeshNode* & tgtN = tgtConn[ iN ]->first;
    //     if ( !tgtN ) // create a node
    //     {
    //       if ( !block.FaceUV( tgtFaceBID, tgtConn[iN]->second, uv ))
    //         return false;
    //       gp_Pnt p = tgtSurface->Value( uv.X(), uv.Y() );
    //       tgtN = helper.AddNode( p.X(), p.Y(), p.Z(), uv.X(), uv.Y() );
    //     }
    //     tgtNodes[ tgtNodes.size() - iN - 1] = tgtN; // reversed orientation
    //   }
    //   switch ( tgtNodes.size() )
    //   {
    //   case 3: helper.AddFace(tgtNodes[0], tgtNodes[1], tgtNodes[2]); break;
    //   case 4: helper.AddFace(tgtNodes[0], tgtNodes[1], tgtNodes[2], tgtNodes[3]); break;
    //   default:
    //     if ( tgtNodes.size() > 4 )
    //       helper.AddPolygonalFace( tgtNodes );
    //   }
    // }
    return false; //true;

  } // bool projectQuads(...)

  //================================================================================
  /*!
   * \brief Fix bad faces by smoothing
   */
  //================================================================================

  bool fixDistortedFaces( SMESH_MesherHelper& helper,
                          TSideVector&        tgtWires )
  {
    SMESH_subMesh* faceSM = helper.GetMesh()->GetSubMesh( helper.GetSubShape() );

    //if ( helper.IsDistorted2D( faceSM, /*checkUV=*/true ))
    {
      SMESH_MeshEditor editor( helper.GetMesh() );
      SMESHDS_SubMesh* smDS = faceSM->GetSubMeshDS();
      const TopoDS_Face&  F = TopoDS::Face( faceSM->GetSubShape() );

      TIDSortedElemSet faces;
      SMDS_ElemIteratorPtr faceIt = smDS->GetElements();
      for ( faceIt = smDS->GetElements(); faceIt->more(); )
        faces.insert( faces.end(), faceIt->next() );

      // choose smoothing algo
      //SMESH_MeshEditor:: SmoothMethod algo = SMESH_MeshEditor::CENTROIDAL;
      bool isConcaveBoundary = false;
      for ( size_t iW = 0; iW < tgtWires.size() && !isConcaveBoundary; ++iW )
      {
        TopoDS_Edge prevEdge = tgtWires[iW]->Edge( tgtWires[iW]->NbEdges() - 1 );
        for ( int iE = 0; iE < tgtWires[iW]->NbEdges() && !isConcaveBoundary; ++iE )
        {
          double angle = helper.GetAngle( prevEdge, tgtWires[iW]->Edge( iE ),
                                          F,        tgtWires[iW]->FirstVertex( iE ));
          isConcaveBoundary = ( angle < -5. * M_PI / 180. );

          prevEdge = tgtWires[iW]->Edge( iE );
        }
      }
      SMESH_MeshEditor:: SmoothMethod algo =
        isConcaveBoundary ? SMESH_MeshEditor::CENTROIDAL : SMESH_MeshEditor::LAPLACIAN;

      // smooth in 2D or 3D?
      TopLoc_Location loc;
      Handle(Geom_Surface) surface = BRep_Tool::Surface( F, loc );
      bool isPlanar = GeomLib_IsPlanarSurface( surface ).IsPlanar();

      // smoothing
      set<const SMDS_MeshNode*> fixedNodes;
      editor.Smooth( faces, fixedNodes, algo, /*nbIterations=*/ 10,
                     /*theTgtAspectRatio=*/1.0, /*the2D=*/!isPlanar);

      helper.ToFixNodeParameters( true );

      return !helper.IsDistorted2D( faceSM, /*checkUV=*/true );
    }
    return true;
  }

  //=======================================================================
  /*
   * Set initial association of VERTEXes for the case of projection
   * from a quadrangle FACE to a closed FACE, where opposite src EDGEs
   * have different nb of segments
   */
  //=======================================================================

  void initAssoc4Quad2Closed(const TopoDS_Shape&          tgtFace,
                             SMESH_MesherHelper&          tgtHelper,
                             const TopoDS_Shape&          srcFace,
                             SMESH_Mesh*                  srcMesh,
                             TAssocTool::TShapeShapeMap & assocMap)
  {
    if ( !tgtHelper.HasRealSeam() || srcFace.ShapeType() != TopAbs_FACE )
      return; // no seam edge
    list< TopoDS_Edge > tgtEdges, srcEdges;
    list< int > tgtNbEW, srcNbEW;
    int tgtNbW = SMESH_Block::GetOrderedEdges( TopoDS::Face( tgtFace ), tgtEdges, tgtNbEW );
    int srcNbW = SMESH_Block::GetOrderedEdges( TopoDS::Face( srcFace ), srcEdges, srcNbEW );
    if ( tgtNbW != 1 || srcNbW != 1 ||
         tgtNbEW.front() != 4 || srcNbEW.front() != 4 )
      return; // not quads

    smIdType srcNbSeg[4];
    list< TopoDS_Edge >::iterator edgeS = srcEdges.begin(), edgeT = tgtEdges.begin();
    for ( int i = 0; edgeS != srcEdges.end(); ++i, ++edgeS )
      if ( SMESHDS_SubMesh* sm = srcMesh->GetMeshDS()->MeshElements( *edgeS ))
        srcNbSeg[ i ] = sm->NbNodes();
      else
        return; // not meshed
    if ( srcNbSeg[0] == srcNbSeg[2] && srcNbSeg[1] == srcNbSeg[3] )
      return; // same nb segments
    if ( srcNbSeg[0] != srcNbSeg[2] && srcNbSeg[1] != srcNbSeg[3] )
      return; // all different nb segments

    edgeS = srcEdges.begin();
    if ( srcNbSeg[0] != srcNbSeg[2] )
      ++edgeS;
    TAssocTool::InsertAssociation( tgtHelper.IthVertex( 0,*edgeT ),
                                   tgtHelper.IthVertex( 0,*edgeS ), assocMap );
    TAssocTool::InsertAssociation( tgtHelper.IthVertex( 1,*edgeT ),
                                   tgtHelper.IthVertex( 1,*edgeS ), assocMap );
  }

  //================================================================================
  /*!
   * \brief Find sub-shape association such that corresponding VERTEXes of
   *        two corresponding FACEs lie on lines parallel to thePiercingLine
   */
  //================================================================================

  bool findSubShapeAssociationByPiercing( const TopoDS_Face&          theTgtFace,
                                          SMESH_Mesh *                /*theTgtMesh*/,
                                          const TopoDS_Shape&         theSrcShape,
                                          SMESH_Mesh*                 theSrcMesh,
                                          TAssocTool::TShapeShapeMap& theShape2ShapeMap,
                                          Handle(Geom_Line) &         thePiercingLine )
  {
    list< TopoDS_Edge > tgtEdges, srcEdges;
    list< int > tgtNbEW, srcNbEW;
    int tgtNbW = SMESH_Block::GetOrderedEdges( TopoDS::Face( theTgtFace ), tgtEdges, tgtNbEW );

    TopTools_IndexedMapOfShape tgtVV, srcVV, srcVVtemp;
    for ( const TopoDS_Edge& tgtEdge : tgtEdges )
      tgtVV.Add( SMESH_MesherHelper::IthVertex( 0, tgtEdge ));
    // if ( tgtVV.Size() < 2 )
    //   return false;

    const int       nbVV = tgtVV.Size();
    const gp_Pnt   tgtP0 = BRep_Tool::Pnt( TopoDS::Vertex( tgtVV( 1 )));
    double minVertexDist = Precision::Infinite(), assocTol;
    gp_Lin      piercingLine;
    TopoDS_Face assocSrcFace;
    double      tol;

    for ( TopExp_Explorer faceExp( theSrcShape, TopAbs_FACE ); faceExp.More(); faceExp.Next())
    {
      const TopoDS_Face& srcFace = TopoDS::Face( faceExp.Current() );

      srcEdges.clear();
      srcNbEW.clear();
      int srcNbW = SMESH_Block::GetOrderedEdges( srcFace, srcEdges, srcNbEW );
      if ( tgtNbW != srcNbW )
        continue;

      srcVVtemp.Clear( false );
      for ( const TopoDS_Edge& srcEdge : srcEdges )
        srcVVtemp.Add( SMESH_MesherHelper::IthVertex( 0, srcEdge ));
      if ( srcVVtemp.Extent() != tgtVV.Extent() )
        continue;

      // make srcFace computed
      SMESH_subMesh* srcFaceSM = theSrcMesh->GetSubMesh( srcFace );
      if ( !TAssocTool::MakeComputed( srcFaceSM ))
        continue;

      // compute tolerance
      double sumLen = 0, nbEdges = 0;
      for ( const TopoDS_Edge& srcEdge : srcEdges )
      {
        SMESH_subMesh* srcSM = theSrcMesh->GetSubMesh( srcEdge );
        if ( !srcSM->GetSubMeshDS() )
          continue;
        SMDS_ElemIteratorPtr edgeIt = srcSM->GetSubMeshDS()->GetElements();
        while ( edgeIt->more() )
        {
          const SMDS_MeshElement* edge = edgeIt->next();
          sumLen += SMESH_NodeXYZ( edge->GetNode( 0 )).Distance( edge->GetNode( 1 ));
          nbEdges += 1;
        }
      }
      if ( nbEdges == 0 )
        continue;

      tol = 0.1 * sumLen / nbEdges;

      // try to find corresponding VERTEXes

      gp_Lin line;
      double vertexDist;
      for ( int iSrcV0 = 1; iSrcV0 <= srcVVtemp.Size(); ++iSrcV0 )
      {
        const gp_Pnt srcP0 = BRep_Tool::Pnt( TopoDS::Vertex( srcVVtemp( iSrcV0 )));
        try {
          line.SetDirection( gp_Vec( srcP0, tgtP0 ));
        }
        catch (...) {
          continue;
        }
        bool correspond;
        for ( int iDir : { -1, 1 }) // move connected VERTEX forward and backward
        {
          correspond = true;
          vertexDist = 0;
          int iTgtV = 0, iSrcV = iSrcV0 - 1;
          for ( int i = 1; i < tgtVV.Size() &&   correspond; ++i )
          {
            iTgtV = ( iTgtV + 1           ) % nbVV;
            iSrcV = ( iSrcV + iDir + nbVV ) % nbVV;
            gp_Pnt tgtP = BRep_Tool::Pnt( TopoDS::Vertex( tgtVV( iTgtV + 1 )));
            gp_Pnt srcP = BRep_Tool::Pnt( TopoDS::Vertex( srcVVtemp( iSrcV + 1 )));
            line.SetLocation( tgtP );
            correspond = ( line.SquareDistance( srcP ) < tol * tol );
            vertexDist += tgtP.SquareDistance( srcP );
          }
          if ( correspond )
            break;
        }
        if ( correspond )
        {
          if ( vertexDist < minVertexDist )
          {
            minVertexDist = vertexDist;
            piercingLine    = line;
            assocSrcFace  = srcFace;
            assocTol      = tol;
            srcVV.Clear(false);
            for ( const TopoDS_Shape& srcVtemp : srcVVtemp )
            {
              srcVV.Add( srcVtemp );
            }
          }
          break;
        }
      }
      continue;

    } // loop on src FACEs

    if ( Precision::IsInfinite( minVertexDist ))
      return false; // no correspondence found

    thePiercingLine = new Geom_Line( piercingLine );

    // fill theShape2ShapeMap

    TAssocTool::InsertAssociation( theTgtFace, assocSrcFace, theShape2ShapeMap );

    for ( const TopoDS_Shape& tgtV : tgtVV ) // fill theShape2ShapeMap with VERTEXes
    {
      gp_Pnt tgtP = BRep_Tool::Pnt( TopoDS::Vertex( tgtV ));
      piercingLine.SetLocation( tgtP );
      bool found = false;
      for ( const TopoDS_Shape& srcV : srcVV )
      {
        gp_Pnt srcP = BRep_Tool::Pnt( TopoDS::Vertex( srcV ));
        if ( piercingLine.SquareDistance( srcP ) < assocTol * assocTol )
        {
          TAssocTool::InsertAssociation( tgtV, srcV, theShape2ShapeMap );
          found = true;
          break;
        }
      }
      if ( !found )
        return false;
    }

    TopoDS_Vertex vvT[2], vvS[2], vvMapped[2];
    for ( const TopoDS_Edge& tgtEdge : tgtEdges ) // fill theShape2ShapeMap with EDGEs
    {
      if ( SMESH_Algo::isDegenerated( tgtEdge ))
        continue;

      TopExp::Vertices( tgtEdge, vvT[0], vvT[1], true );
      if ( !theShape2ShapeMap.IsBound( vvT[0] ) ||
           !theShape2ShapeMap.IsBound( vvT[1] ))
        return false;

      vvMapped[0] = TopoDS::Vertex( theShape2ShapeMap( vvT[0] ));
      vvMapped[1] = TopoDS::Vertex( theShape2ShapeMap( vvT[1] ));

      bool found = false;
      for ( TopExp_Explorer eExp( assocSrcFace, TopAbs_EDGE ); eExp.More(); eExp.Next())
      {
        TopoDS_Edge srcEdge = TopoDS::Edge( eExp.Current() );
        TopExp::Vertices( srcEdge, vvS[0], vvS[1], true );
        found = (( vvMapped[0].IsSame( vvS[0] ) && vvMapped[1].IsSame( vvS[1] )) ||
                 ( vvMapped[0].IsSame( vvS[1] ) && vvMapped[1].IsSame( vvS[0] )));

        if ( found && nbVV < 3 )
        {
          BRepAdaptor_Curve tgtCurve( tgtEdge );
          gp_Pnt tgtP = tgtCurve.Value( 0.5 * ( tgtCurve.FirstParameter() +
                                                tgtCurve.LastParameter() ));
          thePiercingLine->SetLocation( tgtP );

          double f,l;
          Handle(Geom_Curve) srcCurve = BRep_Tool::Curve( srcEdge, f,l );
          if ( srcCurve.IsNull() )
          {
            found = false;
            continue;
          }
          GeomAPI_ExtremaCurveCurve extrema( thePiercingLine, srcCurve );
          if ( !extrema.Extrema().IsDone() ||
               extrema.Extrema().IsParallel() ||
               extrema.NbExtrema() == 0 ||
               extrema.LowerDistance() > tol )
            found = false;
        }
        if ( found )
        {
          if ( !vvMapped[0].IsSame( vvS[0] ))
            srcEdge.Reverse();
          TAssocTool::InsertAssociation( tgtEdge, srcEdge, theShape2ShapeMap );
          break;
        }
      }
      if ( !found )
        return false;
    }

    return true;

  } // findSubShapeAssociationByPiercing()

    //================================================================================
  /*!
   * \brief Project by piercing theTgtFace by lines parallel to thePiercingLine
   */
  //================================================================================

  bool projectByPiercing(Handle(Geom_Line)                  thePiercingLine,
                         const TopoDS_Face&                 theTgtFace,
                         const TopoDS_Face&                 theSrcFace,
                         const TSideVector&                 theTgtWires,
                         const TSideVector&                 theSrcWires,
                         const TAssocTool::TShapeShapeMap&  theShape2ShapeMap,
                         TAssocTool::TNodeNodeMap&          theSrc2tgtNodes,
                         const bool                         theIs1DComputed)
  {
    SMESH_Mesh * tgtMesh = theTgtWires[0]->GetMesh();
    SMESH_Mesh * srcMesh = theSrcWires[0]->GetMesh();

    if ( thePiercingLine.IsNull() )
    {
      // try to set thePiercingLine by VERTEX association of theShape2ShapeMap

      const double tol = 0.1 * theSrcWires[0]->Length() / theSrcWires[0]->NbSegments();

      for ( TopExp_Explorer vExp( theTgtFace, TopAbs_VERTEX ); vExp.More(); vExp.Next() )
      {
        const TopoDS_Vertex & tgtV = TopoDS::Vertex( vExp.Current() );
        const TopoDS_Vertex & srcV = TopoDS::Vertex( theShape2ShapeMap( tgtV ));
        gp_Pnt tgtP = BRep_Tool::Pnt( tgtV );
        gp_Pnt srcP = BRep_Tool::Pnt( srcV );
        if ( thePiercingLine.IsNull() ) // set thePiercingLine
        {
          gp_Lin line;
          try {
            line.SetDirection( gp_Vec( srcP, tgtP ));
            line.SetLocation( tgtP );
            thePiercingLine = new Geom_Line( line );
          }
          catch ( ... )
          {
            continue;
          }
        }
        else // check thePiercingLine
        {
          thePiercingLine->SetLocation( tgtP );
          if ( thePiercingLine->Lin().SquareDistance( srcP ) > tol * tol )
            return false;
        }
      }

      for ( TopExp_Explorer eExp( theTgtFace, TopAbs_EDGE ); eExp.More(); eExp.Next() )
      {
        BRepAdaptor_Curve tgtCurve( TopoDS::Edge( eExp.Current() ));
        gp_Pnt tgtP = tgtCurve.Value( 0.5 * ( tgtCurve.FirstParameter() +
                                              tgtCurve.LastParameter() ));
        thePiercingLine->SetLocation( tgtP );

        double f,l;
        TopoDS_Edge srcEdge = TopoDS::Edge( theShape2ShapeMap( eExp.Current() ));
        Handle(Geom_Curve) srcCurve = BRep_Tool::Curve( srcEdge, f,l );
        if ( srcCurve.IsNull() )
          continue;
        GeomAPI_ExtremaCurveCurve extrema( thePiercingLine, srcCurve,
                                           -Precision::Infinite(), Precision::Infinite(), f, l );
        if ( !extrema.Extrema().IsDone() ||
             extrema.Extrema().IsParallel() ||
             extrema.NbExtrema() == 0 ||
             extrema.LowerDistance() > tol )
          return false;
      }
    } // if ( thePiercingLine.IsNull() )

    SMESHDS_SubMesh* srcSubDS = srcMesh->GetMeshDS()->MeshElements( theSrcFace );

    SMESH_MesherHelper* helper = theTgtWires[0]->FaceHelper();
    if ( theIs1DComputed )
      helper->IsQuadraticSubMesh( theTgtFace );
    else
      helper->SetIsQuadratic( srcSubDS->GetElements()->next()->IsQuadratic() );
    helper->SetElementsOnShape( true );
    SMESHDS_Mesh* tgtMeshDS = tgtMesh->GetMeshDS();

    Handle(Geom_Surface)             tgtSurface = BRep_Tool::Surface( theTgtFace );
#if OCC_VERSION_LARGE < 0x07070000
    Handle(GeomAdaptor_HSurface) tgtSurfAdaptor = new GeomAdaptor_HSurface( tgtSurface );
    Handle(GeomAdaptor_HCurve)    piercingCurve = new GeomAdaptor_HCurve( thePiercingLine );
#else
    Handle(GeomAdaptor_Surface) tgtSurfAdaptor = new GeomAdaptor_Surface( tgtSurface );
    Handle(GeomAdaptor_Curve)    piercingCurve = new GeomAdaptor_Curve( thePiercingLine );
#endif
    IntCurveSurface_HInter intersect;

    SMESH_MesherHelper* srcHelper = theSrcWires[0]->FaceHelper();

    const SMDS_MeshNode* nullNode = 0;
    TAssocTool::TNodeNodeMap::iterator srcN_tgtN;
    vector< const SMDS_MeshNode* > tgtNodes;

    SMDS_ElemIteratorPtr elemIt = srcSubDS->GetElements();
    while ( elemIt->more() ) // loop on all mesh faces on srcFace
    {
      const SMDS_MeshElement* elem = elemIt->next();
      const int nbN = elem->NbCornerNodes();
      tgtNodes.resize( nbN );
      for ( int i = 0; i < nbN; ++i ) // loop on nodes of the source element
      {
        const SMDS_MeshNode* srcNode = elem->GetNode(i);
        srcN_tgtN = theSrc2tgtNodes.insert( make_pair( srcNode, nullNode )).first;
        if ( srcN_tgtN->second == nullNode )
        {
          // create a new node
          thePiercingLine->SetLocation( SMESH_NodeXYZ( srcNode ));
          intersect.Perform( piercingCurve, tgtSurfAdaptor );
          bool pierced = ( intersect.IsDone() && intersect.NbPoints() > 0 );
          double U, V;
          const SMDS_MeshNode* n = nullNode;
          if ( pierced )
          {
            double W, minW = Precision::Infinite();
            gp_Pnt tgtP;
            for ( int iInt = 1; iInt <= intersect.NbPoints(); ++iInt )
            {
              W = intersect.Point( iInt ).W();
              if ( 0 < W && W < minW )
              {
                U    = intersect.Point( iInt ).U();
                V    = intersect.Point( iInt ).V();
                tgtP = intersect.Point( iInt ).Pnt();
                minW = W;
              }
            }
            n = tgtMeshDS->AddNode( tgtP.X(), tgtP.Y(), tgtP.Z() );
          }

          SMDS_TypeOfPosition shapeType = srcNode->GetPosition()->GetTypeOfPosition();
          TopoDS_Shape        srcShape;
          if ( shapeType != SMDS_TOP_FACE )
          {
            srcShape = srcHelper->GetSubShapeByNode( srcNode, srcHelper->GetMeshDS() );
            if ( !theShape2ShapeMap.IsBound( srcShape, /*isSrc=*/true ))
            {
              if ( n )  // INTERNAL shape w/o corresponding target shape (3D_mesh_Extrusion_02/E0)
                shapeType = SMDS_TOP_FACE;
              else
                return false;
            }
          }

          switch ( shapeType )
          {
          case SMDS_TOP_FACE: {
            if ( !n )
              return false;
            tgtMeshDS->SetNodeOnFace( n, helper->GetSubShapeID(), U, V );
            break;
          }
          case SMDS_TOP_EDGE: {
            TopoDS_Edge  tgtEdge = TopoDS::Edge( theShape2ShapeMap( srcShape, /*isSrc=*/true ));
            if ( n )
            {
              U = Precision::Infinite();
              helper->CheckNodeU( tgtEdge, n, U, Precision::PConfusion());
            }
            else
            {
              Handle(Geom_Curve) tgtCurve = BRep_Tool::Curve( tgtEdge, U,V );
              if ( tgtCurve.IsNull() )
                return false;
              GeomAPI_ExtremaCurveCurve extrema( thePiercingLine, tgtCurve );
              if ( !extrema.Extrema().IsDone() ||
                   extrema.Extrema().IsParallel() ||
                   extrema.NbExtrema() == 0 )
                return false;
              gp_Pnt pOnLine, pOnEdge;
              extrema.NearestPoints( pOnLine, pOnEdge );
              extrema.LowerDistanceParameters( V, U );
              n = tgtMeshDS->AddNode( pOnEdge.X(), pOnEdge.Y(), pOnEdge.Z() );
            }
            tgtMeshDS->SetNodeOnEdge( n, tgtEdge, U );
            break;
          }
          case SMDS_TOP_VERTEX: {
            TopoDS_Shape tgtV = theShape2ShapeMap( srcShape, /*isSrc=*/true );
            if ( !n )
            {
              gp_Pnt tgtP = BRep_Tool::Pnt( TopoDS::Vertex( tgtV ));
              n = tgtMeshDS->AddNode( tgtP.X(), tgtP.Y(), tgtP.Z() );
            }
            tgtMeshDS->SetNodeOnVertex( n, TopoDS::Vertex( tgtV ));
            break;
          }
          default:;
          }
          srcN_tgtN->second = n;
        }
        tgtNodes[i] = srcN_tgtN->second;
      }
      // create a new face (with reversed orientation)
      switch ( nbN )
      {
      case 3: helper->AddFace(tgtNodes[0], tgtNodes[2], tgtNodes[1]); break;
      case 4: helper->AddFace(tgtNodes[0], tgtNodes[3], tgtNodes[2], tgtNodes[1]); break;
      }
    }  // loop on all mesh faces on srcFace

    return true;

  } // projectByPiercing()



} // namespace


//=======================================================================
//function : Compute
//purpose  :
//=======================================================================

bool StdMeshers_Projection_2D::Compute(SMESH_Mesh& theMesh, const TopoDS_Shape& theShape)
{
  _src2tgtNodes.clear();

  if ( !_sourceHypo )
    return false;

  SMESH_Mesh * srcMesh = _sourceHypo->GetSourceMesh();
  SMESH_Mesh * tgtMesh = & theMesh;
  if ( !srcMesh )
    srcMesh = tgtMesh;

  SMESHDS_Mesh * meshDS = theMesh.GetMeshDS();
  SMESH_MesherHelper helper( theMesh );

  // ---------------------------
  // Make sub-shapes association
  // ---------------------------

  TopoDS_Face   tgtFace = TopoDS::Face( theShape.Oriented(TopAbs_FORWARD));
  TopoDS_Shape srcShape = _sourceHypo->GetSourceFace().Oriented(TopAbs_FORWARD);

  helper.SetSubShape( tgtFace );

  TAssocTool::TShapeShapeMap shape2ShapeMap;
  TAssocTool::InitVertexAssociation( _sourceHypo, shape2ShapeMap );
  if ( shape2ShapeMap.IsEmpty() )
    initAssoc4Quad2Closed( tgtFace, helper, srcShape, srcMesh, shape2ShapeMap );

  Handle(Geom_Line) piercingLine;
  bool piercingTried = false;

  if ( !TAssocTool::FindSubShapeAssociation( tgtFace, tgtMesh, srcShape, srcMesh,
                                             shape2ShapeMap)  ||
       !shape2ShapeMap.IsBound( tgtFace ))
  {
    piercingTried = true;
    if ( !findSubShapeAssociationByPiercing( tgtFace, tgtMesh, srcShape, srcMesh,
                                             shape2ShapeMap, piercingLine ))
    {
      if ( srcShape.ShapeType() == TopAbs_FACE )
      {
        int nbE1 = helper.Count( tgtFace, TopAbs_EDGE, /*ignoreSame=*/true );
        int nbE2 = helper.Count( srcShape, TopAbs_EDGE, /*ignoreSame=*/true );
        if ( nbE1 != nbE2 )
          return error(COMPERR_BAD_SHAPE,
                       SMESH_Comment("Different number of edges in source and target faces: ")
                       << nbE2 << " and " << nbE1 );
      }
      return error(COMPERR_BAD_SHAPE,"Topology of source and target faces seems different" );
    }
  }
  TopoDS_Face srcFace = TopoDS::Face( shape2ShapeMap( tgtFace ).Oriented(TopAbs_FORWARD));

  // ----------------------------------------------
  // Assure that mesh on a source Face is computed
  // ----------------------------------------------

  SMESH_subMesh* srcSubMesh = srcMesh->GetSubMesh( srcFace );
  SMESH_subMesh* tgtSubMesh = tgtMesh->GetSubMesh( tgtFace );

  string srcMeshError;
  if ( tgtMesh == srcMesh ) {
    if ( !TAssocTool::MakeComputed( srcSubMesh ))
      srcMeshError = TAssocTool::SourceNotComputedError( srcSubMesh, this );
  }
  else {
    if ( !srcSubMesh->IsMeshComputed() )
      srcMeshError = TAssocTool::SourceNotComputedError();
  }
  if ( !srcMeshError.empty() )
    return error(COMPERR_BAD_INPUT_MESH, srcMeshError );

  // ===========
  // Projection
  // ===========

  // get ordered src and tgt EDGEs
  TSideVector srcWires, tgtWires;
  bool is1DComputed = false; // if any tgt EDGE is meshed
  TError err = getWires( tgtFace, srcFace, tgtMesh, srcMesh, &helper,
                         shape2ShapeMap, srcWires, tgtWires, _src2tgtNodes, is1DComputed );
  if ( err && !err->IsOK() )
    return error( err );

  bool projDone = false;

  if ( !projDone && !piercingLine.IsNull() )
  {
    // project by piercing tgtFace by lines parallel to piercingLine
    projDone = projectByPiercing( piercingLine, tgtFace, srcFace, tgtWires, srcWires,
                                  shape2ShapeMap, _src2tgtNodes, is1DComputed );
    piercingTried = true;
  }
  if ( !projDone )
  {
    // try to project from the same face with different location
    projDone = projectPartner( tgtFace, srcFace, tgtWires, srcWires,
                               shape2ShapeMap, _src2tgtNodes, is1DComputed );
  }
  if ( !projDone )
  {
    // projection in case if the faces are similar in 2D space
    projDone = projectBy2DSimilarity( tgtFace, srcFace, tgtWires, srcWires,
                                      shape2ShapeMap, _src2tgtNodes, is1DComputed );
  }
  if ( !projDone )
  {
    // projection in case of quadrilateral faces
    projDone = projectQuads( tgtFace, srcFace, tgtWires, srcWires,
                             shape2ShapeMap, _src2tgtNodes, is1DComputed);
  }
  if ( !projDone && !piercingTried )
  {
    // project by piercing tgtFace by lines parallel to piercingLine
    projDone = projectByPiercing( piercingLine, tgtFace, srcFace, tgtWires, srcWires,
                                  shape2ShapeMap, _src2tgtNodes, is1DComputed );
  }

  // it will remove mesh built on edges and vertices in failure case
  MeshCleaner cleaner( tgtSubMesh );

  if ( !projDone )
  {
    _src2tgtNodes.clear();
    // --------------------
    // Prepare to mapping
    // --------------------

    // Check if node projection to a face is needed
    Bnd_B2d uvBox;
    SMDS_ElemIteratorPtr faceIt = srcSubMesh->GetSubMeshDS()->GetElements();
    set< const SMDS_MeshNode* > faceNodes;
    for ( ; faceNodes.size() < 3 && faceIt->more();  ) {
      const SMDS_MeshElement* face = faceIt->next();
      SMDS_ElemIteratorPtr nodeIt = face->nodesIterator();
      while ( nodeIt->more() ) {
        const SMDS_MeshNode* node = static_cast<const SMDS_MeshNode*>( nodeIt->next() );
        if ( node->GetPosition()->GetTypeOfPosition() == SMDS_TOP_FACE &&
             faceNodes.insert( node ).second )
          uvBox.Add( helper.GetNodeUV( srcFace, node ));
      }
    }
    bool toProjectNodes = false;
    if ( faceNodes.size() == 1 )
      toProjectNodes = ( uvBox.IsVoid() || uvBox.CornerMin().IsEqual( gp_XY(0,0), 1e-12 ));
    else if ( faceNodes.size() > 1 )
      toProjectNodes = ( uvBox.IsVoid() || uvBox.SquareExtent() < DBL_MIN );

    // Find the corresponding source and target vertex
    // and <theReverse> flag needed to call mapper.Apply()

    TopoDS_Vertex srcV1, tgtV1;
    bool reverse = false;

    TopExp_Explorer vSrcExp( srcFace, TopAbs_VERTEX );
    srcV1 = TopoDS::Vertex( vSrcExp.Current() );
    tgtV1 = TopoDS::Vertex( shape2ShapeMap( srcV1, /*isSrc=*/true ));

    list< TopoDS_Edge > tgtEdges, srcEdges;
    list< int > nbEdgesInWires;
    SMESH_Block::GetOrderedEdges( tgtFace, tgtEdges, nbEdgesInWires, tgtV1 );
    SMESH_Block::GetOrderedEdges( srcFace, srcEdges, nbEdgesInWires, srcV1 );

    if ( nbEdgesInWires.front() > 1 ) // possible to find out orientation
    {
      TopoDS_Edge srcE1 = srcEdges.front(), tgtE1 = tgtEdges.front();
      TopoDS_Shape srcE1bis = shape2ShapeMap( tgtE1 );
      reverse = ( ! srcE1.IsSame( srcE1bis ));
      if ( ( reverse || srcE1.Orientation() != srcE1bis.Orientation() ) &&
           nbEdgesInWires.front() > 2 &&
           helper.IsRealSeam( tgtEdges.front() ))
      {
        if ( srcE1.Orientation() != srcE1bis.Orientation() )
          reverse = true;
        // projection to a face with seam EDGE; pb is that GetOrderedEdges()
        // always puts a seam EDGE first (if possible) and as a result
        // we can't use only theReverse flag to correctly associate source
        // and target faces in the mapper. Thus we select srcV1 so that
        // GetOrderedEdges() to return EDGEs in a needed order
        TopoDS_Face tgtFaceBis = tgtFace;
        TopTools_MapOfShape checkedVMap( tgtEdges.size() );
        checkedVMap.Add ( srcV1 );
        for ( vSrcExp.Next(); vSrcExp.More(); )
        {
          tgtFaceBis.Reverse();
          tgtEdges.clear();
          SMESH_Block::GetOrderedEdges( tgtFaceBis, tgtEdges, nbEdgesInWires, tgtV1 );
          bool ok = true;
          list< TopoDS_Edge >::iterator edgeS = srcEdges.begin(), edgeT = tgtEdges.begin();
          for ( ; edgeS != srcEdges.end() && ok ; ++edgeS, ++edgeT )
            ok = edgeT->IsSame( shape2ShapeMap( *edgeS, /*isSrc=*/true ));
          if ( ok )
            break; // FOUND!

          reverse = !reverse;
          if ( reverse )
          {
            vSrcExp.Next();
            while ( vSrcExp.More() && !checkedVMap.Add( vSrcExp.Current() ))
              vSrcExp.Next();
          }
          else
          {
            srcV1 = TopoDS::Vertex( vSrcExp.Current() );
            tgtV1 = TopoDS::Vertex( shape2ShapeMap( srcV1, /*isSrc=*/true ));
            srcEdges.clear();
            SMESH_Block::GetOrderedEdges( srcFace, srcEdges, nbEdgesInWires, srcV1 );
          }
        }
      }
      // for the case: project to a closed face from a non-closed face w/o vertex assoc;
      // avoid projecting to a seam from two EDGEs with different nb nodes on them
      // ( test mesh_Projection_2D_01/B1 )
      if ( !_sourceHypo->HasVertexAssociation() &&
           nbEdgesInWires.front() > 2 &&
           helper.IsRealSeam( tgtEdges.front() ))
      {
        TopoDS_Shape srcEdge1 = shape2ShapeMap( tgtEdges.front() );
        list< TopoDS_Edge >::iterator srcEdge2 =
          std::find( srcEdges.begin(), srcEdges.end(), srcEdge1);
        list< TopoDS_Edge >::iterator srcEdge3 =
          std::find( srcEdges.begin(), srcEdges.end(), srcEdge1.Reversed());
        if ( srcEdge2 == srcEdges.end() || srcEdge3 == srcEdges.end() ) // srcEdge1 is not a seam
        {
          // find srcEdge2 which also will be projected to tgtEdges.front()
          for ( srcEdge2 = srcEdges.begin(); srcEdge2 != srcEdges.end(); ++srcEdge2 )
            if ( !srcEdge1.IsSame( *srcEdge2 ) &&
                 tgtEdges.front().IsSame( shape2ShapeMap( *srcEdge2, /*isSrc=*/true )))
              break;
          // compare nb nodes on srcEdge1 and srcEdge2
          if ( srcEdge2 != srcEdges.end() )
          {
            smIdType nbN1 = 0, nbN2 = 0;
            if ( SMESHDS_SubMesh* sm = srcMesh->GetMeshDS()->MeshElements( srcEdge1 ))
              nbN1 = sm->NbNodes();
            if ( SMESHDS_SubMesh* sm = srcMesh->GetMeshDS()->MeshElements( *srcEdge2 ))
              nbN2 = sm->NbNodes();
            if ( nbN1 != nbN2 )
              srcV1 = helper.IthVertex( 1, srcEdges.front() );
          }
        }
      }
    }
    else if ( nbEdgesInWires.front() == 1 ) // a sole edge in a wire
    {
      TopoDS_Edge srcE1 = srcEdges.front(), tgtE1 = tgtEdges.front();
      for ( size_t iW = 0; iW < srcWires.size(); ++iW )
      {
        StdMeshers_FaceSidePtr srcWire = srcWires[iW];
        for ( int iE = 0; iE < srcWire->NbEdges(); ++iE )
          if ( srcE1.IsSame( srcWire->Edge( iE )))
          {
            reverse = ( tgtE1.Orientation() != tgtWires[iW]->Edge( iE ).Orientation() );
            break;
          }
      }
    }
    else
    {
      RETURN_BAD_RESULT("Bad result from SMESH_Block::GetOrderedEdges()");
    }

    // Load pattern from the source face
    SMESH_Pattern mapper;
    mapper.Load( srcMesh, srcFace, toProjectNodes, srcV1, /*keepNodes=*/true );
    if ( mapper.GetErrorCode() != SMESH_Pattern::ERR_OK )
      return error(COMPERR_BAD_INPUT_MESH,"Can't load mesh pattern from the source face");

    // --------------------
    // Perform 2D mapping
    // --------------------

    // Compute mesh on a target face

    mapper.Apply( tgtFace, tgtV1, reverse );
    if ( mapper.GetErrorCode() != SMESH_Pattern::ERR_OK ) {
      // std::ofstream file("/tmp/Pattern.smp" );
      // mapper.Save( file );
      return error("Can't apply source mesh pattern to the face");
    }

    // Create the mesh

    const bool toCreatePolygons = false, toCreatePolyedrs = false;
    mapper.MakeMesh( tgtMesh, toCreatePolygons, toCreatePolyedrs );
    if ( mapper.GetErrorCode() != SMESH_Pattern::ERR_OK )
      return error("Can't make mesh by source mesh pattern");

    // fill _src2tgtNodes
    std::vector< const SMDS_MeshNode* > *srcNodes, *tgtNodes;
    mapper.GetInOutNodes( srcNodes, tgtNodes );
    size_t nbN = std::min( srcNodes->size(), tgtNodes->size() );
    for ( size_t i = 0; i < nbN; ++i )
      if ( (*srcNodes)[i] && (*tgtNodes)[i] )
        _src2tgtNodes.insert( make_pair( (*srcNodes)[i], (*tgtNodes)[i] ));


  } // end of projection using Pattern mapping

  {
    // -------------------------------------------------------------------------
    // mapper doesn't take care of nodes already existing on edges and vertices,
    // so we must merge nodes created by it with existing ones
    // -------------------------------------------------------------------------

    SMESH_MeshEditor::TListOfListOfNodes groupsOfNodes;

    // Make groups of nodes to merge

    // loop on EDGE and VERTEX sub-meshes of a target FACE
    SMESH_subMeshIteratorPtr smIt = tgtSubMesh->getDependsOnIterator(/*includeSelf=*/false,
                                                                     /*complexShapeFirst=*/false);
    while ( smIt->more() )
    {
      SMESH_subMesh*     sm = smIt->next();
      SMESHDS_SubMesh* smDS = sm->GetSubMeshDS();
      if ( !smDS || smDS->NbNodes() == 0 )
        continue;
      //if ( !is1DComputed && sm->GetSubShape().ShapeType() == TopAbs_EDGE )
      //  break;

      if ( helper.IsDegenShape( sm->GetId() ) ) // to merge all nodes on degenerated
      {
        if ( sm->GetSubShape().ShapeType() == TopAbs_EDGE )
        {
          groupsOfNodes.push_back( list< const SMDS_MeshNode* >() );
          SMESH_subMeshIteratorPtr smDegenIt
            = sm->getDependsOnIterator(/*includeSelf=*/true,/*complexShapeFirst=*/false);
          while ( smDegenIt->more() )
            if (( smDS = smDegenIt->next()->GetSubMeshDS() ))
            {
              SMDS_NodeIteratorPtr nIt = smDS->GetNodes();
              while ( nIt->more() )
                groupsOfNodes.back().push_back( nIt->next() );
            }
        }
        continue; // do not treat sm of degen VERTEX
      }

      // Sort new and old nodes of a sub-mesh separately

      bool isSeam = helper.IsRealSeam( sm->GetId() );

      enum { NEW_NODES = 0, OLD_NODES };
      map< double, const SMDS_MeshNode* > u2nodesMaps[2], u2nodesOnSeam;
      map< double, const SMDS_MeshNode* >::iterator u_oldNode, u_newNode, u_newOnSeam, newEnd;
      set< const SMDS_MeshNode* > seamNodes;

      // mapper changed, no more "mapper puts on a seam edge nodes from 2 edges"
      if ( isSeam && ! getBoundaryNodes ( sm, tgtFace, u2nodesOnSeam, seamNodes ))
      {
        //RETURN_BAD_RESULT("getBoundaryNodes() failed");
      }

      SMDS_NodeIteratorPtr nIt = smDS->GetNodes();
      while ( nIt->more() )
      {
        const SMDS_MeshNode* node = nIt->next();
        bool isOld = isOldNode( node );

        if ( !isOld && isSeam ) { // new node on a seam edge
          if ( seamNodes.count( node ) )
            continue; // node is already in the map
        }

        // sort nodes on edges by their position
        map< double, const SMDS_MeshNode* > & pos2nodes = u2nodesMaps[isOld ? OLD_NODES : NEW_NODES];
        switch ( node->GetPosition()->GetTypeOfPosition() )
        {
        case  SMDS_TOP_VERTEX: {
          if ( !is1DComputed && !pos2nodes.empty() )
            u2nodesMaps[isOld ? NEW_NODES : OLD_NODES].insert( make_pair( 0, node ));
          else
            pos2nodes.insert( make_pair( 0, node ));
          break;
        }
        case  SMDS_TOP_EDGE:   {
          SMDS_EdgePositionPtr pos = node->GetPosition();
          pos2nodes.insert( make_pair( pos->GetUParameter(), node ));
          break;
        }
        default:
          RETURN_BAD_RESULT("Wrong node position type: "<<
                            node->GetPosition()->GetTypeOfPosition());
        }
      }
      const bool mergeNewToOld =
        ( u2nodesMaps[ NEW_NODES ].size() == u2nodesMaps[ OLD_NODES ].size() );
      const bool mergeSeamToNew =
        ( u2nodesMaps[ NEW_NODES ].size() == u2nodesOnSeam.size() );

      if ( !mergeNewToOld )
        if ( u2nodesMaps[ NEW_NODES ].size() > 0 &&
             u2nodesMaps[ OLD_NODES ].size() > 0 )
        {
          u_oldNode = u2nodesMaps[ OLD_NODES ].begin();
          newEnd    = u2nodesMaps[ OLD_NODES ].end();
          for ( ; u_oldNode != newEnd; ++u_oldNode )
            SMESH_Algo::addBadInputElement( u_oldNode->second );
          return error( COMPERR_BAD_INPUT_MESH,
                        SMESH_Comment( "Existing mesh mismatches the projected 2D mesh on " )
                        << ( sm->GetSubShape().ShapeType() == TopAbs_EDGE ? "edge" : "vertex" )
                        << " #" << sm->GetId() );
        }
      if ( isSeam && !mergeSeamToNew ) {
        const TopoDS_Shape& seam = sm->GetSubShape();
        if ( u2nodesMaps[ NEW_NODES ].size() > 0 &&
             u2nodesOnSeam.size()            > 0 &&
             seam.ShapeType() == TopAbs_EDGE )
        {
          int nbE1 = helper.Count( tgtFace, TopAbs_EDGE, /*ignoreSame=*/true );
          int nbE2 = helper.Count( srcFace, TopAbs_EDGE, /*ignoreSame=*/true );
          if ( nbE1 != nbE2 ) // 2 EDGEs are mapped to a seam EDGE
          {
            // find the 2 EDGEs of srcFace
            TopTools_DataMapIteratorOfDataMapOfShapeShape src2tgtIt( shape2ShapeMap._map2to1 );
            for ( ; src2tgtIt.More(); src2tgtIt.Next() )
              if ( seam.IsSame( src2tgtIt.Value() ))
                SMESH_Algo::addBadInputElements
                  ( srcMesh->GetMeshDS()->MeshElements( src2tgtIt.Key() ));
            return error( COMPERR_BAD_INPUT_MESH,
                          "Different number of nodes on two edges projected to a seam edge" );
          }
        }
      }

      // Make groups of nodes to merge

      u_oldNode = u2nodesMaps[ OLD_NODES ].begin();
      u_newNode = u2nodesMaps[ NEW_NODES ].begin();
      newEnd    = u2nodesMaps[ NEW_NODES ].end();
      u_newOnSeam = u2nodesOnSeam.begin();
      if ( mergeNewToOld )
        for ( ; u_newNode != newEnd; ++u_newNode, ++u_oldNode )
        {
          groupsOfNodes.push_back( list< const SMDS_MeshNode* >() );
          groupsOfNodes.back().push_back( u_oldNode->second );
          groupsOfNodes.back().push_back( u_newNode->second );
          if ( mergeSeamToNew )
            groupsOfNodes.back().push_back( (u_newOnSeam++)->second );
        }
      else if ( mergeSeamToNew )
        for ( ; u_newNode != newEnd; ++u_newNode, ++u_newOnSeam )
        {
          groupsOfNodes.push_back( list< const SMDS_MeshNode* >() );
          groupsOfNodes.back().push_back( u_newNode->second );
          groupsOfNodes.back().push_back( u_newOnSeam->second );
        }

    } // loop on EDGE and VERTEX submeshes of a target FACE

    // Merge

    SMESH_MeshEditor editor( tgtMesh );
    smIdType nbFaceBeforeMerge = tgtSubMesh->GetSubMeshDS()->NbElements();
    editor.MergeNodes( groupsOfNodes );
    smIdType nbFaceAtferMerge = tgtSubMesh->GetSubMeshDS()->NbElements();
    if ( nbFaceBeforeMerge != nbFaceAtferMerge && !helper.HasDegeneratedEdges() )
      return error(COMPERR_BAD_INPUT_MESH, "Probably invalid node parameters on geom faces");

    // ----------------------------------------------------------------
    // The mapper can't create quadratic elements, so convert if needed
    // ----------------------------------------------------------------

    SMDS_ElemIteratorPtr faceIt;
    faceIt         = srcSubMesh->GetSubMeshDS()->GetElements();
    bool srcIsQuad = faceIt->next()->IsQuadratic();
    faceIt         = tgtSubMesh->GetSubMeshDS()->GetElements();
    bool tgtIsQuad = faceIt->next()->IsQuadratic();
    if ( srcIsQuad && !tgtIsQuad )
    {
      TIDSortedElemSet tgtFaces;
      faceIt = tgtSubMesh->GetSubMeshDS()->GetElements();
      while ( faceIt->more() )
        tgtFaces.insert( tgtFaces.end(), faceIt->next() );

      editor.ConvertToQuadratic(/*theForce3d=*/false, tgtFaces, false);
    }
  } // end of coincident nodes and quadratic elements treatment


  if ( !projDone || is1DComputed )
    // ----------------------------------------------------------------
    // The mapper can create distorted faces by placing nodes out of the FACE
    // boundary, also bad faces can be created if EDGEs already discretized
    // --> fix bad faces by smoothing
    // ----------------------------------------------------------------
    if ( helper.IsDistorted2D( tgtSubMesh, /*checkUV=*/false, &helper ))
    {
      TAssocTool::Morph morph( srcWires );
      morph.Perform( helper, tgtWires, helper.GetSurface( tgtFace ),
                     _src2tgtNodes, /*moveAll=*/true );
      if(SALOME::VerbosityActivated())
        cout << "StdMeshers_Projection_2D: Projection mesh IsDistorted2D() ==> do morph" << endl;

      if ( !fixDistortedFaces( helper, tgtWires )) // smooth and check
        return error("Invalid mesh generated");
    }
  // ---------------------------
  // Check elements orientation
  // ---------------------------

  TopoDS_Face face = TopoDS::Face( theShape );
  if ( !theMesh.IsMainShape( tgtFace ))
  {
    // find the main shape
    TopoDS_Shape mainShape = meshDS->ShapeToMesh();
    switch ( mainShape.ShapeType() ) {
    case TopAbs_SHELL:
    case TopAbs_SOLID: break;
    default:
      TopTools_ListIteratorOfListOfShape ancestIt = theMesh.GetAncestors( face );
      for ( ; ancestIt.More(); ancestIt.Next() ) {
        TopAbs_ShapeEnum type = ancestIt.Value().ShapeType();
        if ( type == TopAbs_SOLID ) {
          mainShape = ancestIt.Value();
          break;
        } else if ( type == TopAbs_SHELL ) {
          mainShape = ancestIt.Value();
        }
      }
    }
    // find tgtFace in the main solid or shell to know it's true orientation.
    TopExp_Explorer exp( mainShape, TopAbs_FACE );
    for ( ; exp.More(); exp.Next() ) {
      if ( tgtFace.IsSame( exp.Current() )) {
        face = TopoDS::Face( exp.Current() );
        break;
      }
    }
  }
  // Fix orientation
  if ( helper.IsReversedSubMesh( face ))
  {
    SMESH_MeshEditor editor( tgtMesh );
    SMDS_ElemIteratorPtr eIt = meshDS->MeshElements( face )->GetElements();
    while ( eIt->more() ) {
      const SMDS_MeshElement* e = eIt->next();
      if ( e->GetType() == SMDSAbs_Face && !editor.Reorient( e ))
        RETURN_BAD_RESULT("Pb of SMESH_MeshEditor::Reorient()");
    }
  }

  cleaner.Release(); // not to remove mesh

  return true;
}


//=======================================================================
//function : Evaluate
//purpose  :
//=======================================================================

bool StdMeshers_Projection_2D::Evaluate(SMESH_Mesh&         theMesh,
                                        const TopoDS_Shape& theShape,
                                        MapShapeNbElems&    aResMap)
{
  if ( !_sourceHypo )
    return false;

  SMESH_Mesh * srcMesh = _sourceHypo->GetSourceMesh();
  SMESH_Mesh * tgtMesh = & theMesh;
  if ( !srcMesh )
    srcMesh = tgtMesh;

  // ---------------------------
  // Make sub-shapes association
  // ---------------------------

  TopoDS_Face tgtFace = TopoDS::Face( theShape.Oriented(TopAbs_FORWARD));
  TopoDS_Shape srcShape = _sourceHypo->GetSourceFace().Oriented(TopAbs_FORWARD);

  TAssocTool::TShapeShapeMap shape2ShapeMap;
  TAssocTool::InitVertexAssociation( _sourceHypo, shape2ShapeMap );
  if ( !TAssocTool::FindSubShapeAssociation( tgtFace, tgtMesh, srcShape, srcMesh,
                                             shape2ShapeMap)  ||
       !shape2ShapeMap.IsBound( tgtFace ))
    return error(COMPERR_BAD_SHAPE,"Topology of source and target faces seems different" );

  TopoDS_Face srcFace = TopoDS::Face( shape2ShapeMap( tgtFace ).Oriented(TopAbs_FORWARD));

  // -------------------------------------------------------
  // Assure that mesh on a source Face is computed/evaluated
  // -------------------------------------------------------

  std::vector<smIdType> aVec;

  SMESH_subMesh* srcSubMesh = srcMesh->GetSubMesh( srcFace );
  if ( srcSubMesh->IsMeshComputed() )
  {
    aVec.resize( SMDSEntity_Last, 0 );
    aVec[SMDSEntity_Node] = srcSubMesh->GetSubMeshDS()->NbNodes();

    SMDS_ElemIteratorPtr elemIt = srcSubMesh->GetSubMeshDS()->GetElements();
    while ( elemIt->more() )
      aVec[ elemIt->next()->GetEntityType() ]++;
  }
  else
  {
    MapShapeNbElems  tmpResMap;
    MapShapeNbElems& srcResMap = (srcMesh == tgtMesh) ? aResMap : tmpResMap;
    if ( !_gen->Evaluate( *srcMesh, srcShape, srcResMap ))
      return error(COMPERR_BAD_INPUT_MESH,"Source mesh not evaluatable");
    aVec = srcResMap[ srcSubMesh ];
    if ( aVec.empty() )
      return error(COMPERR_BAD_INPUT_MESH,"Source mesh is wrongly evaluated");
  }

  SMESH_subMesh * sm = theMesh.GetSubMesh(theShape);
  aResMap.insert(std::make_pair(sm,aVec));

  return true;
}


//=============================================================================
/*!
 * \brief Sets a default event listener to submesh of the source face
  * \param subMesh - submesh where algo is set
 *
 * This method is called when a submesh gets HYP_OK algo_state.
 * After being set, event listener is notified on each event of a submesh.
 * Arranges that CLEAN event is translated from source submesh to
 * the submesh
 */
//=============================================================================

void StdMeshers_Projection_2D::SetEventListener(SMESH_subMesh* subMesh)
{
  TAssocTool::SetEventListener( subMesh,
                                _sourceHypo->GetSourceFace(),
                                _sourceHypo->GetSourceMesh() );
}
