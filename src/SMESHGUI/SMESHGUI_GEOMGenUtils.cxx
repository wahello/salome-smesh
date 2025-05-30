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

// SMESH SMESHGUI : GUI for SMESH component
// File   : SMESHGUI_GEOMGenUtils.cxx
// Author : Open CASCADE S.A.S.
// SMESH includes
//
#include "SMESHGUI_GEOMGenUtils.h"
#include "SMESHGUI_Utils.h"
#include "SMESHGUI.h"

// SALOME GEOM includes
#include <GeometryGUI.h>
#include <GEOM_wrap.hxx>

// SALOME KERNEL includes
#include <SALOMEDS_SObject.hxx>

// IDL includes
#include <SALOMEconfig.h>
#include CORBA_CLIENT_HEADER(SMESH_Mesh)

#include <QString>

#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Iterator.hxx>

namespace SMESH
{
  GEOM::GEOM_Gen_var GetGEOMGen( GEOM::GEOM_Object_ptr go )
  {
    GEOM::GEOM_Gen_ptr gen = GEOM::GEOM_Gen::_nil();
    if ( !CORBA::is_nil( go ))
      gen = go->GetGen();
    return gen;
  }

  GEOM::GEOM_Object_var GetShapeOnMeshOrSubMesh(_PTR(SObject) theMeshOrSubmesh,
                                                bool*         isMesh)
  {
    SALOMEDS_SObject* aMeshOrSubmesh = _CAST(SObject,theMeshOrSubmesh);
    if(aMeshOrSubmesh) {
      CORBA::Object_var Obj = aMeshOrSubmesh->GetObject();
      if ( !CORBA::is_nil( Obj ) ) {
        SMESH::SMESH_Mesh_var aMesh =
          SObjectToInterface<SMESH::SMESH_Mesh>( theMeshOrSubmesh );
        if ( !aMesh->_is_nil() )
        {
          if ( isMesh ) *isMesh = true;
          return aMesh->GetShapeToMesh();
        }
        SMESH::SMESH_subMesh_var aSubmesh =
          SObjectToInterface<SMESH::SMESH_subMesh>( theMeshOrSubmesh );
        if ( !aSubmesh->_is_nil() )
        {
          if ( isMesh ) *isMesh = false;
          return aSubmesh->GetSubShape();
        }
      }
    }
    return GEOM::GEOM_Object::_nil();
  }

  GEOM::GEOM_Object_var GetGeom (_PTR(SObject) theSO)
  {
    GEOM::GEOM_Object_var aMeshShape;
    if (!theSO)
      return aMeshShape;

    CORBA::Object_var obj = _CAST( SObject,theSO )->GetObject();
    aMeshShape = GEOM::GEOM_Object::_narrow( obj );
    if ( !aMeshShape->_is_nil() )
      return aMeshShape;

    _PTR(ChildIterator) anIter (SMESH::getStudy()->NewChildIterator(theSO));
    for ( ; anIter->More(); anIter->Next()) {
      _PTR(SObject) aSObject = anIter->Value();
      _PTR(SObject) aRefSOClient;

      if (aSObject->ReferencedObject(aRefSOClient)) {
        SALOMEDS_SObject* aRefSO = _CAST(SObject,aRefSOClient);
        aMeshShape = GEOM::GEOM_Object::_narrow(aRefSO->GetObject());
      } else {
        SALOMEDS_SObject* aSO = _CAST(SObject,aSObject);
        aMeshShape = GEOM::GEOM_Object::_narrow(aSO->GetObject());
      }
      if ( !aMeshShape->_is_nil() )
        return aMeshShape;
    }
    return aMeshShape;
  }

  GEOM::GEOM_Object_var GetGeom( Handle(SALOME_InteractiveObject) io )
  {
    GEOM::GEOM_Object_var go;
    if ( !io.IsNull() && io->hasEntry() )
    {
      _PTR(SObject) so = SMESH::getStudy()->FindObjectID( io->getEntry() );
      go = GetGeom( so );
    }
    return go._retn();
  }

  SMESHGUI_EXPORT char* GetGeomName( _PTR(SObject) smeshSO )
  {
    if (!smeshSO)
      return 0;

    _PTR(ChildIterator) anIter (SMESH::getStudy()->NewChildIterator( smeshSO ));
    for ( ; anIter->More(); anIter->Next()) {
      _PTR(SObject) aSObject = anIter->Value();
      _PTR(SObject) aRefSOClient;
      GEOM::GEOM_Object_var aMeshShape;

      if (aSObject->ReferencedObject(aRefSOClient)) {
        SALOMEDS_SObject* aRefSO = _CAST(SObject,aRefSOClient);
        aMeshShape = GEOM::GEOM_Object::_narrow(aRefSO->GetObject());
        aSObject = aRefSOClient;
      }
      else {
        SALOMEDS_SObject* aSO = _CAST(SObject,aSObject);
        aMeshShape = GEOM::GEOM_Object::_narrow(aSO->GetObject());
      }

      if (!aMeshShape->_is_nil())
      {
        std::string name = aSObject->GetName();
        return CORBA::string_dup( name.c_str() );
      }
    }
    return 0;
  }

  GEOM::GEOM_Object_ptr GetSubShape (GEOM::GEOM_Object_ptr theMainShape,
                                     long                  theID)
  {
    if ( CORBA::is_nil( theMainShape ))
      return GEOM::GEOM_Object::_nil();

    GEOM::GEOM_Gen_var geomGen = SMESH::GetGEOMGen( theMainShape );
    if (geomGen->_is_nil())
      return GEOM::GEOM_Object::_nil();

    GEOM::GEOM_IShapesOperations_wrap aShapesOp = geomGen->GetIShapesOperations();
    if (aShapesOp->_is_nil())
      return GEOM::GEOM_Object::_nil();

    GEOM::GEOM_Object_wrap subShape = aShapesOp->GetSubShape( theMainShape, theID );
    return subShape._retn();
  }

  //================================================================================
  /*!
   * \brief Return entries of sub-mesh geometry and mesh geometry by an IO of assigned
   *        hypothesis
   *  \param [in] hypIO - IO of hyp which is a reference SO to a hyp SO
   *  \param [out] subGeom - found entry of a sub-mesh if any
   *  \param [out] meshGeom - found entry of a mesh
   *  \return bool - \c true if any geometry has been found
   */
  //================================================================================

  bool GetGeomEntries( Handle(SALOME_InteractiveObject)& hypIO,
                       QString&                          subGeom,
                       QString&                          meshGeom )
  {
    subGeom.clear();
    meshGeom.clear();
    if ( hypIO.IsNull() ) return false;

    _PTR(SObject) hypSO = SMESH::getStudy()->FindObjectID( hypIO->getEntry() );
    if ( !hypSO ) return false;

    // Depth() is a number of fathers
    if ( hypSO->Depth() == 4 ) // hypSO is not a reference to a hyp but a hyp it-self
    {
      SMESH::SMESH_Hypothesis_var hyp =
        SMESH::SObjectToInterface< SMESH::SMESH_Hypothesis >( hypSO );
      SMESH::SMESH_Mesh_var mesh;
      GEOM::GEOM_Object_var geom;
      SMESH::SMESH_Gen_var  gen = SMESHGUI::GetSMESHGUI()->GetSMESHGen();
      if ( !gen || !gen->GetSoleSubMeshUsingHyp( hyp, mesh.out(), geom.out() ))
        return false;

      subGeom = toQStr( geom->GetStudyEntry() );

      geom  = mesh->GetShapeToMesh();
      if ( geom->_is_nil() )
        return false;
      meshGeom = toQStr( geom->GetStudyEntry() );
    }
    else
    {
      _PTR(SObject) appliedSO = hypSO->GetFather(); // "Applied hypotheses" folder
      if ( !appliedSO ) return false;

      _PTR(SObject) subOrMeshSO = appliedSO->GetFather(); // mesh or sub-mesh SO
      if ( !subOrMeshSO ) return false;

      bool isMesh;
      GEOM::GEOM_Object_var geom = GetShapeOnMeshOrSubMesh( subOrMeshSO, &isMesh );
      if ( geom->_is_nil() )
        return false;

      if ( isMesh )
      {
        meshGeom = toQStr( geom->GetStudyEntry() );
        return !meshGeom.isEmpty();
      }

      subGeom = toQStr( geom->GetStudyEntry() );

      _PTR(SObject) subFolderSO = subOrMeshSO->GetFather(); // "SubMeshes on ..." folder
      if ( !subFolderSO ) return false;

      _PTR(SObject) meshSO = subFolderSO->GetFather(); // mesh SO
      if ( !meshSO ) return false;

      geom = GetShapeOnMeshOrSubMesh( meshSO );
      if ( geom->_is_nil() )
        return false;

      meshGeom = toQStr( geom->GetStudyEntry() );
    }

    return !meshGeom.isEmpty() && !subGeom.isEmpty();
  }


  //================================================================================
  /*!
   * \brief Return type of shape contained in a group
   */
  //================================================================================

  TopAbs_ShapeEnum _getGroupType(const TopoDS_Shape& group)
  {
    if ( group.ShapeType() != TopAbs_COMPOUND )
      return group.ShapeType();

    // iterate on a compound
    TopoDS_Iterator it( group );
    if ( it.More() )
      return _getGroupType( it.Value() );

    return TopAbs_SHAPE;
  }


  //================================================================================
  /*!
   * \brief Check if a subGeom contains sub-shapes of a mainGeom
   */
  //================================================================================

  bool ContainsSubShape( GEOM::GEOM_Object_ptr mainGeom,
                         GEOM::GEOM_Object_ptr subGeom, bool allowMainShape )
  {
    if ( CORBA::is_nil( mainGeom ) ||
         CORBA::is_nil( subGeom ))
      return false;

    if (allowMainShape && mainGeom->IsSame(subGeom))
      return true;

    GEOM::GEOM_Gen_var geomGen = mainGeom->GetGen();
    if ( geomGen->_is_nil() ) return false;

    GEOM::GEOM_IGroupOperations_wrap op = geomGen->GetIGroupOperations();
    if ( op->_is_nil() ) return false;

    GEOM::GEOM_Object_var mainObj = op->GetMainShape( subGeom ); /* _var not _wrap as
                                                                    mainObj already exists! */
    while ( !mainObj->_is_nil() )
    {
      CORBA::String_var entry1 = mainObj->GetEntry();
      CORBA::String_var entry2 = mainGeom->GetEntry();
      if ( std::string( entry1.in() ) == entry2.in() )
        return true;
      mainObj = op->GetMainShape( mainObj );
    }
    if ( subGeom->GetShapeType() == GEOM::COMPOUND )
    {
      // is subGeom a compound of sub-shapes?
      GEOM::GEOM_IShapesOperations_wrap sop = geomGen->GetIShapesOperations();
      if ( sop->_is_nil() ) return false;
      GEOM::ListOfLong_var ids = sop->GetAllSubShapesIDs( subGeom,
                                                          GEOM::SHAPE,/*sorted=*/false);
      if ( ids->length() > 0 )
      {
        GEOM_Client geomClient;
        TopoDS_Shape  subShape = geomClient.GetShape( geomGen, subGeom );
        TopoDS_Shape mainShape = geomClient.GetShape( geomGen, mainGeom );
        if ( subShape.IsNull() || mainShape.IsNull() )
          return false;

        TopAbs_ShapeEnum subType = _getGroupType( subShape );
        TopTools_IndexedMapOfShape subMap;
        TopExp::MapShapes( subShape, subType, subMap );
        for ( TopExp_Explorer exp( mainShape, subType ); exp.More(); exp.Next() )
          if ( subMap.Contains( exp.Current() ))
            return true;

      }
    }
    return false;
  }

} // end of namespace SMESH
