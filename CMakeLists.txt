PROJECT(FreeBTF)

IF(MSVC)
    ADD_DEFINITIONS("/DNOMINMAX /D_SCL_SECURE_NO_WARNINGS /D_CRT_SECURE_NO_WARNINGS /arch:AVX")
ELSE()
	ADD_DEFINITIONS("-msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -mavx")
ENDIF()

ADD_EXECUTABLE(btf-slice btf-slice.cc)