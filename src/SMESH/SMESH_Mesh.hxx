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

//  File   : SMESH_Mesh.hxx
//  Author : Paul RASCLE, EDF
//  Module : SMESH
//
#ifndef _SMESH_MESH_HXX_
#define _SMESH_MESH_HXX_

#include "SMESH_SMESH.hxx"

#include "SMDSAbs_ElementType.hxx"
#include "SMESH_ComputeError.hxx"
#include "SMESH_Controls.hxx"
#include "SMESH_Hypothesis.hxx"
#include "SMESH_subMesh.hxx"
#include "SMDS_Iterator.hxx"

#include "Utils_SALOME_Exception.hxx"

#include <TopoDS_Shape.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_ListOfShape.hxx>

#include "MEDCouplingMemArray.hxx"

#include <map>
#include <list>
#include <vector>
#include <ostream>

#ifndef WIN32
#include <boost/filesystem.hpp>
#endif
#include <boost/thread.hpp>

#ifdef WIN32
#pragma warning(disable:4251) // Warning DLL Interface ...
#pragma warning(disable:4290) // Warning Exception ...
#endif

class SMESHDS_Command;
class SMESHDS_Document;
class SMESHDS_GroupBase;
class SMESHDS_Hypothesis;
class SMESHDS_Mesh;
class SMESH_Gen;
class SMESH_Group;
class SMESH_HypoFilter;
class SMESH_subMesh;
class TopoDS_Solid;

class DriverMED_W_SMESHDS_Mesh;

typedef std::set<int> TSetOfInt;
typedef std::list<int> TListOfInt;
typedef std::list<TListOfInt> TListOfListOfInt;

class SMESH_EXPORT SMESH_Mesh
{
 public:
  SMESH_Mesh(int               theLocalId,
             SMESH_Gen*        theGen,
             bool              theIsEmbeddedMode,
             SMESHDS_Document* theDocument);

  virtual ~SMESH_Mesh();

  /*!
   * \brief Set geometry to be meshed
   */
  void ShapeToMesh(const TopoDS_Shape & aShape);
  /*!
   * \brief Return geometry to be meshed. (It may be a PseudoShape()!)
   */
  TopoDS_Shape GetShapeToMesh() const;
  /*!
   * \brief Return true if there is a geometry to be meshed, not PseudoShape()
   */
  bool HasShapeToMesh() const { return _isShapeToMesh; }

  void UndefShapeToMesh() { _isShapeToMesh = false; }

  /*!
   * \brief Return diagonal size of bounding box of shape to mesh.
   */
  double GetShapeDiagonalSize() const;
  /*!
   * \brief Return diagonal size of bounding box of a shape.
   */
  static double GetShapeDiagonalSize(const TopoDS_Shape & aShape);
  /*!
   * \brief Return a solid which is returned by GetShapeToMesh() if
   *        a real geometry to be meshed was not set
   */
  static const TopoDS_Solid& PseudoShape();

  /*!
   * \brief Load mesh from study file
   */
  void Load();
  /*!
   * \brief Remove all nodes and elements
   */
  void Clear();
  /*!
   * \brief Remove all nodes and elements of indicated shape
   */
  void ClearSubMesh(const int theShapeId);

  /*!
   * consult DriverMED_R_SMESHDS_Mesh::ReadStatus for returned value
   */
  int UNVToMesh(const char* theFileName);

  int MEDToMesh(const char* theFileName, const char* theMeshName);

  std::string STLToMesh(const char* theFileName);

  int CGNSToMesh(const char* theFileName, const int theMeshIndex, std::string& theMeshName);

  SMESH_ComputeErrorPtr GMFToMesh(const char* theFileName,
                                  bool        theMakeRequiredGroups = true );

  SMESH_Hypothesis::Hypothesis_Status
  AddHypothesis(const TopoDS_Shape & aSubShape, int anHypId, std::string* error=0);

  SMESH_Hypothesis::Hypothesis_Status
  RemoveHypothesis(const TopoDS_Shape & aSubShape, int anHypId);

  const std::list <const SMESHDS_Hypothesis * >&
  GetHypothesisList(const TopoDS_Shape & aSubShape) const;

  const SMESH_Hypothesis * GetHypothesis(const TopoDS_Shape &    aSubShape,
                                         const SMESH_HypoFilter& aFilter,
                                         const bool              andAncestors,
                                         TopoDS_Shape*           assignedTo=0) const;

  int GetHypotheses(const TopoDS_Shape &                     aSubShape,
                    const SMESH_HypoFilter&                  aFilter,
                    std::list< const SMESHDS_Hypothesis * >& aHypList,
                    const bool                               andAncestors,
                    std::list< TopoDS_Shape > *              assignedTo=0) const;

  const SMESH_Hypothesis * GetHypothesis(const SMESH_subMesh *   aSubMesh,
                                         const SMESH_HypoFilter& aFilter,
                                         const bool              andAncestors,
                                         TopoDS_Shape*           assignedTo=0) const;

  int GetHypotheses(const SMESH_subMesh *                    aSubMesh,
                    const SMESH_HypoFilter&                  aFilter,
                    std::list< const SMESHDS_Hypothesis * >& aHypList,
                    const bool                               andAncestors,
                    std::list< TopoDS_Shape > *              assignedTo=0) const;

  SMESH_Hypothesis * GetHypothesis(const int aHypID) const;

  SMESH_Hypothesis::Hypothesis_Status CheckHypothesesOnSubMeshes(
    SMESH_subMesh* subMesh,
    const SMESH_Hypothesis* anHyp,
    const SMESH_subMesh::algo_event event) const;

  const std::list<SMESHDS_Command*> & GetLog();

  void ClearLog();

  int GetId() const          { return _id; }

  bool MeshExists( int meshId ) const;

  SMESH_Mesh* FindMesh( int meshId ) const;

  SMESHDS_Mesh * GetMeshDS() { return _meshDS; }

  const SMESHDS_Mesh * GetMeshDS() const { return _meshDS; }

  SMESH_Gen *GetGen()        { return _gen; }

  SMESH_subMesh *GetSubMesh(const TopoDS_Shape & aSubShape);

  SMESH_subMesh *GetSubMeshContaining(const TopoDS_Shape & aSubShape) const;

  SMESH_subMesh *GetSubMeshContaining(const int aShapeID) const;
  /*!
   * \brief Return submeshes of groups containing the given subshape
   */
  std::list<SMESH_subMesh*> GetGroupSubMeshesContaining(const TopoDS_Shape & shape) const;
  /*!
   * \brief Say all submeshes that theChangedHyp has been modified
   */
  void NotifySubMeshesHypothesisModification(const SMESH_Hypothesis* theChangedHyp);

  // const std::list < SMESH_subMesh * >&
  // GetSubMeshUsingHypothesis(SMESHDS_Hypothesis * anHyp) throw(SALOME_Exception);
  /*!
   * \brief Return True if anHyp is used to mesh aSubShape
   */
  bool IsUsedHypothesis(SMESHDS_Hypothesis *  anHyp,
                        const SMESH_subMesh * aSubMesh);
  /*!
   * \brief check if a hypothesis allowing notconform mesh is present
   */
  bool IsNotConformAllowed() const;

  bool IsMainShape(const TopoDS_Shape& theShape) const;

  TopoDS_Shape GetShapeByEntry(const std::string& entry) const;

  /*!
   * \brief Return list of ancestors of theSubShape in the order
   *        that lower dimension shapes come first
   */
  const TopTools_ListOfShape& GetAncestors(const TopoDS_Shape& theSubShape) const;

  void SetAutoColor(bool theAutoColor);

  bool GetAutoColor();

  /*!
   * \brief Set the flag meaning that the mesh has been edited "manually".
   * It is to set to false after Clear() and to set to true by MeshEditor
   */
  void SetIsModified(bool isModified);

  bool GetIsModified() const { return _isModified; }

  /*!
   * \brief Return true if the mesh has been edited since a total re-compute
   *        and those modifications may prevent successful partial re-compute.
   *        As a side effect reset _isModified flag if mesh is empty
   */
  bool HasModificationsToDiscard() const;

  /*!
   * \brief Return true if all sub-meshes are computed OK - to update an icon
   */
  bool IsComputedOK();

  /*!
   * \brief Return data map of descendant to ancestor shapes
   */
  typedef TopTools_IndexedDataMapOfShapeListOfShape TAncestorMap;
  const TAncestorMap& GetAncestorMap() const { return _mapAncestors; }

  /*!
   * \brief Check group names for duplications.
   *  Consider maximum group name length stored in MED file
   */
  bool HasDuplicatedGroupNamesMED();

  /*!
   * \brief Exception thrown by Export*() in case if a mesh is too large for export
   *        due to limitation of a format
   */
  struct TooLargeForExport : public std::runtime_error
  {
    TooLargeForExport(const char* format):runtime_error(format) {}
  };

  MEDCoupling::MCAuto<MEDCoupling::DataArrayByte>
    ExportMEDCoupling(const char*         theMeshName = NULL,
                      bool                theAutoGroups = true,
                      const SMESHDS_Mesh* theMeshPart = 0,
                      bool                theAutoDimension = false,
                      bool                theAddODOnVertices = false,
                      double              theZTolerance = -1.,
                      bool                theSaveNumbers = true);

  void ExportMED(const char *        theFile,
                 const char*         theMeshName = NULL,
                 bool                theAutoGroups = true,
                 int                 theVersion = -1,
                 const SMESHDS_Mesh* theMeshPart = 0,
                 bool                theAutoDimension = false,
                 bool                theAddODOnVertices = false,
                 double              theZTolerance = -1.,
                 bool                theSaveNumbers = true);

  void ExportDAT(const char *        file,
                 const SMESHDS_Mesh* meshPart = 0,
                 const bool          renumber = true);
  void ExportUNV(const char *        file,
                 const SMESHDS_Mesh* meshPart = 0,
                 const bool          renumber = true);
  void ExportSTL(const char *        file,
                 const bool          isascii,
                 const char *        name = 0,
                 const SMESHDS_Mesh* meshPart = 0);
  void ExportCGNS(const char *        file,
                  const SMESHDS_Mesh* mesh,
                  const char *        meshName = 0,
                  const bool          groupElemsByType = false);
  void ExportStructuredCGNS(const char *        file,
                            const SMESHDS_Mesh* mesh,
                            const char *        meshName = 0);
  void ExportGMF(const char *        file,
                 const SMESHDS_Mesh* mesh,
                 bool                withRequiredGroups = true );

  double GetComputeProgress() const;

  smIdType NbNodes() const;
  smIdType Nb0DElements() const;
  smIdType NbBalls() const;

  smIdType NbEdges(SMDSAbs_ElementOrder order = ORDER_ANY) const;

  smIdType NbFaces(SMDSAbs_ElementOrder order = ORDER_ANY) const;
  smIdType NbTriangles(SMDSAbs_ElementOrder order = ORDER_ANY) const;
  smIdType NbQuadrangles(SMDSAbs_ElementOrder order = ORDER_ANY) const;
  smIdType NbBiQuadQuadrangles() const;
  smIdType NbBiQuadTriangles() const;
  smIdType NbPolygons(SMDSAbs_ElementOrder order = ORDER_ANY) const;

  smIdType NbVolumes(SMDSAbs_ElementOrder order = ORDER_ANY) const;
  smIdType NbTetras(SMDSAbs_ElementOrder order = ORDER_ANY) const;
  smIdType NbHexas(SMDSAbs_ElementOrder order = ORDER_ANY) const;
  smIdType NbTriQuadraticHexas() const;
  smIdType NbPyramids(SMDSAbs_ElementOrder order = ORDER_ANY) const;
  smIdType NbPrisms(SMDSAbs_ElementOrder order = ORDER_ANY) const;
  smIdType NbQuadPrisms() const;
  smIdType NbBiQuadPrisms() const;
  smIdType NbHexagonalPrisms() const;
  smIdType NbPolyhedrons() const;

  smIdType NbSubMesh() const;

  size_t NbGroup() const { return _mapGroup.size(); }

  int NbMeshes() const; // nb meshes in the Study

  SMESH_Group* AddGroup (const SMDSAbs_ElementType theType,
                         const char*               theName,
                         const int                 theId = -1,
                         const TopoDS_Shape&       theShape = TopoDS_Shape(),
                         const SMESH_PredicatePtr& thePredicate = SMESH_PredicatePtr());

  SMESH_Group* AddGroup (SMESHDS_GroupBase* groupDS);

  typedef boost::shared_ptr< SMDS_Iterator<SMESH_Group*> > GroupIteratorPtr;
  GroupIteratorPtr GetGroups() const;

  std::list<int> GetGroupIds() const;

  SMESH_Group* GetGroup (const int theGroupID) const;

  bool RemoveGroup (const int theGroupID);

  SMESH_Group* ConvertToStandalone ( int theGroupID );

  struct TCallUp // callback from SMESH to SMESH_I level
  {
    virtual void RemoveGroup( const int theGroupID )=0;
    virtual void HypothesisModified( int hypID, bool updateIcons )=0;
    virtual void Load()=0;
    virtual bool IsLoaded()=0;
    virtual TopoDS_Shape GetShapeByEntry(const std::string& entry)=0;
    virtual ~TCallUp() {}
  };
  void SetCallUp( TCallUp * upCaller );

  bool SynchronizeGroups();


  SMDSAbs_ElementType GetElementType( const smIdType id, const bool iselem );

  void ClearMeshOrder();
  void SetMeshOrder(const TListOfListOfInt& theOrder );
  const TListOfListOfInt& GetMeshOrder() const;

  // sort submeshes according to stored mesh order
  bool SortByMeshOrder(std::vector<SMESH_subMesh*>& theListToSort) const;

  // return true if given order of sub-meshes is OK
  bool IsOrderOK( const SMESH_subMesh* smBefore,
                  const SMESH_subMesh* smAfter ) const;

  std::ostream& Dump(std::ostream & save);

  // Parallel computation functions

  virtual void Lock(){};
  virtual void Unlock(){};

  virtual void wait(){};

  virtual bool IsParallel(){throw SALOME_Exception("Calling SMESH_Mesh::IsParallel");return false;};
  virtual int GetParallelElement(){throw SALOME_Exception("Calling SMESH_Mesh::GetParallelElement");return 0;};

  virtual bool ComputeSubMeshes(
            SMESH_Gen* gen,
            SMESH_Mesh & aMesh,
            const TopoDS_Shape & aShape,
            const ::MeshDimension       aDim,
            TSetOfInt*                  aShapesId /*=0*/,
            TopTools_IndexedMapOfShape* allowedSubShapes,
            SMESH_subMesh::compute_event &computeEvent,
            const bool includeSelf,
            const bool complexShapeFirst,
            const bool aShapeOnly){(void) gen;(void) aMesh;(void) aShape;(void) aDim;(void) aShapesId;(void) allowedSubShapes;(void) computeEvent;(void) includeSelf;(void) complexShapeFirst;(void) aShapeOnly;throw SALOME_Exception("Calling SMESH_Mesh::ComputeSubMeshes");return false;};

private:

  void exportMEDCommmon(DriverMED_W_SMESHDS_Mesh& myWriter,
                        const char*               theMeshName,
                        bool                      theAutoGroups,
                        const SMESHDS_Mesh*       meshPart,
                        bool                      theAutoDimension,
                        bool                      theAddODOnVertices,
                        double                    theZTolerance,
                        bool                      theSaveNumbers);

 private:
  void fillAncestorsMap(const TopoDS_Shape& theShape);
  void getAncestorsSubMeshes(const TopoDS_Shape&            theSubShape,
                             std::vector< SMESH_subMesh* >& theSubMeshes) const;

protected:
  int                        _id;           // id given by creator (unique within the creator instance)
  int                        _groupId;      // id generator for group objects
  int                        _nbSubShapes;  // initial nb of subshapes in the shape to mesh
  bool                       _isShapeToMesh;// set to true when a shape is given (only once)
  SMESHDS_Document *         _document;
  SMESHDS_Mesh *             _meshDS;
  SMESH_Gen *                _gen;
  std::map <int, SMESH_Group*> _mapGroup;

  class SubMeshHolder;
  SubMeshHolder*             _subMeshHolder;

  bool                       _isAutoColor;
  bool                       _isModified; //!< modified since last total re-compute, issue 0020693

  double                     _shapeDiagonal; //!< diagonal size of bounding box of shape to mesh

  TopTools_IndexedDataMapOfShapeListOfShape _mapAncestors;

  mutable std::vector<SMESH_subMesh*> _ancestorSubMeshes; // to speed up GetHypothes[ei]s()

  TListOfListOfInt           _subMeshOrder;

  // Struct calling methods at CORBA API implementation level, used to
  // 1) make an upper level (SMESH_I) be consistent with a lower one (SMESH)
  // when group removal is invoked by hyp modification (issue 0020918)
  // 2) to forget not loaded mesh data at hyp modification
  TCallUp*                    _callUp;

protected:
  SMESH_Mesh();
  SMESH_Mesh(const SMESH_Mesh&) {};
};
#endif
