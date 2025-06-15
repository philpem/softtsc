
#find_package(PulseAudio REQUIRED)

#include_directories( ${PULSEAUDIO_INCLUDE_DIR} )

add_library(mskmodem sound_jack.c mskmodem.c)

set_target_properties( mskmodem PROPERTIES COMPILE_FLAGS -fPIC)

target_link_libraries( mskmodem 
                       m #Math
                       jack
                       ${GLIB2_LIBRARIES} 
                       #${PULSEAUDIO_LIBRARY} 
                       #${PULSEAUDIO_MAINLOOP_LIBRARY} 
                       )

