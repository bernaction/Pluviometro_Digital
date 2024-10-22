# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/berna/esp/v5.3.1/esp-idf/components/bootloader/subproject"
  "D:/Pluviometro_Digital/build/bootloader"
  "D:/Pluviometro_Digital/build/bootloader-prefix"
  "D:/Pluviometro_Digital/build/bootloader-prefix/tmp"
  "D:/Pluviometro_Digital/build/bootloader-prefix/src/bootloader-stamp"
  "D:/Pluviometro_Digital/build/bootloader-prefix/src"
  "D:/Pluviometro_Digital/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/Pluviometro_Digital/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/Pluviometro_Digital/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
