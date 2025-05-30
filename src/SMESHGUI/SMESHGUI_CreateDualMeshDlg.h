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
// File   : SMESHGUI_CreateDualMeshDlg.h
// Author : Yoann Audouin (EDF)
//
#ifndef SMESHGUI_CREATEDUALMESHDLG_H
#define SMESHGUI_CREATEDUALMESHDLG_H

// SMESH includes
#include "SMESH_SMESHGUI.hxx"

#include "SMESHGUI_Dialog.h"

class QCheckBox;
class QRadioButton;
class QButtonGroup;
class QGroupBox;
class QLabel;
class QLineEdit;

class SMESHGUI_EXPORT SMESHGUI_CreateDualMeshDlg : public SMESHGUI_Dialog
{
  Q_OBJECT

public:
  SMESHGUI_CreateDualMeshDlg();
  virtual ~SMESHGUI_CreateDualMeshDlg();

  void          ShowWarning(bool);
  bool          isWarningShown();

  QLineEdit* myMeshName;
  QCheckBox* myProjShape;

signals:
  void          onClicked( int );


private:
  QLabel* myWarning;
  QLabel* myMeshNameLabel;
};

#endif // SMESHGUI_CREATEDUALMESHDLG_H
