find_package(PkgConfig)

pkg_check_modules(uring QUIET liburing.so)
set(URING_DEFINITIONS ${URING_CFLAGS_OTHER})

find_path(URING_INCLUDE_DIR liburing.h HINTS ${URING_INCLUDEDIR} ${URING_INCLUDE_DIRS})
find_library(URING_LIBRARY NAMES liburing.so HINTS ${URING_LIBDIR} ${URING_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(uring DEFAULT_MSG URING_LIBRARY URING_INCLUDE_DIR)

mark_as_advanced(URING_UNCLUDE_DIR URING_LIBRARY)