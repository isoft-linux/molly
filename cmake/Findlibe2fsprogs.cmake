find_library(LIBE2FSPROGS_LIBRARIES NAMES ext2fs)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libe2fsprogs DEFAULT_MSG 
                                  LIBE2FSPROGS_LIBRARIES)
