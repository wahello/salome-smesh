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

//  SMESH DriverMED : driver to read and write 'med' files
//  File   : DriverMED_W_SMESHDS_Mesh.cxx
//  Module : SMESH
//

#include "DriverMED_W_SMESHDS_Mesh.h"

#include "DriverMED_Family.h"
#include "MED_Factory.hxx"
#include "MED_Utilities.hxx"
#include "SMDS_IteratorOnIterators.hxx"
#include "SMDS_MeshNode.hxx"
#include "SMDS_SetIterator.hxx"
#include "SMESHDS_Mesh.hxx"
#include "MED_Common.hxx"
#include "MED_TFile.hxx"

#include <med.h>

#include <BRep_Tool.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include <utilities.h>


#define _EDF_NODE_IDS_
//#define _ELEMENTS_BY_DIM_

using namespace std;
using namespace MED;


//================================================================================
/*!
 * \brief Constructor
 */
//================================================================================

DriverMED_W_SMESHDS_Mesh::DriverMED_W_SMESHDS_Mesh():
  myAllSubMeshes (false),
  myDoGroupOfNodes (false),
  myDoGroupOfEdges (false),
  myDoGroupOfFaces (false),
  myDoGroupOfVolumes (false),
  myDoGroupOf0DElems(false),
  myDoGroupOfBalls(false),
  myAutoDimension(false),
  myAddODOnVertices(false),
  myDoAllInGroups(false),
  myVersion(-1),
  myZTolerance(-1.),
  mySaveNumbers(true)
{}

//================================================================================
/*!
 * \brief Set a file name and a version
 *  \param [in] theFileName - output file name
 *  \param [in] theVersion - desired MED file version == major * 10 + minor
 */
//================================================================================

void DriverMED_W_SMESHDS_Mesh::SetFile(const std::string& theFileName, int theVersion)
{
  myVersion = theVersion;
  Driver_SMESHDS_Mesh::SetFile(theFileName);
}

//================================================================================
/*!
 * MED version is either the latest available, or with an inferior minor,
 * to ensure backward compatibility on writing med files.
 */
//================================================================================

string DriverMED_W_SMESHDS_Mesh::GetVersionString(int theMinor, int theNbDigits)
{
  TInt majeur, mineur, release;
  majeur=MED_MAJOR_NUM;
  mineur=MED_MINOR_NUM;
  release=MED_RELEASE_NUM;
  TInt imposedMineur = mineur;

  if (theMinor < 0)
    imposedMineur = mineur;
  else if (theMinor > MED_MINOR_NUM)
    imposedMineur = mineur;
  else
    imposedMineur = theMinor;

  ostringstream name;
  if ( theNbDigits > 0 )
    name << majeur;
  if ( theNbDigits > 1 )
    name << "." << imposedMineur;
  if ( theNbDigits > 2 )
    name << "." << release;
  return name.str();
}

void DriverMED_W_SMESHDS_Mesh::AddGroup(SMESHDS_GroupBase* theGroup)
{
  myGroups.push_back(theGroup);
}

void DriverMED_W_SMESHDS_Mesh::AddAllSubMeshes()
{
  myAllSubMeshes = true;
}

void DriverMED_W_SMESHDS_Mesh::AddSubMesh(SMESHDS_SubMesh* theSubMesh, int /*theID*/)
{
  mySubMeshes.push_back( theSubMesh );
}

void DriverMED_W_SMESHDS_Mesh::AddGroupOfNodes()
{
  myDoGroupOfNodes = true;
}

void DriverMED_W_SMESHDS_Mesh::AddGroupOfEdges()
{
  myDoGroupOfEdges = true;
}

void DriverMED_W_SMESHDS_Mesh::AddGroupOfFaces()
{
  myDoGroupOfFaces = true;
}

void DriverMED_W_SMESHDS_Mesh::AddGroupOfVolumes()
{
  myDoGroupOfVolumes = true;
}

void DriverMED_W_SMESHDS_Mesh::AddGroupOf0DElems()
{
  myDoGroupOf0DElems = true;
}

void DriverMED_W_SMESHDS_Mesh::AddGroupOfBalls()
{
  myDoGroupOfBalls = true;
}


namespace
{
  //---------------------------------------------
  /*!
   * \brief Retrieving node coordinates utilities
   */
  //---------------------------------------------

  typedef double (SMDS_MeshNode::* TGetCoord)() const;
  typedef const char* TName;
  typedef const char* TUnit;

  // name length in a mesh must be equal to 16 :
  //         1234567890123456
  TName M = "m               ";
  TName X = "x               ";
  TName Y = "y               ";
  TName Z = "z               ";

  TUnit aUnit[3] = {M,M,M};

  // 3 dim
  TGetCoord aXYZGetCoord[3] = {
    &SMDS_MeshNode::X, 
    &SMDS_MeshNode::Y, 
    &SMDS_MeshNode::Z
  };
  TName aXYZName[3] = {X,Y,Z};
  
  // 2 dim
  TGetCoord aXYGetCoord[2] = {
    &SMDS_MeshNode::X, 
    &SMDS_MeshNode::Y
  };
  TName aXYName[2] = {X,Y};

  TGetCoord aYZGetCoord[2] = {
    &SMDS_MeshNode::Y, 
    &SMDS_MeshNode::Z
  };
  TName aYZName[2] = {Y,Z};

  TGetCoord aXZGetCoord[2] = {
    &SMDS_MeshNode::X, 
    &SMDS_MeshNode::Z
  };
  TName aXZName[2] = {X,Z};

  // 1 dim
  TGetCoord aXGetCoord[1] = {
    &SMDS_MeshNode::X
  };
  TName aXName[1] = {X};

  TGetCoord aYGetCoord[1] = {
    &SMDS_MeshNode::Y
  };
  TName aYName[1] = {Y};

  TGetCoord aZGetCoord[1] = {
    &SMDS_MeshNode::Z
  };
  TName aZName[1] = {Z};


  class TCoordHelper{
    SMDS_NodeIteratorPtr myNodeIter;
    const SMDS_MeshNode* myCurrentNode;
    TGetCoord* myGetCoord;
    TName* myName;
    TUnit* myUnit;
  public:
    TCoordHelper(const SMDS_NodeIteratorPtr& theNodeIter,
                 TGetCoord* theGetCoord,
                 TName* theName,
                 TUnit* theUnit = aUnit):
      myNodeIter(theNodeIter),
      myGetCoord(theGetCoord),
      myName(theName),
      myUnit(theUnit)
    {}
    virtual ~TCoordHelper(){}
    bool Next(){ 
      return myNodeIter->more() && 
        (myCurrentNode = myNodeIter->next());
    }
    const SMDS_MeshNode* GetNode(){
      return myCurrentNode;
    }
    MED::TIDVector::value_type GetID(){
      return myCurrentNode->GetID();
    }
    MED::TFloatVector::value_type GetCoord(TInt theCoodId){
      return (myCurrentNode->*myGetCoord[theCoodId])();
    }
    MED::TStringVector::value_type GetName(TInt theDimId){
      return myName[theDimId];
    }
    MED::TStringVector::value_type GetUnit(TInt theDimId){
      return myUnit[theDimId];
    }
  };
  typedef boost::shared_ptr<TCoordHelper> TCoordHelperPtr;

  //-------------------------------------------------------
  /*!
   * \brief Structure describing element type
   */
  //-------------------------------------------------------
  struct TElemTypeData
  {
    EEntiteMaillage     _entity;
    EGeometrieElement   _geomType;
    TInt                _nbElems;
    SMDSAbs_ElementType _smdsType;

    TElemTypeData (EEntiteMaillage entity, EGeometrieElement geom, TInt nb, SMDSAbs_ElementType type)
      : _entity(entity), _geomType(geom), _nbElems( nb ), _smdsType( type ) {}
  };


  typedef NCollection_DataMap< Standard_Address, int > TElemFamilyMap;

  //================================================================================
  /*!
   * \brief Fills element to famaly ID map for element type.
   * Removes all families of anElemType
   */
  //================================================================================

  void fillElemFamilyMap( TElemFamilyMap &            anElemFamMap,
                          list<DriverMED_FamilyPtr> & aFamilies,
                          const SMDSAbs_ElementType   anElemType)
  {
    anElemFamMap.Clear();
    list<DriverMED_FamilyPtr>::iterator aFamsIter = aFamilies.begin();
    while ( aFamsIter != aFamilies.end() )
    {
      if ((*aFamsIter)->GetType() != anElemType) {
        aFamsIter++;
      }
      else {
        int aFamId = (*aFamsIter)->GetId();
        const ElementsSet&              anElems = (*aFamsIter)->GetElements();
        ElementsSet::const_iterator anElemsIter = anElems.begin();
        for (; anElemsIter != anElems.end(); anElemsIter++)
        {
          anElemFamMap.Bind( (Standard_Address)*anElemsIter, aFamId );
        }
        // remove a family from the list
        aFamilies.erase( aFamsIter++ );
      }
    }
  }

  //================================================================================
  /*!
   * \brief For an element, return family ID found in the map or a default one
   */
  //================================================================================

  int getFamilyId( const TElemFamilyMap &  anElemFamMap,
                   const SMDS_MeshElement* anElement,
                   const int               aDefaultFamilyId)
  {
    if ( anElemFamMap.IsBound( (Standard_Address) anElement ))
      return anElemFamMap( (Standard_Address) anElement );

    return aDefaultFamilyId;
  }

  //================================================================================
  /*!
   * \brief Returns iterator on sub-meshes
   */
  //================================================================================

  SMESHDS_SubMeshIteratorPtr getIterator( std::vector<SMESHDS_SubMesh*>& mySubMeshes )
  {
    return SMESHDS_SubMeshIteratorPtr
      ( new SMDS_SetIterator
        < const SMESHDS_SubMesh*, std::vector< SMESHDS_SubMesh* >::iterator >( mySubMeshes.begin(),
                                                                               mySubMeshes.end() ));
  }
}

//================================================================================
/*!
 * \brief Write my mesh to a file
 */
//================================================================================

Driver_Mesh::Status DriverMED_W_SMESHDS_Mesh::Perform()
{
  MED::PWrapper myMed = CrWrapperW(myFile, myVersion);
  return this->PerformInternal<MED::PWrapper>(myMed);
}

//================================================================================
/*!
 * \brief Write my mesh to a MEDCoupling DS
 */
//================================================================================

Driver_Mesh::Status DriverMED_W_SMESHDS_Mesh_Mem::Perform()
{
  void *ptr(nullptr);
  std::size_t sz(0);
  Driver_Mesh::Status status = Driver_Mesh::DRS_OK;
  bool isClosed(false);
  {// let braces to flush (call of MED::PWrapper myMed destructor)
    TMemFile *tfileInst = new TMemFile(&ptr,&sz,&isClosed);// this new will be destroyed by destructor of myMed
    MED::PWrapper myMed = CrWrapperW(myFile, -1, tfileInst);
    status = this->PerformInternal<MED::PWrapper>(myMed);
  }
  if(!isClosed)
    EXCEPTION(std::runtime_error, "TFTMemFile destructor : on destruction file has not been closed properly -> chunk of memory data may be invalid !");
  _data = MEDCoupling::DataArrayByte::New();
  _data->useArray(reinterpret_cast<char *>(ptr),true,MEDCoupling::DeallocType::C_DEALLOC,sz,1);
  if(!isClosed)
    THROW_SALOME_EXCEPTION("DriverMED_W_SMESHDS_Mesh_Mem::Perform - MED memory file id is supposed to be closed !");
  return status;
}

//================================================================================
/*!
 * \brief Write my mesh
 */
//================================================================================

template<class LowLevelWriter>
Driver_Mesh::Status DriverMED_W_SMESHDS_Mesh::PerformInternal(LowLevelWriter myMed)
{
  Status aResult = DRS_OK;
  try {
    //MESSAGE("Perform - myFile : "<<myFile);

    if ( Driver_Mesh::IsMeshTooLarge< TInt >( myMesh, /*checkIDs =*/ true ))
      return DRS_TOO_LARGE_MESH;

    // Creating the MED mesh for corresponding SMDS structure
    //-------------------------------------------------------
    string aMeshName;
    if (myMeshId != -1) {
      ostringstream aMeshNameStr;
      aMeshNameStr<<myMeshId;
      aMeshName = aMeshNameStr.str();
    } else {
      aMeshName = myMeshName;
    }

    // Mesh dimension definition

    TInt aMeshDimension = 0;
    if ( myMesh->NbEdges() > 0 )
      aMeshDimension = 1;
    if ( myMesh->NbFaces() > 0 )
      aMeshDimension = 2;
    if ( myMesh->NbVolumes() > 0 )
      aMeshDimension = 3;

    TInt aSpaceDimension = 3;
    TCoordHelperPtr aCoordHelperPtr;
    {
      bool anIsXDimension = false;
      bool anIsYDimension = false;
      bool anIsZDimension = false;
      if ( myAutoDimension && aMeshDimension < 3 )
      {
        SMDS_NodeIteratorPtr aNodesIter = myMesh->nodesIterator();
        double aBounds[6] = {0.,0.,0.,0.,0.,0.};
        if(aNodesIter->more()){
          const SMDS_MeshNode* aNode = aNodesIter->next();
          aBounds[0] = aBounds[1] = aNode->X();
          aBounds[2] = aBounds[3] = aNode->Y();
          aBounds[4] = aBounds[5] = aNode->Z();
        }
        while(aNodesIter->more()){
          const SMDS_MeshNode* aNode = aNodesIter->next();
          aBounds[0] = min(aBounds[0],aNode->X());
          aBounds[1] = max(aBounds[1],aNode->X());

          aBounds[2] = min(aBounds[2],aNode->Y());
          aBounds[3] = max(aBounds[3],aNode->Y());

          aBounds[4] = min(aBounds[4],aNode->Z());
          aBounds[5] = max(aBounds[5],aNode->Z());
        }

        double EPS = 1.0E-7;
        TopoDS_Shape mainShape = myMesh->ShapeToMesh();
        bool    hasShapeToMesh = ( myMesh->SubMeshIndices().size() > 1 );
        if ( !mainShape.IsNull() && hasShapeToMesh )
        {
          // define EPS by max tolerance of the mainShape (IPAL53097)
          TopExp_Explorer subShape;
          for ( subShape.Init( mainShape, TopAbs_FACE ); subShape.More(); subShape.Next() ) {
            EPS = Max( EPS, BRep_Tool::Tolerance( TopoDS::Face( subShape.Current() )));
          }
          for ( subShape.Init( mainShape, TopAbs_EDGE ); subShape.More(); subShape.Next() ) {
            EPS = Max( EPS, BRep_Tool::Tolerance( TopoDS::Edge( subShape.Current() )));
          }
          for ( subShape.Init( mainShape, TopAbs_VERTEX ); subShape.More(); subShape.Next() ) {
            EPS = Max( EPS, BRep_Tool::Tolerance( TopoDS::Vertex( subShape.Current() )));
          }
          EPS *= 2.;
        }
        anIsXDimension = (aBounds[1] - aBounds[0]) + abs(aBounds[1]) + abs(aBounds[0]) > EPS;
        anIsYDimension = (aBounds[3] - aBounds[2]) + abs(aBounds[3]) + abs(aBounds[2]) > EPS;
        anIsZDimension = (aBounds[5] - aBounds[4]) + abs(aBounds[5]) + abs(aBounds[4]) > EPS;
        if ( myZTolerance > 0 && anIsZDimension )
          anIsZDimension = (aBounds[5] > myZTolerance || aBounds[4] < -myZTolerance );
        aSpaceDimension = Max( aMeshDimension, anIsXDimension + anIsYDimension + anIsZDimension );
        if ( !aSpaceDimension )
          aSpaceDimension = 3;
        // PAL16857(SMESH not conform to the MED convention):
        if ( aSpaceDimension == 2 && anIsZDimension ) // 2D only if mesh is in XOY plane
          aSpaceDimension = 3;
        // PAL18941(a saved study with a mesh belong Z is opened and the mesh is belong X)
        if ( aSpaceDimension == 1 && !anIsXDimension ) {// 1D only if mesh is along OX
          if ( anIsYDimension ) {
            aSpaceDimension = 2;
            anIsXDimension = true;
          } else {
            aSpaceDimension = 3;
          }
        }
      }

      SMDS_NodeIteratorPtr aNodesIter = myMesh->nodesIterator();
      switch ( aSpaceDimension ) {
      case 3:
        aCoordHelperPtr.reset(new TCoordHelper(aNodesIter,aXYZGetCoord,aXYZName));
        break;
      case 2:
        if(anIsXDimension && anIsYDimension)
          aCoordHelperPtr.reset(new TCoordHelper(aNodesIter,aXYGetCoord,aXYName));
        if(anIsYDimension && anIsZDimension)
          aCoordHelperPtr.reset(new TCoordHelper(aNodesIter,aYZGetCoord,aYZName));
        if(anIsXDimension && anIsZDimension)
          aCoordHelperPtr.reset(new TCoordHelper(aNodesIter,aXZGetCoord,aXZName));
        break;
      case 1:
        if(anIsXDimension)
          aCoordHelperPtr.reset(new TCoordHelper(aNodesIter,aXGetCoord,aXName));
        if(anIsYDimension)
          aCoordHelperPtr.reset(new TCoordHelper(aNodesIter,aYGetCoord,aYName));
        if(anIsZDimension)
          aCoordHelperPtr.reset(new TCoordHelper(aNodesIter,aZGetCoord,aZName));
        break;
      }
    }

    PMeshInfo aMeshInfo = myMed->CrMeshInfo(aMeshDimension,aSpaceDimension,aMeshName);
    //MESSAGE("Add - aMeshName : "<<aMeshName<<"; "<<aMeshInfo->GetName());
    myMed->SetMeshInfo(aMeshInfo);

    // Storing SMDS groups and sub-meshes as med families
    //----------------------------------------------------
    int myNodesDefaultFamilyId      = 0;
    int my0DElementsDefaultFamilyId = 0;
    int myBallsDefaultFamilyId      = 0;
    int myEdgesDefaultFamilyId      = 0;
    int myFacesDefaultFamilyId      = 0;
    int myVolumesDefaultFamilyId    = 0;
    smIdType nbNodes      = myMesh->NbNodes();
    smIdType nb0DElements = myMesh->Nb0DElements();
    smIdType nbBalls      = myMesh->NbBalls();
    smIdType nbEdges      = myMesh->NbEdges();
    smIdType nbFaces      = myMesh->NbFaces();
    smIdType nbVolumes    = myMesh->NbVolumes();
    if (myDoGroupOfNodes)   myNodesDefaultFamilyId      = REST_NODES_FAMILY;
    if (myDoGroupOfEdges)   myEdgesDefaultFamilyId      = REST_EDGES_FAMILY;
    if (myDoGroupOfFaces)   myFacesDefaultFamilyId      = REST_FACES_FAMILY;
    if (myDoGroupOfVolumes) myVolumesDefaultFamilyId    = REST_VOLUMES_FAMILY;
    if (myDoGroupOf0DElems) my0DElementsDefaultFamilyId = REST_0DELEM_FAMILY;
    if (myDoGroupOfBalls)   myBallsDefaultFamilyId      = REST_BALL_FAMILY;
    if (myDoAllInGroups )
    {
      if (!myDoGroupOfEdges)   myEdgesDefaultFamilyId      = NIG_EDGES_FAMILY   ;
      if (!myDoGroupOfFaces)   myFacesDefaultFamilyId      = NIG_FACES_FAMILY   ;
      if (!myDoGroupOfVolumes) myVolumesDefaultFamilyId    = NIG_VOLS_FAMILY    ;
      if (!myDoGroupOf0DElems) my0DElementsDefaultFamilyId = NIG_0DELEM_FAMILY  ;
      if (!myDoGroupOfBalls)   myBallsDefaultFamilyId      = NIG_BALL_FAMILY    ;
    }

    //MESSAGE("Perform - aFamilyInfo");
    list<DriverMED_FamilyPtr> aFamilies;
    if (myAllSubMeshes) {
      aFamilies = DriverMED_Family::MakeFamilies
        (myMesh->SubMeshes(), myGroups,
         myDoGroupOfNodes   && nbNodes,
         myDoGroupOfEdges   && nbEdges,
         myDoGroupOfFaces   && nbFaces,
         myDoGroupOfVolumes && nbVolumes,
         myDoGroupOf0DElems && nb0DElements,
         myDoGroupOfBalls   && nbBalls,
         myDoAllInGroups);
    }
    else {
      aFamilies = DriverMED_Family::MakeFamilies
        (getIterator( mySubMeshes ), myGroups,
         myDoGroupOfNodes   && nbNodes,
         myDoGroupOfEdges   && nbEdges,
         myDoGroupOfFaces   && nbFaces,
         myDoGroupOfVolumes && nbVolumes,
         myDoGroupOf0DElems && nb0DElements,
         myDoGroupOfBalls   && nbBalls,
         myDoAllInGroups);
    }
    list<DriverMED_FamilyPtr>::iterator aFamsIter;
    for (aFamsIter = aFamilies.begin(); aFamsIter != aFamilies.end(); aFamsIter++)
    {
      PFamilyInfo aFamilyInfo = (*aFamsIter)->GetFamilyInfo<LowLevelWriter>(myMed,aMeshInfo);
      myMed->SetFamilyInfo(aFamilyInfo);
    }

    // Storing SMDS nodes to the MED file for the MED mesh
    //----------------------------------------------------
#ifdef _EDF_NODE_IDS_
    typedef map<TInt,TInt> TNodeIdMap;
    TNodeIdMap aNodeIdMap;
#endif
    const EModeSwitch   theMode        = eFULL_INTERLACE;
    const ERepere       theSystem      = eCART;
    const EBooleen      theIsElemNum   = mySaveNumbers ? eVRAI : eFAUX;
    const EBooleen      theIsElemNames = eFAUX;
    const EConnectivite theConnMode    = eNOD;

    TInt aNbNodes = FromSmIdType<TInt>( myMesh->NbNodes() );
    PNodeInfo aNodeInfo = myMed->CrNodeInfo(aMeshInfo, aNbNodes,
                                            theMode, theSystem, theIsElemNum, theIsElemNames);

    // find family numbers for nodes
    TElemFamilyMap anElemFamMap;
    fillElemFamilyMap( anElemFamMap, aFamilies, SMDSAbs_Node );

    for (TInt iNode = 0; aCoordHelperPtr->Next(); iNode++)
    {
      // coordinates
      TCoordSlice aTCoordSlice = aNodeInfo->GetCoordSlice( iNode );
      for(TInt iCoord = 0; iCoord < aSpaceDimension; iCoord++){
        aTCoordSlice[iCoord] = aCoordHelperPtr->GetCoord(iCoord);
      }
      if ( aSpaceDimension == 3 &&
           -myZTolerance < aTCoordSlice[2] && aTCoordSlice[2] < myZTolerance )
        aTCoordSlice[2] = 0.;

      // node number
      TInt aNodeID = FromSmIdType<TInt>( aCoordHelperPtr->GetID() );
      aNodeInfo->SetElemNum( iNode, aNodeID );
#ifdef _EDF_NODE_IDS_
      aNodeIdMap.insert( aNodeIdMap.end(), make_pair( aNodeID, iNode+1 ));
#endif
      // family number
      const SMDS_MeshNode* aNode = aCoordHelperPtr->GetNode();
      int famNum = getFamilyId( anElemFamMap, aNode, myNodesDefaultFamilyId );
      aNodeInfo->SetFamNum( iNode, famNum );
    }
    anElemFamMap.Clear();

    // coordinate names and units
    for (TInt iCoord = 0; iCoord < aSpaceDimension; iCoord++) {
      aNodeInfo->SetCoordName( iCoord, aCoordHelperPtr->GetName(iCoord));
      aNodeInfo->SetCoordUnit( iCoord, aCoordHelperPtr->GetUnit(iCoord));
    }

    //MESSAGE("Perform - aNodeInfo->GetNbElem() = "<<aNbNodes);
    myMed->SetNodeInfo(aNodeInfo);
    aNodeInfo.reset(); // free memory used for arrays


    // Storing SMDS elements to the MED file for the MED mesh
    //-------------------------------------------------------
    // Write one element type at once in order to minimize memory usage (PAL19276)

    const SMDS_MeshInfo& nbElemInfo = myMesh->GetMeshInfo();

    // poly elements are not supported by med-2.1
    bool polyTypesSupported = ( myMed->CrPolygoneInfo(aMeshInfo,eMAILLE,ePOLYGONE,0,0).get() != 0 );
    TInt nbPolygonNodes = 0, nbPolyhedronNodes = 0, nbPolyhedronFaces = 0;

    // nodes on VERTEXes where 0D elements are absent
    std::vector<const SMDS_MeshElement*> nodesOf0D;
    std::vector< SMDS_ElemIteratorPtr > iterVec;
    SMDS_ElemIteratorPtr iterVecIter;
    if ( myAddODOnVertices && getNodesOfMissing0DOnVert( myMesh, nodesOf0D ))
    {
      iterVec.resize(2);
      iterVec[0] = myMesh->elementsIterator( SMDSAbs_0DElement );
      iterVec[1] = SMDS_ElemIteratorPtr
        ( new SMDS_ElementVectorIterator( nodesOf0D.begin(), nodesOf0D.end() ));

      typedef SMDS_IteratorOnIterators
        < const SMDS_MeshElement *, std::vector< SMDS_ElemIteratorPtr > > TItIterator;
      iterVecIter = SMDS_ElemIteratorPtr( new TItIterator( iterVec ));
    }

    // collect info on all geom types

    list< TElemTypeData > aTElemTypeDatas;

    EEntiteMaillage anEntity = eMAILLE;
#ifdef _ELEMENTS_BY_DIM_
    anEntity = eNOEUD_ELEMENT;
#endif
    aTElemTypeDatas.push_back(TElemTypeData(anEntity,
                                            ePOINT1,
                                            nbElemInfo.Nb0DElements() + nodesOf0D.size(),
                                            SMDSAbs_0DElement));
#ifdef _ELEMENTS_BY_DIM_
    anEntity = eSTRUCT_ELEMENT;
#endif
    aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                             eBALL,
                                             FromSmIdType<TInt>(nbElemInfo.NbBalls()),
                                             SMDSAbs_Ball));
#ifdef _ELEMENTS_BY_DIM_
    anEntity = eARETE;
#endif
    aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                             eSEG2,
                                             FromSmIdType<TInt>(nbElemInfo.NbEdges( ORDER_LINEAR )),
                                             SMDSAbs_Edge));
    aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                             eSEG3,
                                             FromSmIdType<TInt>(nbElemInfo.NbEdges( ORDER_QUADRATIC )),
                                             SMDSAbs_Edge));
#ifdef _ELEMENTS_BY_DIM_
    anEntity = eFACE;
#endif
    aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                             eTRIA3,
                                             FromSmIdType<TInt>(nbElemInfo.NbTriangles( ORDER_LINEAR )),
                                             SMDSAbs_Face));
    aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                             eTRIA6,
                                             FromSmIdType<TInt>(nbElemInfo.NbTriangles( ORDER_QUADRATIC ) -
                                             nbElemInfo.NbBiQuadTriangles()),
                                             SMDSAbs_Face));
    aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                             eTRIA7,
                                             FromSmIdType<TInt>(nbElemInfo.NbBiQuadTriangles()),
                                             SMDSAbs_Face));
    aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                             eQUAD4,
                                             FromSmIdType<TInt>(nbElemInfo.NbQuadrangles( ORDER_LINEAR )),
                                             SMDSAbs_Face));
    aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                             eQUAD8,
                                             FromSmIdType<TInt>(nbElemInfo.NbQuadrangles( ORDER_QUADRATIC ) -
                                             nbElemInfo.NbBiQuadQuadrangles()),
                                             SMDSAbs_Face));
    aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                             eQUAD9,
                                             FromSmIdType<TInt>(nbElemInfo.NbBiQuadQuadrangles()),
                                             SMDSAbs_Face));
    if ( polyTypesSupported ) {
      aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                               ePOLYGONE,
                                               FromSmIdType<TInt>(nbElemInfo.NbPolygons( ORDER_LINEAR )),
                                               SMDSAbs_Face));
      // we need one more loop on poly elements to count nb of their nodes
      aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                               ePOLYGONE,
                                               FromSmIdType<TInt>(nbElemInfo.NbPolygons( ORDER_LINEAR )),
                                               SMDSAbs_Face));
      aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                               ePOLYGON2,
                                               FromSmIdType<TInt>(nbElemInfo.NbPolygons( ORDER_QUADRATIC )),
                                               SMDSAbs_Face));
      // we need one more loop on QUAD poly elements to count nb of their nodes
      aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                               ePOLYGON2,
                                               FromSmIdType<TInt>(nbElemInfo.NbPolygons( ORDER_QUADRATIC )),
                                               SMDSAbs_Face));
    }
#ifdef _ELEMENTS_BY_DIM_
    anEntity = eMAILLE;
#endif
    aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                             eTETRA4,
                                             FromSmIdType<TInt>(nbElemInfo.NbTetras( ORDER_LINEAR )),
                                             SMDSAbs_Volume));
    aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                             eTETRA10,
                                             FromSmIdType<TInt>(nbElemInfo.NbTetras( ORDER_QUADRATIC )),
                                             SMDSAbs_Volume));
    aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                             ePYRA5,
                                             FromSmIdType<TInt>(nbElemInfo.NbPyramids( ORDER_LINEAR )),
                                             SMDSAbs_Volume));
    aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                             ePYRA13,
                                             FromSmIdType<TInt>(nbElemInfo.NbPyramids( ORDER_QUADRATIC )),
                                             SMDSAbs_Volume));
    aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                             ePENTA6,
                                             FromSmIdType<TInt>(nbElemInfo.NbPrisms( ORDER_LINEAR )),
                                             SMDSAbs_Volume));
    aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                             ePENTA15,
                                             FromSmIdType<TInt>(nbElemInfo.NbQuadPrisms()),
                                             SMDSAbs_Volume));
    aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                             ePENTA18,
                                             FromSmIdType<TInt>(nbElemInfo.NbBiQuadPrisms()),
                                             SMDSAbs_Volume));
    aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                             eHEXA8,
                                             FromSmIdType<TInt>(nbElemInfo.NbHexas( ORDER_LINEAR )),
                                             SMDSAbs_Volume));
    aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                             eHEXA20,
                                             FromSmIdType<TInt>(nbElemInfo.NbHexas( ORDER_QUADRATIC )-
                                             nbElemInfo.NbTriQuadHexas()),
                                             SMDSAbs_Volume));
    aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                             eHEXA27,
                                             FromSmIdType<TInt>(nbElemInfo.NbTriQuadHexas()),
                                             SMDSAbs_Volume));
    aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                             eOCTA12,
                                             FromSmIdType<TInt>(nbElemInfo.NbHexPrisms()),
                                             SMDSAbs_Volume));
    if ( polyTypesSupported ) {
      aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                               ePOLYEDRE,
                                               FromSmIdType<TInt>(nbElemInfo.NbPolyhedrons()),
                                               SMDSAbs_Volume));
      // we need one more loop on poly elements to count nb of their nodes
      aTElemTypeDatas.push_back( TElemTypeData(anEntity,
                                               ePOLYEDRE,
                                               FromSmIdType<TInt>(nbElemInfo.NbPolyhedrons()),
                                               SMDSAbs_Volume));
    }

    vector< bool > isElemFamMapBuilt( SMDSAbs_NbElementTypes, false );

    // loop on all geom types of elements

    list< TElemTypeData >::iterator aElemTypeData = aTElemTypeDatas.begin();
    for ( ; aElemTypeData != aTElemTypeDatas.end(); ++aElemTypeData )
    {
      if ( aElemTypeData->_nbElems == 0 )
        continue;

      int defaultFamilyId = 0;
      switch ( aElemTypeData->_smdsType ) {
      case SMDSAbs_0DElement: defaultFamilyId = my0DElementsDefaultFamilyId; break;
      case SMDSAbs_Ball:      defaultFamilyId = myBallsDefaultFamilyId;      break;
      case SMDSAbs_Edge:      defaultFamilyId = myEdgesDefaultFamilyId;      break;
      case SMDSAbs_Face:      defaultFamilyId = myFacesDefaultFamilyId;      break;
      case SMDSAbs_Volume:    defaultFamilyId = myVolumesDefaultFamilyId;    break;
      default:
        continue;
      }

      // build map of family numbers for this type
      if ( !isElemFamMapBuilt[ aElemTypeData->_smdsType ])
      {
        fillElemFamilyMap( anElemFamMap, aFamilies, aElemTypeData->_smdsType );
        isElemFamMapBuilt[ aElemTypeData->_smdsType ] = true;
      }

      // iterator on elements of a current type
      SMDS_ElemIteratorPtr elemIterator;
      TInt iElem = 0;

      // Treat POLYGONs
      // ---------------
      if ( aElemTypeData->_geomType == ePOLYGONE ||
           aElemTypeData->_geomType == ePOLYGON2 )
      {
        if ( aElemTypeData->_geomType == ePOLYGONE )
          elemIterator = myMesh->elementEntityIterator( SMDSEntity_Polygon );
        else
          elemIterator = myMesh->elementEntityIterator( SMDSEntity_Quad_Polygon );

        if ( nbPolygonNodes == 0 ) {
          // Count nb of nodes
          while ( elemIterator->more() ) {
            const SMDS_MeshElement* anElem = elemIterator->next();
            nbPolygonNodes += anElem->NbNodes();
            if ( ++iElem == aElemTypeData->_nbElems )
              break;
          }
        }
        else {
          // Store in med file
          PPolygoneInfo aPolygoneInfo = myMed->CrPolygoneInfo(aMeshInfo,
                                                              aElemTypeData->_entity,
                                                              aElemTypeData->_geomType,
                                                              aElemTypeData->_nbElems,
                                                              nbPolygonNodes,
                                                              theConnMode, theIsElemNum,
                                                              theIsElemNames);
          TElemNum & index = *(aPolygoneInfo->myIndex.get());
          index[0] = 1;

          while ( elemIterator->more() )
          {
            const SMDS_MeshElement* anElem = elemIterator->next();
            // index
            TInt aNbNodes = anElem->NbNodes();
            index[ iElem+1 ] = index[ iElem ] + aNbNodes;

            // connectivity
            TConnSlice aTConnSlice = aPolygoneInfo->GetConnSlice( iElem );
            for(TInt iNode = 0; iNode < aNbNodes; iNode++) {
              const SMDS_MeshElement* aNode = anElem->GetNode( iNode );
#ifdef _EDF_NODE_IDS_
              aTConnSlice[ iNode ] = aNodeIdMap[FromSmIdType<TInt>(aNode->GetID())];
#else
              aTConnSlice[ iNode ] = aNode->GetID();
#endif
            }
            // element number
            aPolygoneInfo->SetElemNum( iElem, FromSmIdType<TInt>(anElem->GetID()) );

            // family number
            int famNum = getFamilyId( anElemFamMap, anElem, defaultFamilyId );
            aPolygoneInfo->SetFamNum( iElem, famNum );

            if ( ++iElem == aPolygoneInfo->GetNbElem() )
              break;
          }
          myMed->SetPolygoneInfo(aPolygoneInfo);

          nbPolygonNodes = 0; // to treat next polygon type
        }
      }

      // Treat POLYEDREs
      // ----------------
      else if ( aElemTypeData->_geomType == ePOLYEDRE )
      {
        elemIterator = myMesh->elementGeomIterator( SMDSGeom_POLYHEDRA );

        if ( nbPolyhedronNodes == 0 ) {
          // Count nb of nodes
          while ( elemIterator->more() ) {
            const SMDS_MeshElement*  anElem = elemIterator->next();
            nbPolyhedronNodes += anElem->NbNodes();
            nbPolyhedronFaces += anElem->NbFaces();
            if ( ++iElem == aElemTypeData->_nbElems )
              break;
          }
        }
        else {
          // Store in med file
          PPolyedreInfo aPolyhInfo = myMed->CrPolyedreInfo(aMeshInfo,
                                                           aElemTypeData->_entity,
                                                           aElemTypeData->_geomType,
                                                           aElemTypeData->_nbElems,
                                                           nbPolyhedronFaces+1,
                                                           nbPolyhedronNodes,
                                                           theConnMode,
                                                           theIsElemNum,
                                                           theIsElemNames);
          TElemNum & index = *(aPolyhInfo->myIndex.get());
          TElemNum & faces = *(aPolyhInfo->myFaces.get());
          TElemNum & conn  = *(aPolyhInfo->myConn.get());
          index[0] = 1;
          faces[0] = 1;

          TInt iFace = 0, iNode = 0;
          while ( elemIterator->more() )
          {
            const SMDS_MeshElement* anElem = elemIterator->next();
            const SMDS_MeshVolume *aPolyedre = myMesh->DownCast< SMDS_MeshVolume >( anElem );
            if ( !aPolyedre ) continue;
            // index
            TInt aNbFaces = aPolyedre->NbFaces();
            index[ iElem+1 ] = index[ iElem ] + aNbFaces;

            // face index
            for (TInt f = 1; f <= aNbFaces; ++f, ++iFace ) {
              int aNbFaceNodes = aPolyedre->NbFaceNodes( f );
              faces[ iFace+1 ] = faces[ iFace ] + aNbFaceNodes;
            }
            // connectivity
            SMDS_ElemIteratorPtr nodeIt = anElem->nodesIterator();
            while ( nodeIt->more() ) {
              const SMDS_MeshElement* aNode = nodeIt->next();
#ifdef _EDF_NODE_IDS_
              conn[ iNode ] = aNodeIdMap[FromSmIdType<TInt>(aNode->GetID())];
#else
              conn[ iNode ] = aNode->GetID();
#endif
              ++iNode;
            }
            // element number
            aPolyhInfo->SetElemNum( iElem, FromSmIdType<TInt>(anElem->GetID()) );

            // family number
            int famNum = getFamilyId( anElemFamMap, anElem, defaultFamilyId );
            aPolyhInfo->SetFamNum( iElem, famNum );

            if ( ++iElem == aPolyhInfo->GetNbElem() )
              break;
          }
          myMed->SetPolyedreInfo(aPolyhInfo);
        }
      } // if (aElemTypeData->_geomType == ePOLYEDRE )

      // Treat BALLs
      // ----------------
      else if (aElemTypeData->_geomType == eBALL )
      {
        // allocate data arrays
        PBallInfo aBallInfo = myMed->CrBallInfo( aMeshInfo, aElemTypeData->_nbElems );

        elemIterator = myMesh->elementsIterator( SMDSAbs_Ball );
        while ( elemIterator->more() )
        {
          const SMDS_MeshElement* anElem = elemIterator->next();
          // connectivity
          const SMDS_MeshElement* aNode = anElem->GetNode( 0 );
#ifdef _EDF_NODE_IDS_
          (*aBallInfo->myConn)[ iElem ] = aNodeIdMap[FromSmIdType<TInt>(aNode->GetID())];
#else
          (*aBallInfo->myConn)[ iElem ] = aNode->GetID();
#endif
          // element number
          aBallInfo->SetElemNum( iElem, FromSmIdType<TInt>(anElem->GetID()) );

          // diameter
          aBallInfo->myDiameters[ iElem ] =
            static_cast<const SMDS_BallElement*>( anElem )->GetDiameter();

          // family number
          int famNum = getFamilyId( anElemFamMap, anElem, defaultFamilyId );
          aBallInfo->SetFamNum( iElem, famNum );
          ++iElem;
        }
        // store data in a file
        myMed->SetBallInfo(aBallInfo);
      }

      else
      {
        // Treat standard types
        // ---------------------
        // allocate data arrays
        PCellInfo aCellInfo = myMed->CrCellInfo( aMeshInfo,
                                                 aElemTypeData->_entity,
                                                 aElemTypeData->_geomType,
                                                 aElemTypeData->_nbElems,
                                                 theConnMode,
                                                 theIsElemNum,
                                                 theIsElemNames);

        TInt aNbNodes = MED::GetNbNodes(aElemTypeData->_geomType);

        elemIterator = myMesh->elementsIterator( aElemTypeData->_smdsType );
        if ( aElemTypeData->_smdsType == SMDSAbs_0DElement && ! nodesOf0D.empty() )
          elemIterator = iterVecIter;

        while ( elemIterator->more() )
        {
          const SMDS_MeshElement* anElem = elemIterator->next();
          if ( anElem->NbNodes() != aNbNodes || anElem->IsPoly() )
            continue; // other geometry

          // connectivity
          TConnSlice aTConnSlice = aCellInfo->GetConnSlice( iElem );
          for (TInt iNode = 0; iNode < aNbNodes; iNode++) {
            const SMDS_MeshElement* aNode = anElem->GetNode( iNode );
#ifdef _EDF_NODE_IDS_
            aTConnSlice[ iNode ] = aNodeIdMap[FromSmIdType<TInt>(aNode->GetID())];
#else
            aTConnSlice[ iNode ] = aNode->GetID();
#endif
          }
          // element number
          aCellInfo->SetElemNum( iElem, FromSmIdType<TInt>(anElem->GetID()) );

          // family number
          int famNum = getFamilyId( anElemFamMap, anElem, defaultFamilyId );
          aCellInfo->SetFamNum( iElem, famNum );

          if ( ++iElem == aCellInfo->GetNbElem() )
            break;
        }
        // fix numbers of added SMDSAbs_0DElement
        if ( aElemTypeData->_smdsType == SMDSAbs_0DElement && ! nodesOf0D.empty() )
        {
          iElem = myMesh->Nb0DElements();
          TInt elem0DNum = FromSmIdType<TInt>( myMesh->MaxElementID() + 1 );
          for ( size_t i = 0; i < nodesOf0D.size(); ++i )
            aCellInfo->SetElemNum( iElem++, elem0DNum++);
        }

        // store data in a file
        myMed->SetCellInfo(aCellInfo);
      }
    } // loop on geom types
  }
  catch(const std::exception& exc) {
    INFOS("The following exception was caught:\n\t"<<exc.what());
    throw;
  }
  catch(...) {
    INFOS("Unknown exception was caught !!!");
    throw;
  }

  myMeshId = -1;
  myGroups.clear();
  mySubMeshes.clear();
  return aResult;
}

//================================================================================
/*!
 * \brief Returns nodes on VERTEXes where 0D elements are absent
 */
//================================================================================

bool DriverMED_W_SMESHDS_Mesh::
getNodesOfMissing0DOnVert(SMESHDS_Mesh*                         meshDS,
                          std::vector<const SMDS_MeshElement*>& nodes)
{
  nodes.clear();
  for ( int i = 1; i <= meshDS->MaxShapeIndex(); ++i )
  {
    if ( meshDS->IndexToShape( i ).ShapeType() != TopAbs_VERTEX )
      continue;
    if ( SMESHDS_SubMesh* sm = meshDS->MeshElements(i) ) {
      SMDS_NodeIteratorPtr nIt= sm->GetNodes();
      while (nIt->more())
      {
        const SMDS_MeshNode* n = nIt->next();
        if ( n->NbInverseElements( SMDSAbs_0DElement ) == 0 )
          nodes.push_back( n );
      }
    }
  }
  return !nodes.empty();
}
