cmake_minimum_required(VERSION 3.16)

set(EXCLUDE_COMPONENTS "midi_usb" "midi_ethernet")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(MIDI-Cube)