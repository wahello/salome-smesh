#  -*- coding: iso-8859-1 -*-
# Copyright (C) 2007-2025  CEA, EDF, OPEN CASCADE
#
# Copyright (C) 2003-2007  OPEN CASCADE, EADS/CCR, LIP6, CEA/DEN,
# CEDRAT, EDF R&D, LEG, PRINCIPIA R&D, BUREAU VERITAS
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

def BuildGroupLyingOn(theMesh, theElemType, theName, theShape):
    aFilterMgr = smesh.CreateFilterManager()
    aFilter = aFilterMgr.CreateFilter()
   
    aLyingOnGeom = aFilterMgr.CreateLyingOnGeom()
    aLyingOnGeom.SetGeom(theShape)
    aLyingOnGeom.SetElementType(theElemType)
    
    aFilter.SetPredicate(aLyingOnGeom)
    anIds = aFilter.GetElementsId(theMesh)
    aFilterMgr.UnRegister()

    aGroup = theMesh.CreateGroup(theElemType, theName)
    aGroup.Add(anIds)

#Example
from SMESH_test1 import *

isDone = mesh.Compute()
if not isDone:
    raise Exception("Error when computing Mesh")

# First way
BuildGroupLyingOn(mesh.GetMesh(), SMESH.FACE, "Group of faces lying on edge #1", edge )

# Second way
mesh.MakeGroup("Group of faces lying on edge #2", SMESH.FACE, SMESH.FT_LyingOnGeom, edge)

salome.sg.updateObjBrowser()
