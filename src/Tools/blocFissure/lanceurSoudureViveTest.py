# -*- coding: iso-8859-1 -*-
# Copyright (C) 2014-2025  CEA, EDF
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

import sys
import salome

import os
from blocFissure import gmu
from blocFissure.gmu import initLog
#initLog.setDebug()
initLog.setVerbose()

from blocFissure.casStandard import casStandard

mesh = 'areteViveSoudure.med'
crack = 'fissureSoudureTest.brep'

dicoParams = dict(nomCas            = 'casTestCoinTriple',
                  maillageSain      = '/local00/home/I48174/Documents/soudure/essaiFissure/{0}'.format(mesh),
                  CAOFaceFissure    = '/local00/home/I48174/Documents/soudure/essaiFissure/{0}'.format(crack),
                  edgeFiss          = [4],
                  lgInfluence       = 30,
                  meshBrep          = (5,10),
                  rayonPipe         = 10,
                  lenSegPipe        = 7,
                  nbSegRad          = 5,
                  nbSegCercle       = 10,
                  areteFaceFissure  = 10)

execInstance = casStandard(dicoParams)

if salome.sg.hasDesktop():
  salome.sg.updateObjBrowser()
