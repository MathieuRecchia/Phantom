# Install script for directory: D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-src

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "D:/Dev/Phantom/out/install/x64-Debug")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-build/modules/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-build/extras/Build/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/JUCE-9.0.1" TYPE FILE FILES
    "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-build/JUCEConfigVersion.cmake"
    "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-build/JUCEConfig.cmake"
    "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-src/extras/Build/CMake/FindCppwinrt.cmake"
    "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-src/extras/Build/CMake/FindWebView2.cmake"
    "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-src/extras/Build/CMake/FindWindowsMIDIServices.cmake"
    "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-src/extras/Build/CMake/JUCECheckAtomic.cmake"
    "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-src/extras/Build/CMake/JUCEHelperTargets.cmake"
    "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-src/extras/Build/CMake/JUCEModuleSupport.cmake"
    "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-src/extras/Build/CMake/JUCEUtils.cmake"
    "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-src/extras/Build/CMake/JuceLV2Defines.h.in"
    "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-src/extras/Build/CMake/LaunchScreen.storyboard"
    "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-src/extras/Build/CMake/PIPAudioProcessor.cpp.in"
    "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-src/extras/Build/CMake/PIPAudioProcessorWithARA.cpp.in"
    "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-src/extras/Build/CMake/PIPComponent.cpp.in"
    "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-src/extras/Build/CMake/PIPConsole.cpp.in"
    "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-src/extras/Build/CMake/RecentFilesMenuTemplate.nib"
    "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-src/extras/Build/CMake/UnityPluginGUIScript.cs.in"
    "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-src/extras/Build/CMake/bundleplaceholder.mm"
    "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-src/extras/Build/CMake/checkBundleSigning.cmake"
    "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-src/extras/Build/CMake/copyDir.cmake"
    "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-src/extras/Build/CMake/juce_LinuxSubprocessHelper.cpp"
    "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-src/extras/Build/CMake/juce_runtime_arch_detection.cpp"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/JUCE-9.0.1" TYPE DIRECTORY FILES "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-src/extras/Build/CMake/juce_vst3_helper")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "D:/Dev/Phantom/out/build/x64-Debug/_deps/juce-build/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
