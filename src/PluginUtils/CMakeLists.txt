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

# --- options ---
# additional include directories
INCLUDE_DIRECTORIES(
  ${QT_INCLUDES}
  ${OpenCASCADE_INCLUDE_DIR}
  ${PYTHON_INCLUDE_DIRS}
  ${SALOMEBOOTSTRAP_INCLUDE_DIRS}
  ${KERNEL_INCLUDE_DIRS}
  ${GUI_INCLUDE_DIRS}
  ${GEOM_INCLUDE_DIRS}
  ${MEDFILE_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  ${OMNIORB_INCLUDE_DIR}
  ${PROJECT_SOURCE_DIR}/src/SMESHGUI
  ${PROJECT_BINARY_DIR}/idl
)

# additional preprocessor / compiler flags
ADD_DEFINITIONS(
  ${QT_DEFINITIONS}
  ${OpenCASCADE_DEFINITIONS}
  ${BOOST_DEFINITIONS}
  ${OMNIORB_DEFINITIONS}
)

# libraries to link to
SET(_link_LIBRARIES
  ${KERNEL_SalomeIDLKernel}
  ${SALOMEBOOTSTRAP_SALOMELocalTrace}
  ${KERNEL_SalomeLifeCycleCORBA}
  ${OpenCASCADE_ModelingData_LIBRARIES}
  ${OpenCASCADE_Visualization_LIBRARIES}
  ${QT_LIBRARIES}
  ${QT_MT_LIBS}
  ${GUI_SalomeObject}
  ${GUI_suit}
  SalomeIDLSMESH
  SMESH
)

# --- headers ---

# header files / to be processed by moc
SET(PluginUtils_HEADERS
  GeomSelectionTools.h
  SMESH_PluginUtils.h
  SMESH_AdvOptionsWdg.h
)

# --- sources ---

# sources / moc wrappings
QT_WRAP_MOC(_moc_SOURCES SMESH_AdvOptionsWdg.h)

# sources / static
SET(PluginUtils_SOURCES
  GeomSelectionTools.cxx
  SMESH_AdvOptionsWdg.cxx
  ${_moc_SOURCES}
)

# --- rules ---

ADD_LIBRARY(PluginUtils ${PluginUtils_SOURCES})
TARGET_LINK_LIBRARIES(PluginUtils ${_link_LIBRARIES} )
INSTALL(TARGETS PluginUtils EXPORT ${PROJECT_NAME}TargetGroup DESTINATION ${SALOME_INSTALL_LIBS})

INSTALL(FILES ${PluginUtils_HEADERS} DESTINATION ${SALOME_INSTALL_HEADERS})
