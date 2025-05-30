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
//  File   : SMESH_Gen.cxx
//  Author : Paul RASCLE, EDF
//  Module : SMESH
//
//#define CHRONODEF
//
#include "SMESH_Gen.hxx"

#include "SMESH_DriverMesh.hxx"
#include "SMDS_Mesh.hxx"
#include "SMDS_MeshElement.hxx"
#include "SMDS_MeshNode.hxx"
#include "SMESHDS_Document.hxx"
#include "SMESH_HypoFilter.hxx"
#include "SMESH_Mesh.hxx"
#include "SMESH_SequentialMesh.hxx"
#include "SMESH_ParallelMesh.hxx"
#include "SMESH_MesherHelper.hxx"
#include "SMESH_subMesh.hxx"

#include <utilities.h>
#include <Utils_ExceptHandlers.hxx>

#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Iterator.hxx>

#include "memoire.h"
#include <functional>

#include <QString>
#include <QProcess>

#ifdef WIN32
  #include <windows.h>
#endif

#include <Basics_Utils.hxx>

#ifndef WIN32
#include <boost/asio.hpp>
#endif

using namespace std;
#ifndef WIN32
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#endif

// Environment variable separator
#ifdef WIN32
  #define env_sep ';'
#else
  #define env_sep ':'
#endif

namespace
{
  // a structure used to nullify SMESH_Gen field of SMESH_Hypothesis,
  // which is needed for SMESH_Hypothesis not deleted before ~SMESH_Gen()
  struct _Hyp : public SMESH_Hypothesis
  {
    void NullifyGen()
    {
      _gen = 0;
    }
  };

  //================================================================================
  /*!
   * \brief Fill a map of shapes with all sub-shape of a sub-mesh
   */
  //================================================================================

  TopTools_IndexedMapOfShape * fillAllowed( SMESH_subMesh*               sm,
                                            const bool                   toFill,
                                            TopTools_IndexedMapOfShape * allowedSub )
  {
    if ( !toFill || !allowedSub )
    {
      return nullptr;
    }
    if ( allowedSub->IsEmpty() )
    {
      allowedSub->ReSize( sm->DependsOn().size() + 1 );
      allowedSub->Add( sm->GetSubShape() );
      for ( const auto & key_sm : sm->DependsOn() )
        allowedSub->Add( key_sm.second->GetSubShape() );
    }
    return allowedSub;
  }
}

//=============================================================================
/*!
 *  Constructor
 */
//=============================================================================

SMESH_Gen::SMESH_Gen()
{
  _studyContext = new StudyContextStruct;
  _studyContext->myDocument = new SMESHDS_Document();
  _localId = 0;
  _hypId   = 0;
  _segmentation = _nbSegments = 10;
  _compute_canceled = false;
}

//=============================================================================
/*!
 * Destructor
 */
//=============================================================================

SMESH_Gen::~SMESH_Gen()
{
  std::map < int, SMESH_Hypothesis * >::iterator i_hyp = _studyContext->mapHypothesis.begin();
  for ( ; i_hyp != _studyContext->mapHypothesis.end(); ++i_hyp )
  {
    if ( _Hyp* h = static_cast< _Hyp*>( i_hyp->second ))
      h->NullifyGen();
  }
  delete _studyContext->myDocument;
  delete _studyContext;
}

//=============================================================================
/*!
 * Creates a mesh in a study.
 * if (theIsEmbeddedMode) { mesh modification commands are not logged }
 */
//=============================================================================

SMESH_Mesh* SMESH_Gen::CreateMesh(bool theIsEmbeddedMode)
{
  Unexpect aCatch(SalomeException);

  // create a new SMESH_mesh object
  SMESH_Mesh *aMesh = new SMESH_SequentialMesh(
                                     _localId++,
                                     this,
                                     theIsEmbeddedMode,
                                     _studyContext->myDocument);
  _studyContext->mapMesh[_localId-1] = aMesh;

  return aMesh;
}

//=============================================================================
/*!
 * Creates a parallel mesh in a study.
 * if (theIsEmbeddedMode) { mesh modification commands are not logged }
 */
//=============================================================================

SMESH_ParallelMesh* SMESH_Gen::CreateParallelMesh(bool theIsEmbeddedMode)
{
  Unexpect aCatch(SalomeException);

  // create a new SMESH_mesh object
  SMESH_ParallelMesh *aMesh = new SMESH_ParallelMesh(
                                     _localId++,
                                     this,
                                     theIsEmbeddedMode,
                                     _studyContext->myDocument);
  _studyContext->mapMesh[_localId-1] = aMesh;

  return aMesh;
}

//=============================================================================
/*!
 * Algo to run the computation of all the submeshes of a mesh in sequentila
 */
//=============================================================================

bool SMESH_Gen::sequentialComputeSubMeshes(
          SMESH_Mesh & aMesh,
          const TopoDS_Shape & aShape,
          const ::MeshDimension       aDim,
          TSetOfInt*                  aShapesId /*=0*/,
          TopTools_IndexedMapOfShape* allowedSubShapes,
          SMESH_subMesh::compute_event &computeEvent,
          const bool includeSelf,
          const bool complexShapeFirst,
          const bool   aShapeOnly)
{
  MESSAGE("Sequential Compute of submeshes");

  bool ret = true;

  SMESH_subMeshIteratorPtr smIt;
  SMESH_subMesh *shapeSM = aMesh.GetSubMesh(aShape);

  smIt = shapeSM->getDependsOnIterator(includeSelf, !complexShapeFirst);
  while ( smIt->more() )
  {
    SMESH_subMesh* smToCompute = smIt->next();

    // do not mesh vertices of a pseudo shape
    const TopoDS_Shape&        shape = smToCompute->GetSubShape();
    const TopAbs_ShapeEnum shapeType = shape.ShapeType();
    if ( !aMesh.HasShapeToMesh() && shapeType == TopAbs_VERTEX )
      continue;

    // check for preview dimension limitations
    if ( aShapesId && SMESH_Gen::GetShapeDim( shapeType ) > (int)aDim )
    {
      // clear compute state not to show previous compute errors
      //  if preview invoked less dimension less than previous
      smToCompute->ComputeStateEngine( SMESH_subMesh::CHECK_COMPUTE_STATE );
      continue;
    }

    if (smToCompute->GetComputeState() == SMESH_subMesh::READY_TO_COMPUTE)
    {
      if (_compute_canceled)
        return false;
      smToCompute->SetAllowedSubShapes( fillAllowed( shapeSM, aShapeOnly, allowedSubShapes ));
      setCurrentSubMesh( smToCompute );
      smToCompute->ComputeStateEngine( computeEvent );
      setCurrentSubMesh( nullptr );
      smToCompute->SetAllowedSubShapes( nullptr );
    }

    // we check all the sub-meshes here and detect if any of them failed to compute
    if (smToCompute->GetComputeState() == SMESH_subMesh::FAILED_TO_COMPUTE &&
        ( shapeType != TopAbs_EDGE || !SMESH_Algo::isDegenerated( TopoDS::Edge( shape ))))
      ret = false;
    else if ( aShapesId )
      aShapesId->insert( smToCompute->GetId() );
  }
  //aMesh.GetMeshDS()->Modified();
  return ret;

};

//=============================================================================
/*
 * compute of a submesh
 * This function is passed to the thread pool
 */
//=============================================================================
const std::function<void(SMESH_subMesh*,
                         SMESH_subMesh::compute_event,
                         SMESH_subMesh*,
                         bool,
                         TopTools_IndexedMapOfShape *,
                         TSetOfInt*)>
     compute_function([] (SMESH_subMesh* sm,
                          SMESH_subMesh::compute_event event,
                          SMESH_subMesh *shapeSM,
                          bool aShapeOnly,
                          TopTools_IndexedMapOfShape *allowedSubShapes,
                          TSetOfInt*                  aShapesId) -> void
{
  if (sm->GetComputeState() == SMESH_subMesh::READY_TO_COMPUTE)
  {
    sm->SetAllowedSubShapes( fillAllowed( shapeSM, aShapeOnly, allowedSubShapes ));
    //setCurrentSubMesh( sm );
    sm->ComputeStateEngine(event);
    //setCurrentSubMesh( nullptr );
    sm->SetAllowedSubShapes( nullptr );
  }

  if ( aShapesId )
    aShapesId->insert( sm->GetId() );

});


//=============================================================================
/*
 * Copy a file on remote resource
 */
//=============================================================================

void SMESH_Gen::send_mesh(SMESH_Mesh& aMesh, std::string file_name)
{
#ifndef WIN32
  SMESH_ParallelMesh& aParMesh = dynamic_cast<SMESH_ParallelMesh&>(aMesh);
  // Calling run_mesher
  // Path to mesher script
  fs::path send_files = fs::path(std::getenv("SMESH_ROOT_DIR"))/
  fs::path("bin")/
  fs::path("salome")/
  fs::path("send_files.py");

  std::string s_program="python3";
  std::list<std::string> params;
  params.push_back(send_files.string());
  params.push_back(file_name);
  params.push_back("--resource="+aParMesh.GetResource());

  // log file
  fs::path log_file=aParMesh.GetTmpFolder() / fs::path("copy.log");
  QString out_file = log_file.string().c_str();

  // Building arguments for QProcess
  QString program = QString::fromStdString(s_program);
  QStringList arguments;
  for(auto arg : params){
    arguments << arg.c_str();
  }

  std::string cmd = "";
  cmd += s_program;
  for(auto arg: params){
    cmd += " " + arg;
  }
  MESSAGE("Send files command: ");
  MESSAGE(cmd);

  QProcess myProcess;
  myProcess.setProcessChannelMode(QProcess::MergedChannels);
  myProcess.setStandardOutputFile(out_file);

  myProcess.start(program, arguments);
  // Waiting for process to finish (argument -1 make it wait until the end of
  // the process otherwise it just waits 30 seconds)
  bool finished = myProcess.waitForFinished(-1);
  int ret = myProcess.exitCode();

  if(ret != 0 || !finished){
  // Run crashed
    std::string msg = "Issue with send_files: \n";
    msg += "See log for more details: " + log_file.string() + "\n";
    msg += cmd + "\n";
    throw SALOME_Exception(msg);
  }
#endif
}

//=============================================================================
/*!
 * Algo to run the computation of all the submeshes of a mesh in parallel
 */
//=============================================================================

bool SMESH_Gen::parallelComputeSubMeshes(
          SMESH_Mesh & aMesh,
          const TopoDS_Shape & aShape,
          const ::MeshDimension       aDim,
          TSetOfInt*                  aShapesId /*=0*/,
          TopTools_IndexedMapOfShape* allowedSubShapes,
          SMESH_subMesh::compute_event &computeEvent,
          const bool includeSelf,
          const bool complexShapeFirst,
          const bool   aShapeOnly)
{
#ifdef WIN32
  throw SALOME_Exception("ParallelMesh is not working on Windows");
#else

  bool ret = true;

  SMESH_subMeshIteratorPtr smIt;
  SMESH_subMesh *shapeSM = aMesh.GetSubMesh(aShape);
  SMESH_ParallelMesh &aParMesh = dynamic_cast<SMESH_ParallelMesh&>(aMesh);

  TopAbs_ShapeEnum previousShapeType = TopAbs_VERTEX;
  MESSAGE("Parallel Compute of submeshes");


  smIt = shapeSM->getDependsOnIterator(includeSelf, !complexShapeFirst);
  while ( smIt->more() )
  {
    SMESH_subMesh* smToCompute = smIt->next();

    // do not mesh vertices of a pseudo shape
    const TopoDS_Shape&        shape = smToCompute->GetSubShape();
    const TopAbs_ShapeEnum shapeType = shape.ShapeType();
    // Not doing in parallel 1D meshes
    if ( !aMesh.HasShapeToMesh() && shapeType == TopAbs_VERTEX )
      continue;

    if (shapeType != previousShapeType) {
      // Waiting for all threads for the previous type to end
      aMesh.wait();

      std::string file_name="";
      if (previousShapeType == aParMesh.GetDumpElement())
        file_name = "Mesh"+std::to_string(aParMesh.GetParallelismDimension()-1)+"D.med";

      if(file_name != "")
      {
        fs::path mesh_file = fs::path(aParMesh.GetTmpFolder()) / fs::path(file_name);
	      SMESH_DriverMesh::exportMesh(mesh_file.string(), aMesh, "MESH");
        if (aParMesh.GetParallelismMethod() == ParallelismMethod::MultiNode) {
          this->send_mesh(aMesh, mesh_file.string());
        }
      }
      //Resetting threaded pool info
      previousShapeType = shapeType;
    }

    // check for preview dimension limitations
    if ( aShapesId && SMESH_Gen::GetShapeDim( shapeType ) > (int)aDim )
    {
      // clear compute state not to show previous compute errors
      //  if preview invoked less dimension less than previous
      smToCompute->ComputeStateEngine( SMESH_subMesh::CHECK_COMPUTE_STATE );
      continue;
    }
    // Parallelism is only for 3D parts
    if(shapeType!=aMesh.GetParallelElement()){
      compute_function(smToCompute, computeEvent,
                      shapeSM, aShapeOnly, allowedSubShapes,
                      aShapesId);
    }else{
      boost::asio::post(*(aParMesh.GetPool()), std::bind(compute_function, smToCompute, computeEvent,
                        shapeSM, aShapeOnly, allowedSubShapes,
                        aShapesId));
    }
  }

  // Waiting for the thread for Solids to finish
  aMesh.wait();

  aMesh.GetMeshDS()->Modified();

  // Cleanup done here as in Python the destructor is not called
  aParMesh.cleanup();

  return ret;
#endif
};

//=============================================================================
/*
 * Compute a mesh
 */
//=============================================================================

bool SMESH_Gen::Compute(SMESH_Mesh &                aMesh,
                        const TopoDS_Shape &        aShape,
                        const int                   aFlags /*= COMPACT_MESH*/,
                        const ::MeshDimension       aDim /*=::MeshDim_3D*/,
                        TSetOfInt*                  aShapesId /*=0*/,
                        TopTools_IndexedMapOfShape* anAllowedSubShapes/*=0*/)
{
  MEMOSTAT;

  const bool   aShapeOnly = aFlags & SHAPE_ONLY;
  const bool     anUpward = aFlags & UPWARD;
  const bool aCompactMesh = aFlags & COMPACT_MESH;

  bool ret = true;

  SMESH_subMesh *sm, *shapeSM = aMesh.GetSubMesh(aShape);

  const bool includeSelf = true;
  const bool complexShapeFirst = true;
  const int  globalAlgoDim = 100;

  SMESH_subMeshIteratorPtr smIt;

  // Fix of Issue 22150. Due to !BLSURF->OnlyUnaryInput(), BLSURF computes edges
  // that must be computed by Projection 1D-2D while the Projection asks to compute
  // one face only.
  SMESH_subMesh::compute_event computeEvent =
    aShapeOnly ? SMESH_subMesh::COMPUTE_SUBMESH : SMESH_subMesh::COMPUTE;
  if ( !aMesh.HasShapeToMesh() )
    computeEvent = SMESH_subMesh::COMPUTE_NOGEOM; // if several algos and no geometry

  TopTools_IndexedMapOfShape *allowedSubShapes = anAllowedSubShapes, allowedSub;
  if ( aShapeOnly && !allowedSubShapes )
    allowedSubShapes = &allowedSub;

  if ( anUpward ) // is called from the below code in this method
  {
    // ===============================================
    // Mesh all the sub-shapes starting from vertices
    // ===============================================
    ret = aMesh.ComputeSubMeshes(
            this,
            aMesh, aShape, aDim,
            aShapesId, allowedSubShapes,
            computeEvent,
            includeSelf,
            complexShapeFirst,
            aShapeOnly);

    return ret;
  }
  else
  {
    // ================================================================
    // Apply algos that do NOT require discretized boundaries
    // ("all-dimensional") and do NOT support sub-meshes, starting from
    // the most complex shapes and collect sub-meshes with algos that
    // DO support sub-meshes
    // ================================================================
    list< SMESH_subMesh* > smWithAlgoSupportingSubmeshes[4]; // for each dim

    // map to sort sm with same dim algos according to dim of
    // the shape the algo assigned to (issue 0021217).
    // Other issues influenced the algo applying order:
    // 21406, 21556, 21893, 20206
    multimap< int, SMESH_subMesh* > shDim2sm;
    multimap< int, SMESH_subMesh* >::reverse_iterator shDim2smIt;
    TopoDS_Shape algoShape;
    int prevShapeDim = -1, aShapeDim;

    smIt = shapeSM->getDependsOnIterator(includeSelf, complexShapeFirst);
    while ( smIt->more() )
    {
      SMESH_subMesh* smToCompute = smIt->next();
      if ( smToCompute->GetComputeState() != SMESH_subMesh::READY_TO_COMPUTE )
        continue;

      const TopoDS_Shape& aSubShape = smToCompute->GetSubShape();
      aShapeDim = GetShapeDim( aSubShape );
      if ( aShapeDim < 1 ) break;

      // check for preview dimension limitations
      if ( aShapesId && aShapeDim > (int)aDim )
        continue;

      SMESH_Algo* algo = GetAlgo( smToCompute, &algoShape );
      if ( algo && !algo->NeedDiscreteBoundary() )
      {
        if ( algo->SupportSubmeshes() )
        {
          // reload sub-meshes from shDim2sm into smWithAlgoSupportingSubmeshes
          // so that more local algos to go first
          if ( prevShapeDim != aShapeDim )
          {
            prevShapeDim = aShapeDim;
            for ( shDim2smIt = shDim2sm.rbegin(); shDim2smIt != shDim2sm.rend(); ++shDim2smIt )
              if ( shDim2smIt->first == globalAlgoDim )
                smWithAlgoSupportingSubmeshes[ aShapeDim ].push_back( shDim2smIt->second );
              else
                smWithAlgoSupportingSubmeshes[ aShapeDim ].push_front( shDim2smIt->second );
            shDim2sm.clear();
          }
          // add smToCompute to shDim2sm map
          if ( algoShape.IsSame( aMesh.GetShapeToMesh() ))
          {
            aShapeDim = globalAlgoDim; // to compute last
          }
          else
          {
            aShapeDim = GetShapeDim( algoShape );
            if ( algoShape.ShapeType() == TopAbs_COMPOUND )
            {
              TopoDS_Iterator it( algoShape );
              aShapeDim += GetShapeDim( it.Value() );
            }
          }
          shDim2sm.insert( make_pair( aShapeDim, smToCompute ));
        }
        else // Compute w/o support of sub-meshes
        {
          if (_compute_canceled)
            return false;
          smToCompute->SetAllowedSubShapes( fillAllowed( shapeSM, aShapeOnly, allowedSubShapes ));
          setCurrentSubMesh( smToCompute );
          smToCompute->ComputeStateEngine( computeEvent );
          setCurrentSubMesh( nullptr );
          smToCompute->SetAllowedSubShapes( nullptr );
          if ( aShapesId )
            aShapesId->insert( smToCompute->GetId() );
        }
      }
    }
    // reload sub-meshes from shDim2sm into smWithAlgoSupportingSubmeshes
    for ( shDim2smIt = shDim2sm.rbegin(); shDim2smIt != shDim2sm.rend(); ++shDim2smIt )
      if ( shDim2smIt->first == globalAlgoDim )
        smWithAlgoSupportingSubmeshes[3].push_back( shDim2smIt->second );
      else
        smWithAlgoSupportingSubmeshes[0].push_front( shDim2smIt->second );

    // ======================================================
    // Apply all-dimensional algorithms supporting sub-meshes
    // ======================================================

    std::vector< SMESH_subMesh* > smVec;
    for ( aShapeDim = 0; aShapeDim < 4; ++aShapeDim )
      smVec.insert( smVec.end(),
                    smWithAlgoSupportingSubmeshes[aShapeDim].begin(),
                    smWithAlgoSupportingSubmeshes[aShapeDim].end() );

    // gather sub-shapes with local uni-dimensional algos (bos #29143)
    // ----------------------------------------------------------------
    TopTools_MapOfShape uniDimAlgoShapes;
    if ( !smVec.empty() )
    {
      ShapeToHypothesis::Iterator s2hyps( aMesh.GetMeshDS()->GetHypotheses() );
      for ( ; s2hyps.More(); s2hyps.Next() )
      {
        const TopoDS_Shape& s = s2hyps.Key();
        if ( s.IsSame( aMesh.GetShapeToMesh() ))
          continue;
        for ( auto & hyp : s2hyps.Value() )
        {
          if ( const SMESH_Algo* algo = dynamic_cast< const SMESH_Algo*>( hyp ))
            if ( algo->NeedDiscreteBoundary() )
            {
              TopAbs_ShapeEnum sType;
              switch ( algo->GetDim() ) {
              case 3:  sType = TopAbs_SOLID; break;
              case 2:  sType = TopAbs_FACE; break;
              default: sType = TopAbs_EDGE; break;
              }
              for ( TopExp_Explorer ex( s2hyps.Key(), sType ); ex.More(); ex.Next() )
                uniDimAlgoShapes.Add( ex.Current() );
            }
        }
      }
    }

    {
      // ------------------------------------------------
      // sort list of sub-meshes according to mesh order
      // ------------------------------------------------
      aMesh.SortByMeshOrder( smVec );

      // ------------------------------------------------------------
      // compute sub-meshes with local uni-dimensional algos under
      // sub-meshes with all-dimensional algos
      // ------------------------------------------------------------
      // start from lower shapes
      for ( size_t i = 0; i < smVec.size(); ++i )
      {
        sm = smVec[i];
        if ( sm->GetComputeState() != SMESH_subMesh::READY_TO_COMPUTE)
          continue;

        const TopAbs_ShapeEnum shapeType = sm->GetSubShape().ShapeType();

        if ( !uniDimAlgoShapes.IsEmpty() )
        {
          // get a shape the algo is assigned to
          if ( !GetAlgo( sm, & algoShape ))
            continue; // strange...

          // look for more local algos
          if ( SMESH_subMesh* algoSM = aMesh.GetSubMesh( algoShape ))
            smIt = algoSM->getDependsOnIterator(!includeSelf, !complexShapeFirst);
          else
            smIt = sm->getDependsOnIterator(!includeSelf, !complexShapeFirst);

          while ( smIt->more() )
          {
            SMESH_subMesh* smToCompute = smIt->next();

            const TopoDS_Shape& aSubShape = smToCompute->GetSubShape();
            const           int aShapeDim = GetShapeDim( aSubShape );
            if ( aShapeDim < 1 || aSubShape.ShapeType() <= shapeType )
              continue;
            if ( !uniDimAlgoShapes.Contains( aSubShape ))
              continue; // [bos #29143] aMesh.GetHypothesis() is too long

            // check for preview dimension limitations
            if ( aShapesId && GetShapeDim( aSubShape.ShapeType() ) > (int)aDim )
              continue;

            SMESH_HypoFilter filter( SMESH_HypoFilter::IsAlgo() );
            filter
              .And( SMESH_HypoFilter::IsApplicableTo( aSubShape ))
              .And( SMESH_HypoFilter::IsMoreLocalThan( algoShape, aMesh ));

            if ( SMESH_Algo* subAlgo = (SMESH_Algo*) aMesh.GetHypothesis( smToCompute, filter, true))
            {
              if ( ! subAlgo->NeedDiscreteBoundary() ) continue;
              TopTools_IndexedMapOfShape* localAllowed = allowedSubShapes;
              if ( localAllowed && localAllowed->IsEmpty() )
                localAllowed = 0; // prevent fillAllowed() with  aSubShape

              SMESH_Hypothesis::Hypothesis_Status status;
              if ( subAlgo->CheckHypothesis( aMesh, aSubShape, status ))
                // mesh a lower smToCompute starting from vertices
                Compute( aMesh, aSubShape, aFlags | SHAPE_ONLY_UPWARD, aDim, aShapesId, localAllowed );
            }
          }
        }
        // --------------------------------
        // apply the all-dimensional algo
        // --------------------------------
        {
          if (_compute_canceled)
            return false;
          // check for preview dimension limitations
          if ( aShapesId && GetShapeDim( shapeType ) > (int)aDim )
            continue;
          sm->SetAllowedSubShapes( fillAllowed( shapeSM, aShapeOnly, allowedSubShapes ));

          setCurrentSubMesh( sm );
          sm->ComputeStateEngine( computeEvent );

          setCurrentSubMesh( NULL );
          sm->SetAllowedSubShapes( nullptr );
          if ( aShapesId )
            aShapesId->insert( sm->GetId() );
        }
      }
    }

    // -----------------------------------------------
    // mesh the rest sub-shapes starting from vertices
    // -----------------------------------------------
    ret = Compute( aMesh, aShape, aFlags | UPWARD, aDim, aShapesId, allowedSubShapes );

  }

  MEMOSTAT;

  // fix quadratic mesh by bending iternal links near concave boundary
  if ( aCompactMesh && // a final compute
       aShape.IsSame( aMesh.GetShapeToMesh() ) &&
       !aShapesId && // not preview
       ret ) // everything is OK
  {
    SMESH_MesherHelper aHelper( aMesh );
    if ( aHelper.IsQuadraticMesh() != SMESH_MesherHelper::LINEAR )
    {
      aHelper.FixQuadraticElements( shapeSM->GetComputeError() );
    }
  }

  if ( aCompactMesh )
  {
    aMesh.GetMeshDS()->Modified();
    aMesh.GetMeshDS()->CompactMesh();
  }
  return ret;
}

//=============================================================================
/*!
 * Prepare Compute a mesh
 */
//=============================================================================
void SMESH_Gen::PrepareCompute(SMESH_Mesh &          /*aMesh*/,
                               const TopoDS_Shape &  /*aShape*/)
{
  _compute_canceled = false;
  resetCurrentSubMesh();
}

//=============================================================================
/*!
 * Cancel Compute a mesh
 */
//=============================================================================
void SMESH_Gen::CancelCompute(SMESH_Mesh &          /*aMesh*/,
                              const TopoDS_Shape &  /*aShape*/)
{
  _compute_canceled = true;
  if ( const SMESH_subMesh* sm = GetCurrentSubMesh() )
  {
    const_cast< SMESH_subMesh* >( sm )->ComputeStateEngine( SMESH_subMesh::COMPUTE_CANCELED );
  }
  resetCurrentSubMesh();
}

//================================================================================
/*!
 * \brief Returns a sub-mesh being currently computed
 */
//================================================================================

const SMESH_subMesh* SMESH_Gen::GetCurrentSubMesh() const
{
  return _sm_current.empty() ? 0 : _sm_current.back();
}

//================================================================================
/*!
 * \brief Sets a sub-mesh being currently computed.
 *
 * An algorithm can call Compute() for a sub-shape, hence we keep a stack of sub-meshes
 */
//================================================================================

void SMESH_Gen::setCurrentSubMesh(SMESH_subMesh* sm)
{
  if ( sm )
    _sm_current.push_back( sm );

  else if ( !_sm_current.empty() )
    _sm_current.pop_back();
}

void SMESH_Gen::resetCurrentSubMesh()
{
  _sm_current.clear();
}

//=============================================================================
/*!
 * Evaluate a mesh
 */
//=============================================================================

bool SMESH_Gen::Evaluate(SMESH_Mesh &          aMesh,
                         const TopoDS_Shape &  aShape,
                         MapShapeNbElems&      aResMap,
                         const bool            anUpward,
                         TSetOfInt*            aShapesId)
{
  bool ret = true;

  SMESH_subMesh *sm = aMesh.GetSubMesh(aShape);

  const bool includeSelf = true;
  const bool complexShapeFirst = true;
  SMESH_subMeshIteratorPtr smIt;

  if ( anUpward ) { // is called from below code here
    // -----------------------------------------------
    // mesh all the sub-shapes starting from vertices
    // -----------------------------------------------
    smIt = sm->getDependsOnIterator(includeSelf, !complexShapeFirst);
    while ( smIt->more() ) {
      SMESH_subMesh* smToCompute = smIt->next();

      // do not mesh vertices of a pseudo shape
      const TopAbs_ShapeEnum shapeType = smToCompute->GetSubShape().ShapeType();
      //if ( !aMesh.HasShapeToMesh() && shapeType == TopAbs_VERTEX )
      //  continue;
      if ( !aMesh.HasShapeToMesh() ) {
        if( shapeType == TopAbs_VERTEX || shapeType == TopAbs_WIRE ||
            shapeType == TopAbs_SHELL )
          continue;
      }

      smToCompute->Evaluate(aResMap);
      if( aShapesId )
        aShapesId->insert( smToCompute->GetId() );
    }
    return ret;
  }
  else {
    // -----------------------------------------------------------------
    // apply algos that DO NOT require Discreteized boundaries and DO NOT
    // support sub-meshes, starting from the most complex shapes
    // and collect sub-meshes with algos that DO support sub-meshes
    // -----------------------------------------------------------------
    list< SMESH_subMesh* > smWithAlgoSupportingSubmeshes;
    smIt = sm->getDependsOnIterator(includeSelf, complexShapeFirst);
    while ( smIt->more() ) {
      SMESH_subMesh* smToCompute = smIt->next();
      const TopoDS_Shape& aSubShape = smToCompute->GetSubShape();
      const int aShapeDim = GetShapeDim( aSubShape );
      if ( aShapeDim < 1 ) break;

      SMESH_Algo* algo = GetAlgo( smToCompute );
      if ( algo && !algo->NeedDiscreteBoundary() ) {
        if ( algo->SupportSubmeshes() ) {
          smWithAlgoSupportingSubmeshes.push_front( smToCompute );
        }
        else {
          smToCompute->Evaluate(aResMap);
          if ( aShapesId )
            aShapesId->insert( smToCompute->GetId() );
        }
      }
    }

    // ------------------------------------------------------------
    // sort list of meshes according to mesh order
    // ------------------------------------------------------------
    std::vector< SMESH_subMesh* > smVec( smWithAlgoSupportingSubmeshes.begin(),
                                         smWithAlgoSupportingSubmeshes.end() );
    aMesh.SortByMeshOrder( smVec );

    // ------------------------------------------------------------
    // compute sub-meshes under shapes with algos that DO NOT require
    // Discreteized boundaries and DO support sub-meshes
    // ------------------------------------------------------------
    // start from lower shapes
    for ( size_t i = 0; i < smVec.size(); ++i )
    {
      sm = smVec[i];

      // get a shape the algo is assigned to
      TopoDS_Shape algoShape;
      if ( !GetAlgo( sm, & algoShape ))
        continue; // strange...

      // look for more local algos
      smIt = sm->getDependsOnIterator(!includeSelf, !complexShapeFirst);
      while ( smIt->more() ) {
        SMESH_subMesh* smToCompute = smIt->next();

        const TopoDS_Shape& aSubShape = smToCompute->GetSubShape();
        const int aShapeDim = GetShapeDim( aSubShape );
        if ( aShapeDim < 1 ) continue;

        SMESH_HypoFilter filter( SMESH_HypoFilter::IsAlgo() );
        filter
          .And( SMESH_HypoFilter::IsApplicableTo( aSubShape ))
          .And( SMESH_HypoFilter::IsMoreLocalThan( algoShape, aMesh ));

        if ( SMESH_Algo* subAlgo = (SMESH_Algo*) aMesh.GetHypothesis( smToCompute, filter, true ))
        {
          if ( ! subAlgo->NeedDiscreteBoundary() ) continue;
          SMESH_Hypothesis::Hypothesis_Status status;
          if ( subAlgo->CheckHypothesis( aMesh, aSubShape, status ))
            // mesh a lower smToCompute starting from vertices
            Evaluate( aMesh, aSubShape, aResMap, /*anUpward=*/true, aShapesId );
        }
      }
    }
    // ----------------------------------------------------------
    // apply the algos that do not require Discreteized boundaries
    // ----------------------------------------------------------
    for ( size_t i = 0; i < smVec.size(); ++i )
    {
      sm = smVec[i];
      sm->Evaluate(aResMap);
      if ( aShapesId )
        aShapesId->insert( sm->GetId() );
    }

    // -----------------------------------------------
    // mesh the rest sub-shapes starting from vertices
    // -----------------------------------------------
    ret = Evaluate( aMesh, aShape, aResMap, /*anUpward=*/true, aShapesId );
  }

  return ret;
}


//=======================================================================
//function : checkConformIgnoredAlgos
//purpose  :
//=======================================================================

static bool checkConformIgnoredAlgos(SMESH_Mesh&               aMesh,
                                     SMESH_subMesh*            aSubMesh,
                                     const SMESH_Algo*         aGlobIgnoAlgo,
                                     const SMESH_Algo*         aLocIgnoAlgo,
                                     bool &                    checkConform,
                                     set<SMESH_subMesh*>&      aCheckedMap,
                                     list< SMESH_Gen::TAlgoStateError > & theErrors)
{
  ASSERT( aSubMesh );
  if ( aSubMesh->GetSubShape().ShapeType() == TopAbs_VERTEX)
    return true;


  bool ret = true;

  const list<const SMESHDS_Hypothesis*>& listHyp =
    aMesh.GetMeshDS()->GetHypothesis( aSubMesh->GetSubShape() );
  list<const SMESHDS_Hypothesis*>::const_iterator it=listHyp.begin();
  for ( ; it != listHyp.end(); it++)
  {
    const SMESHDS_Hypothesis * aHyp = *it;
    if (aHyp->GetType() == SMESHDS_Hypothesis::PARAM_ALGO)
      continue;

    const SMESH_Algo* algo = dynamic_cast<const SMESH_Algo*> (aHyp);
    ASSERT ( algo );

    if ( aLocIgnoAlgo ) // algo is hidden by a local algo of upper dim
    {
      theErrors.push_back( SMESH_Gen::TAlgoStateError() );
      theErrors.back().Set( SMESH_Hypothesis::HYP_HIDDEN_ALGO, algo, false );
      INFOS( "Local <" << algo->GetName() << "> is hidden by local <"
            << aLocIgnoAlgo->GetName() << ">");
    }
    else
    {
      bool       isGlobal = (aMesh.IsMainShape( aSubMesh->GetSubShape() ));
      int             dim = algo->GetDim();
      int aMaxGlobIgnoDim = ( aGlobIgnoAlgo ? aGlobIgnoAlgo->GetDim() : -1 );
      bool    isNeededDim = ( aGlobIgnoAlgo ? aGlobIgnoAlgo->NeedLowerHyps( dim ) : false );

      if (( dim < aMaxGlobIgnoDim && !isNeededDim ) &&
          ( isGlobal || !aGlobIgnoAlgo->SupportSubmeshes() ))
      {
        // algo is hidden by a global algo
        theErrors.push_back( SMESH_Gen::TAlgoStateError() );
        theErrors.back().Set( SMESH_Hypothesis::HYP_HIDDEN_ALGO, algo, true );
        INFOS( ( isGlobal ? "Global" : "Local" )
              << " <" << algo->GetName() << "> is hidden by global <"
              << aGlobIgnoAlgo->GetName() << ">");
      }
      else if ( !algo->NeedDiscreteBoundary() && !isGlobal)
      {
        // local algo is not hidden and hides algos on sub-shapes
        if (checkConform && !aSubMesh->IsConform( algo ))
        {
          ret = false;
          checkConform = false; // no more check conformity
          INFOS( "ERROR: Local <" << algo->GetName() <<
                "> would produce not conform mesh: "
                "<Not Conform Mesh Allowed> hypothesis is missing");
          theErrors.push_back( SMESH_Gen::TAlgoStateError() );
          theErrors.back().Set( SMESH_Hypothesis::HYP_NOTCONFORM, algo, false );
        }

        // sub-algos will be hidden by a local <algo> if <algo> does not support sub-meshes
        if ( algo->SupportSubmeshes() )
          algo = 0;
        SMESH_subMeshIteratorPtr revItSub =
          aSubMesh->getDependsOnIterator( /*includeSelf=*/false, /*complexShapeFirst=*/true);
        bool checkConform2 = false;
        while ( revItSub->more() )
        {
          SMESH_subMesh* sm = revItSub->next();
          checkConformIgnoredAlgos (aMesh, sm, aGlobIgnoAlgo,
                                    algo, checkConform2, aCheckedMap, theErrors);
          aCheckedMap.insert( sm );
        }
      }
    }
  }

  return ret;
}

//=======================================================================
//function : checkMissing
//purpose  : notify on missing hypothesis
//           Return false if algo or hypothesis is missing
//=======================================================================

static bool checkMissing(SMESH_Gen*                aGen,
                         SMESH_Mesh&               aMesh,
                         SMESH_subMesh*            aSubMesh,
                         const int                 aTopAlgoDim,
                         bool*                     globalChecked,
                         const bool                checkNoAlgo,
                         set<SMESH_subMesh*>&      aCheckedMap,
                         list< SMESH_Gen::TAlgoStateError > & theErrors)
{
  switch ( aSubMesh->GetSubShape().ShapeType() )
  {
  case TopAbs_EDGE:
  case TopAbs_FACE:
  case TopAbs_SOLID: break; // check this sub-mesh, it can be meshed
  default:
    return true; // not meshable sub-mesh
  }
  if ( aCheckedMap.count( aSubMesh ))
    return true;

  int ret = true;
  SMESH_Algo* algo = 0;

  switch (aSubMesh->GetAlgoState())
  {
  case SMESH_subMesh::NO_ALGO: {
    if (checkNoAlgo)
    {
      // should there be any algo?
      int shapeDim = SMESH_Gen::GetShapeDim( aSubMesh->GetSubShape() );
      if (aTopAlgoDim > shapeDim)
      {
        MESSAGE( "ERROR: " << shapeDim << "D algorithm is missing" );
        ret = false;
        theErrors.push_back( SMESH_Gen::TAlgoStateError() );
        theErrors.back().Set( SMESH_Hypothesis::HYP_MISSING, shapeDim, true );
      }
    }
    return ret;
  }
  case SMESH_subMesh::MISSING_HYP: {
    // notify if an algo missing hyp is attached to aSubMesh
    algo = aSubMesh->GetAlgo();
    ASSERT( algo );
    bool IsGlobalHypothesis = aGen->IsGlobalHypothesis( algo, aMesh );
    if (!IsGlobalHypothesis || !globalChecked[ algo->GetDim() ])
    {
      TAlgoStateErrorName errName = SMESH_Hypothesis::HYP_MISSING;
      SMESH_Hypothesis::Hypothesis_Status status;
      algo->CheckHypothesis( aMesh, aSubMesh->GetSubShape(), status );
      if ( status == SMESH_Hypothesis::HYP_BAD_PARAMETER ) {
        MESSAGE( "ERROR: hypothesis of " << (IsGlobalHypothesis ? "Global " : "Local ")
                 << "<" << algo->GetName() << "> has a bad parameter value");
        errName = status;
      } else if ( status == SMESH_Hypothesis::HYP_BAD_GEOMETRY ) {
        MESSAGE( "ERROR: " << (IsGlobalHypothesis ? "Global " : "Local ")
                 << "<" << algo->GetName() << "> assigned to mismatching geometry");
        errName = status;
      } else {
        MESSAGE( "ERROR: " << (IsGlobalHypothesis ? "Global " : "Local ")
                 << "<" << algo->GetName() << "> misses some hypothesis");
      }
      if (IsGlobalHypothesis)
        globalChecked[ algo->GetDim() ] = true;
      theErrors.push_back( SMESH_Gen::TAlgoStateError() );
      theErrors.back().Set( errName, algo, IsGlobalHypothesis );
    }
    ret = false;
    break;
  }
  case SMESH_subMesh::HYP_OK:
    algo = aSubMesh->GetAlgo();
    ret = true;
    if (!algo->NeedDiscreteBoundary())
    {
      SMESH_subMeshIteratorPtr itsub = aSubMesh->getDependsOnIterator( /*includeSelf=*/false,
                                                                       /*complexShapeFirst=*/false);
      while ( itsub->more() )
        aCheckedMap.insert( itsub->next() );
    }
    break;
  default: ASSERT(0);
  }

  // do not check under algo that hides sub-algos or
  // re-start checking NO_ALGO state
  ASSERT (algo);
  bool isTopLocalAlgo =
    ( aTopAlgoDim <= algo->GetDim() && !aGen->IsGlobalHypothesis( algo, aMesh ));
  if (!algo->NeedDiscreteBoundary() || isTopLocalAlgo)
  {
    bool checkNoAlgo2 = ( algo->NeedDiscreteBoundary() );
    SMESH_subMeshIteratorPtr itsub = aSubMesh->getDependsOnIterator( /*includeSelf=*/false,
                                                                     /*complexShapeFirst=*/true);
    while ( itsub->more() )
    {
      // sub-meshes should not be checked further more
      SMESH_subMesh* sm = itsub->next();

      if (isTopLocalAlgo)
      {
        //check algo on sub-meshes
        int aTopAlgoDim2 = algo->GetDim();
        if (!checkMissing (aGen, aMesh, sm, aTopAlgoDim2,
                           globalChecked, checkNoAlgo2, aCheckedMap, theErrors))
        {
          ret = false;
          if (sm->GetAlgoState() == SMESH_subMesh::NO_ALGO )
            checkNoAlgo2 = false;
        }
      }
      aCheckedMap.insert( sm );
    }
  }
  return ret;
}

//=======================================================================
//function : CheckAlgoState
//purpose  : notify on bad state of attached algos, return false
//           if Compute() would fail because of some algo bad state
//=======================================================================

bool SMESH_Gen::CheckAlgoState(SMESH_Mesh& aMesh, const TopoDS_Shape& aShape)
{
  list< TAlgoStateError > errors;
  return GetAlgoState( aMesh, aShape, errors );
}

//=======================================================================
//function : GetAlgoState
//purpose  : notify on bad state of attached algos, return false
//           if Compute() would fail because of some algo bad state
//           theErrors list contains problems description
//=======================================================================

bool SMESH_Gen::GetAlgoState(SMESH_Mesh&               theMesh,
                             const TopoDS_Shape&       theShape,
                             list< TAlgoStateError > & theErrors)
{
  bool ret = true;
  bool hasAlgo = false;

  SMESH_subMesh*          sm = theMesh.GetSubMesh(theShape);
  const SMESHDS_Mesh* meshDS = theMesh.GetMeshDS();
  TopoDS_Shape     mainShape = meshDS->ShapeToMesh();

  // -----------------
  // get global algos
  // -----------------

  const SMESH_Algo* aGlobAlgoArr[] = {0,0,0,0};

  const list<const SMESHDS_Hypothesis*>& listHyp = meshDS->GetHypothesis( mainShape );
  list<const SMESHDS_Hypothesis*>::const_iterator it=listHyp.begin();
  for ( ; it != listHyp.end(); it++)
  {
    const SMESHDS_Hypothesis * aHyp = *it;
    if (aHyp->GetType() == SMESHDS_Hypothesis::PARAM_ALGO)
      continue;

    const SMESH_Algo* algo = dynamic_cast<const SMESH_Algo*> (aHyp);
    ASSERT ( algo );

    int dim = algo->GetDim();
    aGlobAlgoArr[ dim ] = algo;

    hasAlgo = true;
  }

  // --------------------------------------------------------
  // info on algos that will be ignored because of ones that
  // don't NeedDiscreteBoundary() attached to super-shapes,
  // check that a conform mesh will be produced
  // --------------------------------------------------------


  // find a global algo possibly hiding sub-algos
  int dim;
  const SMESH_Algo* aGlobIgnoAlgo = 0;
  for (dim = 3; dim > 0; dim--)
  {
    if (aGlobAlgoArr[ dim ] &&
        !aGlobAlgoArr[ dim ]->NeedDiscreteBoundary() /*&&
        !aGlobAlgoArr[ dim ]->SupportSubmeshes()*/ )
    {
      aGlobIgnoAlgo = aGlobAlgoArr[ dim ];
      break;
    }
  }

  set<SMESH_subMesh*> aCheckedSubs;
  bool checkConform = ( !theMesh.IsNotConformAllowed() );

  // loop on theShape and its sub-shapes
  SMESH_subMeshIteratorPtr revItSub = sm->getDependsOnIterator( /*includeSelf=*/true,
                                                                /*complexShapeFirst=*/true);
  while ( revItSub->more() )
  {
    SMESH_subMesh* smToCheck = revItSub->next();
    if ( smToCheck->GetSubShape().ShapeType() == TopAbs_VERTEX)
      break;

    if ( aCheckedSubs.insert( smToCheck ).second ) // not yet checked
      if (!checkConformIgnoredAlgos (theMesh, smToCheck, aGlobIgnoAlgo,
                                     0, checkConform, aCheckedSubs, theErrors))
        ret = false;

    if ( smToCheck->GetAlgoState() != SMESH_subMesh::NO_ALGO )
      hasAlgo = true;
  }

  // ----------------------------------------------------------------
  // info on missing hypothesis and find out if all needed algos are
  // well defined
  // ----------------------------------------------------------------

  // find max dim of global algo
  int aTopAlgoDim = 0;
  for (dim = 3; dim > 0; dim--)
  {
    if (aGlobAlgoArr[ dim ])
    {
      aTopAlgoDim = dim;
      break;
    }
  }
  bool checkNoAlgo = theMesh.HasShapeToMesh() ? bool( aTopAlgoDim ) : false;
  bool globalChecked[] = { false, false, false, false };

  // loop on theShape and its sub-shapes
  aCheckedSubs.clear();
  revItSub = sm->getDependsOnIterator( /*includeSelf=*/true, /*complexShapeFirst=*/true);
  while ( revItSub->more() )
  {
    SMESH_subMesh* smToCheck = revItSub->next();
    if ( smToCheck->GetSubShape().ShapeType() == TopAbs_VERTEX)
      break;

    if (!checkMissing (this, theMesh, smToCheck, aTopAlgoDim,
                       globalChecked, checkNoAlgo, aCheckedSubs, theErrors))
    {
      ret = false;
      if (smToCheck->GetAlgoState() == SMESH_subMesh::NO_ALGO )
        checkNoAlgo = false;
    }
  }

  if ( !hasAlgo ) {
    ret = false;
    theErrors.push_back( TAlgoStateError() );
    theErrors.back().Set( SMESH_Hypothesis::HYP_MISSING, theMesh.HasShapeToMesh() ? 1 : 3, true );
  }

  return ret;
}

//=======================================================================
//function : IsGlobalHypothesis
//purpose  : check if theAlgo is attached to the main shape
//=======================================================================

bool SMESH_Gen::IsGlobalHypothesis(const SMESH_Hypothesis* theHyp, SMESH_Mesh& aMesh)
{
  SMESH_HypoFilter filter( SMESH_HypoFilter::Is( theHyp ));
  return aMesh.GetHypothesis( aMesh.GetMeshDS()->ShapeToMesh(), filter, false );
}

//================================================================================
/*!
 * \brief Return paths to xml files of plugins
 */
//================================================================================

std::vector< std::string > SMESH_Gen::GetPluginXMLPaths()
{
  // Get paths to xml files of plugins
  vector< string > xmlPaths;
  string sep;
  if ( const char* meshersList = getenv("SMESH_MeshersList") )
  {
    string meshers = meshersList, plugin;
    string::size_type from = 0, pos;
    while ( from < meshers.size() )
    {
      // cut off plugin name
      pos = meshers.find( env_sep, from );
      if ( pos != string::npos )
        plugin = meshers.substr( from, pos-from );
      else
        plugin = meshers.substr( from ), pos = meshers.size();
      from = pos + 1;

      // get PLUGIN_ROOT_DIR path
      string rootDirVar, pluginSubDir = plugin;
      if ( plugin == "StdMeshers" )
        rootDirVar = "SMESH", pluginSubDir = "smesh";
      else
        for ( pos = 0; pos < plugin.size(); ++pos )
          rootDirVar += toupper( plugin[pos] );
      rootDirVar += "_ROOT_DIR";

      const char* rootDir = getenv( rootDirVar.c_str() );
      if ( !rootDir || strlen(rootDir) == 0 )
      {
        rootDirVar = plugin + "_ROOT_DIR"; // HexoticPLUGIN_ROOT_DIR
        rootDir = getenv( rootDirVar.c_str() );
        if ( !rootDir || strlen(rootDir) == 0 ) continue;
      }

      // get a separator from rootDir
      for ( int i = strlen( rootDir )-1; i >= 0 && sep.empty(); --i )
        if ( rootDir[i] == '/' || rootDir[i] == '\\' )
        {
          sep = rootDir[i];
          break;
        }
#ifdef WIN32
      if (sep.empty() ) sep = "\\";
#else
      if (sep.empty() ) sep = "/";
#endif

      // get a path to resource file
      string xmlPath = rootDir;
      if ( xmlPath[ xmlPath.size()-1 ] != sep[0] )
        xmlPath += sep;
      xmlPath += "share" + sep + "salome" + sep + "resources" + sep;
      for ( pos = 0; pos < pluginSubDir.size(); ++pos )
        xmlPath += tolower( pluginSubDir[pos] );
      xmlPath += sep + plugin + ".xml";
      bool fileOK;
#ifdef WIN32
#  ifdef UNICODE
      const wchar_t* path = Kernel_Utils::decode_s(xmlPath);
      SMESHUtils::ArrayDeleter<const wchar_t> deleter( path );
#  else
      const char* path = xmlPath.c_str();
#  endif
      fileOK = (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES);

#else
      fileOK = (access(xmlPath.c_str(), F_OK) == 0);
#endif
      if ( fileOK )
        xmlPaths.push_back( xmlPath );
    }
  }

  return xmlPaths;
}

//=============================================================================
/*!
 * Finds algo to mesh a shape. Optionally returns a shape the found algo is bound to
 */
//=============================================================================

SMESH_Algo *SMESH_Gen::GetAlgo(SMESH_Mesh &         aMesh,
                               const TopoDS_Shape & aShape,
                               TopoDS_Shape*        assignedTo)
{
  return GetAlgo( aMesh.GetSubMesh( aShape ), assignedTo );
}

//=============================================================================
/*!
 * Finds algo to mesh a sub-mesh. Optionally returns a shape the found algo is bound to
 */
//=============================================================================

SMESH_Algo *SMESH_Gen::GetAlgo(SMESH_subMesh * aSubMesh,
                               TopoDS_Shape*   assignedTo)
{
  if ( !aSubMesh ) return 0;

  const TopoDS_Shape & aShape = aSubMesh->GetSubShape();
  SMESH_Mesh&          aMesh  = *aSubMesh->GetFather();

  SMESH_HypoFilter filter( SMESH_HypoFilter::IsAlgo() );
  if ( aMesh.HasShapeToMesh() )
    filter.And( filter.IsApplicableTo( aShape ));

  typedef SMESH_Algo::Features AlgoData;

  TopoDS_Shape assignedToShape;
  SMESH_Algo* algo =
    (SMESH_Algo*) aMesh.GetHypothesis( aSubMesh, filter, true, &assignedToShape );

  if ( algo &&
       aShape.ShapeType() == TopAbs_FACE &&
       !aShape.IsSame( assignedToShape ) &&
       SMESH_MesherHelper::NbAncestors( aShape, aMesh, TopAbs_SOLID ) > 1 )
  {
    // Issue 0021559. If there is another 2D algo with different types of output
    // elements that can be used to mesh aShape, and 3D algos on adjacent SOLIDs
    // have different types of input elements, we choose a most appropriate 2D algo.

    // try to find a concurrent 2D algo
    filter.AndNot( filter.Is( algo ));
    TopoDS_Shape assignedToShape2;
    SMESH_Algo* algo2 =
      (SMESH_Algo*) aMesh.GetHypothesis( aSubMesh, filter, true, &assignedToShape2 );
    if ( algo2 &&                                                  // algo found
         !assignedToShape2.IsSame( aMesh.GetShapeToMesh() ) &&     // algo is local
         ( SMESH_MesherHelper::GetGroupType( assignedToShape2 ) == // algo of the same level
           SMESH_MesherHelper::GetGroupType( assignedToShape )) &&
         aMesh.IsOrderOK( aMesh.GetSubMesh( assignedToShape2 ),    // no forced order
                          aMesh.GetSubMesh( assignedToShape  )))
    {
      // get algos on the adjacent SOLIDs
      filter.Init( filter.IsAlgo() ).And( filter.HasDim( 3 ));
      vector< SMESH_Algo* > algos3D;
      PShapeIteratorPtr solidIt = SMESH_MesherHelper::GetAncestors( aShape, aMesh,
                                                                    TopAbs_SOLID );
      while ( const TopoDS_Shape* solid = solidIt->next() )
        if ( SMESH_Algo* algo3D = (SMESH_Algo*) aMesh.GetHypothesis( *solid, filter, true ))
        {
          algos3D.push_back( algo3D );
          filter.AndNot( filter.HasName( algo3D->GetName() ));
        }
      // check compatibility of algos
      if ( algos3D.size() > 1 )
      {
        const AlgoData& algoData    = algo->SMESH_Algo::GetFeatures();
        const AlgoData& algoData2   = algo2->SMESH_Algo::GetFeatures();
        const AlgoData& algoData3d0 = algos3D[0]->SMESH_Algo::GetFeatures();
        const AlgoData& algoData3d1 = algos3D[1]->SMESH_Algo::GetFeatures();
        if (( algoData2.IsCompatible( algoData3d0 ) &&
              algoData2.IsCompatible( algoData3d1 ))
            &&
            !(algoData.IsCompatible( algoData3d0 ) &&
              algoData.IsCompatible( algoData3d1 )))
          algo = algo2;
      }
    }
  }

  if ( assignedTo && algo )
    * assignedTo = assignedToShape;

  return algo;
}

//=============================================================================
/*!
 * Returns StudyContextStruct for a study
 */
//=============================================================================

StudyContextStruct *SMESH_Gen::GetStudyContext()
{
  return _studyContext;
}

//================================================================================
/*!
 * \brief Return shape dimension by TopAbs_ShapeEnum
 */
//================================================================================

int SMESH_Gen::GetShapeDim(const TopAbs_ShapeEnum & aShapeType)
{
  static vector<int> dim;
  if ( dim.empty() )
  {
    dim.resize( TopAbs_SHAPE, -1 );
    dim[ TopAbs_COMPOUND ]  = MeshDim_3D;
    dim[ TopAbs_COMPSOLID ] = MeshDim_3D;
    dim[ TopAbs_SOLID ]     = MeshDim_3D;
    dim[ TopAbs_SHELL ]     = MeshDim_2D;
    dim[ TopAbs_FACE  ]     = MeshDim_2D;
    dim[ TopAbs_WIRE ]      = MeshDim_1D;
    dim[ TopAbs_EDGE ]      = MeshDim_1D;
    dim[ TopAbs_VERTEX ]    = MeshDim_0D;
  }
  return dim[ aShapeType ];
}

//================================================================================
/*!
 * \brief Return shape dimension by exploding compounds
 */
//================================================================================

int SMESH_Gen::GetFlatShapeDim(const TopoDS_Shape &aShape)
{
  int aShapeDim;
  if ( aShape.ShapeType() == TopAbs_COMPOUND ||
       aShape.ShapeType() == TopAbs_COMPSOLID )
  {
    TopoDS_Iterator it( aShape );
    aShapeDim = GetFlatShapeDim( it.Value() );
  }
  else
    aShapeDim = GetShapeDim( aShape );
  return aShapeDim;
}

//=============================================================================
/*!
 * Generate a new id unique within this Gen
 */
//=============================================================================

int SMESH_Gen::GetANewId()
{
  return _hypId++;
}
