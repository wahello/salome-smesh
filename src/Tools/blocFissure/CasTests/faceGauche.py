# -*- coding: utf-8 -*-
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
"""problème de fissure non plane, débouchante non normale"""

import os
import logging

from blocFissure import gmu
from blocFissure.gmu.geomsmesh import geompy
from blocFissure.gmu.geomsmesh import smesh
from blocFissure.gmu.putName import putName

from blocFissure.gmu.fissureGenerique import fissureGenerique
from blocFissure.gmu.triedreBase import triedreBase
from blocFissure.gmu.genereMeshCalculZoneDefaut import genereMeshCalculZoneDefaut
from blocFissure.gmu.creeZoneDefautDansObjetSain import creeZoneDefautDansObjetSain
from blocFissure.gmu.construitFissureGenerale import construitFissureGenerale

O, OX, OY, OZ = triedreBase()

class faceGauche(fissureGenerique):
  """problème de fissure non plane, débouchante non normale"""

  nomProbleme = "faceGauche"
  shapeFissureParams = dict()
  maillageFissureParams = dict()
  referencesMaillageFissure = dict()

  # ---------------------------------------------------------------------------
  def genereMaillageSain(self, geometriesSaines, meshParams):
    logging.info("genereMaillageSain %s", self.nomCas)

    ([objetSain], _) = smesh.CreateMeshesFromMED(os.path.join(gmu.pathBloc, "materielCasTests", "faceGaucheSain.med"))
    putName(objetSain.GetMesh(), self.nomProbleme, i_pref=self.numeroCas)

    return [objetSain, True] # True : maillage hexa

  # ---------------------------------------------------------------------------
  def setParamShapeFissure(self):
    """
    paramètres de la fissure pour méthode construitFissureGenerale
    lgInfluence : distance autour de la shape de fissure a remailler (A ajuster selon le maillage)
    rayonPipe   : le rayon du pile maillé en hexa autour du fond de fissure
    convexe     : optionnel True : la face est convexe (vue de l'exterieur) sert si on ne donne pas de point interne
    pointIn_x   : optionnel coordonnée x d'un point dans le solide sain (pour orienter la face)
    """
    logging.info("setParamShapeFissure %s", self.nomCas)
    self.shapeFissureParams = dict(lgInfluence = 300,
                                   rayonPipe   = 20)

  # ---------------------------------------------------------------------------
  def genereShapeFissure( self, geometriesSaines, geomParams, shapeFissureParams, \
                                mailleur="MeshGems"):
    logging.info("genereShapeFissure %s", self.nomCas)

    lgInfluence = shapeFissureParams['lgInfluence']

    shellFiss = geompy.ImportBREP(os.path.join(gmu.pathBloc, "materielCasTests", "faceGaucheFiss.brep"))
    fondFiss = geompy.CreateGroup(shellFiss, geompy.ShapeType["EDGE"])
    geompy.UnionIDs(fondFiss, [6])
    geompy.addToStudy( shellFiss, 'shellFiss' )
    geompy.addToStudyInFather( shellFiss, fondFiss, 'fondFiss' )

    mailleur = self.mailleur2d3d()
    coordsNoeudsFissure = genereMeshCalculZoneDefaut(shellFiss, 5 ,50, mailleur, self.numeroCas)

    centre = None
    return [shellFiss, centre, lgInfluence, coordsNoeudsFissure, fondFiss]

  # ---------------------------------------------------------------------------
  def setParamMaillageFissure(self):
    self.maillageFissureParams = dict(nomRep           = os.curdir,
                                      nomFicSain       = self.nomProbleme,
                                      nomFicFissure    = self.nomProbleme + "_fissure",
                                      nbsegRad         = 5,
                                      nbsegCercle      = 8,
                                      areteFaceFissure = 1000)

  # ---------------------------------------------------------------------------
  def genereZoneDefaut(self, geometriesSaines, maillagesSains, shapesFissure, shapeFissureParams, maillageFissureParams):
    elementsDefaut = creeZoneDefautDansObjetSain(geometriesSaines, maillagesSains, shapesFissure, shapeFissureParams, maillageFissureParams, \
                                                 self.numeroCas)
    return elementsDefaut

  # ---------------------------------------------------------------------------
  def genereMaillageFissure(self, geometriesSaines, maillagesSains, \
                                  shapesFissure, shapeFissureParams, \
                                  maillageFissureParams, elementsDefaut, step, \
                                  mailleur="MeshGems"):

    mailleur = self.mailleur2d3d()
    maillageFissure = construitFissureGenerale(shapesFissure, shapeFissureParams, \
                                               maillageFissureParams, elementsDefaut, \
                                               mailleur, self.numeroCas)
    return maillageFissure

  # ---------------------------------------------------------------------------
  def setReferencesMaillageFissure(self):
    from salome.smesh import smeshBuilder
    if smeshBuilder.NETGEN_VERSION_MAJOR < 6:
      self.referencesMaillageFissure = dict( \
                                             Entity_Quad_Quadrangle = 4177, \
                                             Entity_Quad_Hexa = 6224, \
                                             Entity_Node = 74848, \
                                             Entity_Quad_Edge = 749, \
                                             Entity_Quad_Triangle = 2338, \
                                             Entity_Quad_Tetra = 28628, \
                                             Entity_Quad_Pyramid = 1236, \
                                             Entity_Quad_Penta = 936 \
      )
    else:
      self.referencesMaillageFissure = dict( \
                                             Entity_Quad_Quadrangle = 4177, \
                                             Entity_Quad_Hexa = 6224, \
                                             Entity_Node = 76374, \
                                             Entity_Quad_Edge = 750, \
                                             Entity_Quad_Triangle = 2434, \
                                             Entity_Quad_Tetra = 29698, \
                                             Entity_Quad_Pyramid = 1236, \
                                             Entity_Quad_Penta = 936 \
      )
