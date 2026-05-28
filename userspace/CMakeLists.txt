cmake_minimum_required(VERSION 3.16)
PROJECT (CTRL_PAiNEL)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(Curses REQUIRED)


SET(APP_SRCS controlHID.c )

foreach(ggfile ${APP_SRCS}) 
	string( REPLACE ".c" "" GGNAME ${ggfile} )
	ADD_EXECUTABLE (${GGNAME} ${ggfile})

	target_include_directories(${GGNAME} PRIVATE ${CURSES_INCLUDE_DIR})
	target_link_libraries(${GGNAME}  PRIVATE ${CURSES_LIBRARIES})

endforeach(ggfile ${APP_SRCS})


