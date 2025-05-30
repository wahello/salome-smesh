#!/usr/bin/env python3
# Copyright (C) 2013-2025  CEA, EDF
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

import sys,os

pathRacine=os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),'..'))
if os.path.dirname(pathRacine) not in sys.path :
   sys.path.insert(0,pathRacine)

from .dataBase import Base

if __name__ == "__main__":
      from argparse import ArgumentParser
      p=ArgumentParser()
      p.add_argument('-p',dest='partiel',action="store_true", default=False,help='export de machine, groupe, ratio Maille et Perf uniquement')
      p.add_argument('-d',dest='database',default="../myMesh.db",help='nom de la database')
      args = p.parse_args()

      maBase=Base(args.database)
      maBase.initialise()
      maBase.exportToCSV(args.partiel)
      maBase.close()

