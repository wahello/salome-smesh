# Copyright (C) 2012-2025  CEA, EDF
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

SALOME_CONFIGURE_FILE(conf.py.in conf.py)

SET(_cmd_options -c ${CMAKE_CURRENT_BINARY_DIR} -b html -d doctrees ${CMAKE_CURRENT_SOURCE_DIR} docutils)
SALOME_GENERATE_ENVIRONMENT_SCRIPT(_cmd env_script "${SPHINX_EXECUTABLE}" "${_cmd_options}" CONTEXT ZCracksPlug_DOC)
ADD_CUSTOM_TARGET(html_docs_ZCracksPlug COMMAND ${_cmd})

INSTALL(CODE "EXECUTE_PROCESS(COMMAND \"${CMAKE_COMMAND}\" --build ${PROJECT_BINARY_DIR} --target html_docs_ZCracksPlug)")
INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docutils/ DESTINATION ${SALOME_INSTALL_DOC}/gui/SMESH/zcracks)

SET(make_clean_files docutils doctrees)
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${make_clean_files}")
