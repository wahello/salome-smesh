// Copyright (C) 2007-2025  CEA, EDF, OPEN CASCADE
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
// File   : SMESHGUI_FileValidator.h
// Author : Oleg UVAROV
//
#ifndef SMESHGUI_FILEVALIDATOR_H
#define SMESHGUI_FILEVALIDATOR_H

// SMESH includes
#include "SMESH_SMESHGUI.hxx"

// SALOME GUI includes
#include <SUIT_FileValidator.h>

class SMESHGUI_EXPORT SMESHGUI_FileValidator : public SUIT_FileValidator
{
public:
  SMESHGUI_FileValidator( QWidget* = 0 );
  
  virtual bool canSave( const QString&, bool = true );

  bool         isOverwrite( const QString& fileName) const;

private:
  bool         myIsOverwrite;
};

#endif // SMESHGUI_FILEVALIDATOR_H
