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
  ${KERNEL_INCLUDE_DIRS}
  ${GUI_INCLUDE_DIRS}
  ${MEDFILE_INCLUDE_DIRS}
  ${QWT_INCLUDE_DIR}
  ${GEOM_INCLUDE_DIRS}
  ${OpenCASCADE_INCLUDE_DIR}
  ${OMNIORB_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}/src/Controls
  ${PROJECT_SOURCE_DIR}/src/SMDS
  ${PROJECT_SOURCE_DIR}/src/SMESHDS
  ${PROJECT_SOURCE_DIR}/src/Driver
  ${PROJECT_SOURCE_DIR}/src/DriverMED
  ${PROJECT_SOURCE_DIR}/src/SMESH
  ${PROJECT_SOURCE_DIR}/src/SMESHClient
  ${PROJECT_SOURCE_DIR}/src/SMESHUtils
  ${PROJECT_BINARY_DIR}/idl
)

# additional preprocessor / compiler flags
ADD_DEFINITIONS(
  ${OpenCASCADE_DEFINITIONS}
  ${BOOST_DEFINITIONS}
  ${OMNIORB_DEFINITIONS}
)

# libraries to link to
SET(_link_LIBRARIES
  ${QT_LIBRARIES}
  ${OpenCASCADE_KERNEL}
  ${GEOM_GEOMObject}
  ${GUI_SalomeObject}
  ${GUI_SVTK}
  ${GUI_VTKViewer}
  ${GUI_qtx}
  ${GUI_suit}
  ${GUI_std}
  ${GUI_SalomeApp}
  SMESHClient
  SMDS
  SMESHControls
  VTK::RenderingLabel
)

IF(SALOME_USE_PLOT2DVIEWER)
  LIST(APPEND _link_LIBRARIES
    ${GUI_Plot2d}
    ${GUI_SPlot2d}
    )
ENDIF()

# --- headers ---

# header files / no moc processing
SET(SMESHObject_HEADERS
  SMESH_Actor.h
  SMESH_Object.h
  SMESH_ObjectDef.h
  SMESH_ActorProps.h
  SMESH_ActorUtils.h
  SMESH_ActorDef.h
  SMESH_DeviceActor.h
  SMESH_PreviewActorsCollection.h
  SMESH_ExtractGeometry.h
  SMESH_FaceOrientationFilter.h
  SMESH_ScalarBarActor.h
  SMESH_NodeLabelActor.h
  SMESH_CellLabelActor.h
  SMESH_SVTKActor.h 
)

# --- sources ---

# sources / static
SET(SMESHObject_SOURCES
  SMESH_Object.cxx
  SMESH_DeviceActor.cxx
  SMESH_Actor.cxx
  SMESH_ActorProps.cxx
  SMESH_PreviewActorsCollection.cxx
  SMESH_ExtractGeometry.cxx
  SMESH_ActorUtils.cxx
  SMESH_FaceOrientationFilter.cxx
  SMESH_ScalarBarActor.cxx
  SMESH_NodeLabelActor.cxx
  SMESH_CellLabelActor.cxx
  SMESH_SVTKActor.cxx 
)

# --- rules ---

ADD_LIBRARY(SMESHObject ${SMESHObject_SOURCES})
TARGET_LINK_LIBRARIES(SMESHObject ${_link_LIBRARIES} )
INSTALL(TARGETS SMESHObject EXPORT ${PROJECT_NAME}TargetGroup DESTINATION ${SALOME_INSTALL_LIBS})

INSTALL(FILES ${SMESHObject_HEADERS} DESTINATION ${SALOME_INSTALL_HEADERS})
