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

//  File   : SMESH_ParallelMesh.hxx
//  Author : Yoann AUDOUIN, EDF
//  Module : SMESH
//
#ifndef _SMESH_PARALLELMESH_HXX_
#define _SMESH_PARALLELMESH_HXX_

#include "SMESH_Mesh.hxx"

#ifndef WIN32
#include <boost/asio.hpp>
#endif

#include "SMESH_Gen.hxx"
#include "SMESH_subMesh.hxx"
#ifdef WIN32
#include <thread>
#include <boost/filesystem.hpp>
#endif
enum ParallelismMethod {MultiThread, MultiNode};

class SMESH_EXPORT SMESH_ParallelMesh: public SMESH_Mesh
{
 public:
  SMESH_ParallelMesh(int               theLocalId,
                       SMESH_Gen*        theGen,
                       bool              theIsEmbeddedMode,
                       SMESHDS_Document* theDocument);

  ~SMESH_ParallelMesh();

  // Locking mechanism
  #ifndef WIN32
  void Lock() override {_my_lock.lock();};
  void Unlock() override {_my_lock.unlock();};
  // We need to recreate the pool afterthe join
  void wait() override {_pool->join(); DeletePoolThreads(); InitPoolThreads(); };
  #endif

  // Thread Pool
#ifndef WIN32
  void InitPoolThreads() {_pool = new boost::asio::thread_pool(GetPoolNbThreads());};
  boost::asio::thread_pool* GetPool() {return _pool;};
  void DeletePoolThreads() {delete _pool;};
#else
  void InitPoolThreads() {};
  void* GetPool() {return NULL;};
  void DeletePoolThreads(){};
#endif

  int GetPoolNbThreads();

  // Temporary folder
  bool keepingTmpFolfer();
  void CreateTmpFolder();
  void DeleteTmpFolder();
  boost::filesystem::path GetTmpFolder() {return tmp_folder;};
  void cleanup();

  //
  bool IsParallel() override {return true;};
  int GetParallelElement() override;
  int GetDumpElement();

  // Parallelims parameters
  int GetParallelismMethod() {return _method;};
  void SetParallelismMethod(int aMethod) {_method = aMethod;};

  int GetParallelismDimension() {return _paraDim;};
  void SetParallelismDimension(int aDim) {_paraDim = aDim;};

  // Multithreading parameters
  int GetNbThreads() {return _NbThreads;};
  void SetNbThreads(long nbThreads);

  // Multinode parameters
  std::string GetResource() {return _resource;};
  void SetResource(std::string aResource) {_resource = aResource;};

  int GetNbProc() {return _nbProc;};
  void SetNbProc(long nbProc) {_nbProc = nbProc;};

  int GetNbProcPerNode() {return _nbProcPerNode;};
  void SetNbProcPerNode(long nbProcPerNodes) {_nbProcPerNode = nbProcPerNodes;};

  int GetNbNode() {return _nbNode;};
  void SetNbNode(long nbNodes) {_nbNode = nbNodes;};

  std::string GetWcKey() {return _wcKey;};
  void SetWcKey(std::string wcKey) {_wcKey = wcKey;};

  std::string GetWalltime() {return _walltime;};
  void SetWalltime(std::string walltime) {_walltime = walltime;};

  // Parallel computation
  bool ComputeSubMeshes(
            SMESH_Gen* gen,
            SMESH_Mesh & aMesh,
            const TopoDS_Shape & aShape,
            const ::MeshDimension       aDim,
            TSetOfInt*                  aShapesId /*=0*/,
            TopTools_IndexedMapOfShape* allowedSubShapes,
            SMESH_subMesh::compute_event &computeEvent,
            const bool includeSelf,
            const bool complexShapeFirst,
            const bool   aShapeOnly) override;

 protected:
  SMESH_ParallelMesh():SMESH_Mesh() {};
  SMESH_ParallelMesh(const SMESH_ParallelMesh& aMesh):SMESH_Mesh(aMesh) {};
 private:
  // Mutex for multhitreading write in SMESH_Mesh
#ifndef WIN32
  boost::mutex _my_lock;
  // thread pool for computation
  boost::asio::thread_pool *     _pool = nullptr;
#endif
  boost::filesystem::path tmp_folder;
  int _method = ParallelismMethod::MultiThread;
  int _paraDim = 3;

  int _NbThreads = std::thread::hardware_concurrency();

  int _nbProc = 1;
  int _nbProcPerNode = 1;
  int _nbNode = 1;
  std::string _resource = "";
  std::string _wcKey = "P11N0:SALOME";
  std::string _walltime = "01:00:00";
};
#endif
