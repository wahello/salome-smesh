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

INCLUDE(tests.set)

SET(TEST_INSTALL_DIRECTORY ${SMESH_TEST_DIR}/other)

# Install 'salome test' staff
# ---------------------------

SALOME_CONFIGURE_FILE(CTestTestfileInstall.cmake
                      ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CTestTestfile.cmake
                      INSTALL ${TEST_INSTALL_DIRECTORY})
INSTALL(FILES tests.set DESTINATION ${TEST_INSTALL_DIRECTORY})
INSTALL(FILES ${GOOD_TESTS} ${BAD_TESTS} ${SESSION_FREE_TESTS} ${OTHER_FILES} ${SMESH_WITH_MG} ${SMESH_WITH_SHAPER} DESTINATION ${TEST_INSTALL_DIRECTORY})
INSTALL(DIRECTORY data DESTINATION ${TEST_INSTALL_DIRECTORY})

# Add tests for 'make test'
# -------------------------

SALOME_GENERATE_TESTS_ENVIRONMENT(_test_env)

FOREACH(_test ${GOOD_TESTS})
  GET_FILENAME_COMPONENT(testname ${_test} NAME_WE)
  SET(testname "TESTS_${testname}")
  ADD_TEST(NAME ${testname}
           COMMAND ${PYTHON_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/test/test_helper.py ${CMAKE_CURRENT_SOURCE_DIR}/${_test})
  SET_TESTS_PROPERTIES(${testname} PROPERTIES ENVIRONMENT "${tests_env}" LABELS "tests")
ENDFOREACH()

INCLUDE_DIRECTORIES( 
  ${PROJECT_SOURCE_DIR}/src/SMESHUtils
  ${PROJECT_SOURCE_DIR}/src/SMDS
  )

FOREACH(_test ${CPP_TESTS})
  GET_FILENAME_COMPONENT(testname ${_test} NAME_WE)
  SET(testname "TESTS_${testname}")
  
  add_executable(${_test} ${_test}.cxx)
  target_link_libraries(${_test} SMESHUtils SMDS )

  ADD_TEST(NAME ${testname}
           COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/${_test} )
  SET_TESTS_PROPERTIES(${testname} PROPERTIES ENVIRONMENT "${tests_env}" LABELS "tests")
ENDFOREACH()

IF(WIN32)
  FOREACH(_test ${CPP_TESTS})
    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/${_test}${CMAKE_EXECUTABLE_SUFFIX} PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ DESTINATION ${TEST_INSTALL_DIRECTORY})
  ENDFOREACH()
ELSE()
  FOREACH(_test ${CPP_TESTS})
    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${_test} PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ DESTINATION ${TEST_INSTALL_DIRECTORY})
  ENDFOREACH()
ENDIF(WIN32)
