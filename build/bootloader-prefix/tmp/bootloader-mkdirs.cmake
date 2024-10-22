# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/berna/esp/v5.3.1/esp-idf/components/bootloader/subproject"
  "D:/Cloud/OneDrive - UNIVALI/2024-2/Projeto de Sistemas Embarcados/M1/Pluviometro Codigo/build/bootloader"
  "D:/Cloud/OneDrive - UNIVALI/2024-2/Projeto de Sistemas Embarcados/M1/Pluviometro Codigo/build/bootloader-prefix"
  "D:/Cloud/OneDrive - UNIVALI/2024-2/Projeto de Sistemas Embarcados/M1/Pluviometro Codigo/build/bootloader-prefix/tmp"
  "D:/Cloud/OneDrive - UNIVALI/2024-2/Projeto de Sistemas Embarcados/M1/Pluviometro Codigo/build/bootloader-prefix/src/bootloader-stamp"
  "D:/Cloud/OneDrive - UNIVALI/2024-2/Projeto de Sistemas Embarcados/M1/Pluviometro Codigo/build/bootloader-prefix/src"
  "D:/Cloud/OneDrive - UNIVALI/2024-2/Projeto de Sistemas Embarcados/M1/Pluviometro Codigo/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/Cloud/OneDrive - UNIVALI/2024-2/Projeto de Sistemas Embarcados/M1/Pluviometro Codigo/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/Cloud/OneDrive - UNIVALI/2024-2/Projeto de Sistemas Embarcados/M1/Pluviometro Codigo/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
