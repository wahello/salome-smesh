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

//  SMESH SMESH_I : idl implementation based on 'SMESH' unit's classes
//  File   : StdMeshers_ImportSource2D_i.hxx
//  Module : SMESH
//
#ifndef _SMESH_ImportSource2D_I_HXX_
#define _SMESH_ImportSource2D_I_HXX_

#include "SMESH_StdMeshers_I.hxx"

#include <SALOMEconfig.h>
#include CORBA_SERVER_HEADER(SMESH_BasicHypothesis)

#include "SMESH_Hypothesis_i.hxx"
#include "StdMeshers_ImportSource.hxx"

class SMESH_Gen;

class STDMESHERS_I_EXPORT StdMeshers_ImportSource2D_i:
  public virtual POA_StdMeshers::StdMeshers_ImportSource2D,
  public virtual SMESH_Hypothesis_i
{
 public:
  // Constructor
  StdMeshers_ImportSource2D_i( PortableServer::POA_ptr thePOA,
                               ::SMESH_Gen*            theGenImpl );
  // Destructor
  virtual ~StdMeshers_ImportSource2D_i();

  void SetSourceFaces(const ::SMESH::ListOfGroups& groups);
  SMESH::string_array*  GetSourceFaces();
  void SetCopySourceMesh(::CORBA::Boolean toCopyMesh, ::CORBA::Boolean toCopyGroups);
  void GetCopySourceMesh(::CORBA::Boolean& toCopyMesh, ::CORBA::Boolean& toCopyGroups);

  // Get implementation
  ::StdMeshers_ImportSource2D* GetImpl();

  // Verify whether hypothesis supports given entity type 
  CORBA::Boolean IsDimSupported( SMESH::Dimension type );

  // Redefined Persistence
  virtual char* SaveTo();
  virtual void  LoadFrom( const char* theStream );
  virtual void  UpdateAsMeshesRestored();



  // Methods for copying mesh definition to other geometry

  // Return geometry this hypothesis depends on. Return false if there is no geometry parameter
  virtual bool getObjectsDependOn( std::vector< std::string > & /*entryArray*/,
                                   std::vector< int >         & /*subIDArray*/ ) const { return 0; }

  // Set new geometry instead of that returned by getObjectsDependOn()
  virtual bool setObjectsDependOn( std::vector< std::string > & /*entryArray*/,
                                   std::vector< int >         & /*subIDArray*/ ) { return true; }
 private:
  SMESH::string_array_var _groupEntries;
  std::vector< std::string > _groupIDs;
};

#endif

