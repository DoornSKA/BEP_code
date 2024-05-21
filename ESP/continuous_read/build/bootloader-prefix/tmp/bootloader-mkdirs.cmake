# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/skado/programming/esp/esp-idf/components/bootloader/subproject"
  "/home/skado/bep/BEP_code/ESP/continuous_read/build/bootloader"
  "/home/skado/bep/BEP_code/ESP/continuous_read/build/bootloader-prefix"
  "/home/skado/bep/BEP_code/ESP/continuous_read/build/bootloader-prefix/tmp"
  "/home/skado/bep/BEP_code/ESP/continuous_read/build/bootloader-prefix/src/bootloader-stamp"
  "/home/skado/bep/BEP_code/ESP/continuous_read/build/bootloader-prefix/src"
  "/home/skado/bep/BEP_code/ESP/continuous_read/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/skado/bep/BEP_code/ESP/continuous_read/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/skado/bep/BEP_code/ESP/continuous_read/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
