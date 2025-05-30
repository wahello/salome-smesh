# Copyright (C) 2012-2025  CEA, EDF, OPEN CASCADE
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
#
# See http://www.salome-platform.org/ or email : webmaster.salome@opencascade.com
#

INCLUDE(UseOmniORB)  # Provided by KERNEL

INCLUDE_DIRECTORIES(
  ${OMNIORB_INCLUDE_DIR}
  ${KERNEL_INCLUDE_DIRS}
  ${GEOM_INCLUDE_DIRS}
  ${PROJECT_BINARY_DIR}/idl
)

SALOME_CONFIGURE_FILE(SMESH_smIdType.idl.in SMESH_smIdType.idl)

SET(SalomeIDLSMESH_IDLSOURCES
  SMESH_Gen.idl
  SMESH_Mesh.idl
  SMESH_Hypothesis.idl
  SMESH_BasicHypothesis.idl
  SMESH_Filter.idl
  SMESH_Group.idl
  SMESH_Pattern.idl
  SMESH_MeshEditor.idl
  SMESH_Measurements.idl
  ${CMAKE_CURRENT_BINARY_DIR}/SMESH_smIdType.idl
  MG_ADAPT.idl
  SMESH_Homard.idl
)

SET(_idl_include_dirs
  ${KERNEL_ROOT_DIR}/idl/salome
  ${GEOM_ROOT_DIR}/idl/salome
  ${PROJECT_BINARY_DIR}/idl
)

SET(_idl_link_flags
  ${KERNEL_SalomeIDLKernel}
  ${GEOM_SalomeIDLGEOM}
)

OMNIORB_ADD_MODULE(SalomeIDLSMESH "${SalomeIDLSMESH_IDLSOURCES}" "${_idl_include_dirs}" "${_idl_link_flags}")
INSTALL(TARGETS SalomeIDLSMESH EXPORT ${PROJECT_NAME}TargetGroup DESTINATION ${SALOME_INSTALL_LIBS})
