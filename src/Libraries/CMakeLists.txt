if(OKTOPLUS_WITH_GRPC)
  add_subdirectory(Commands)
endif()
add_subdirectory(Configurations)
add_subdirectory(Resp)
add_subdirectory(Storage)


####################################################################
# Using system gtest/gmock and abseil packages
####################################################################

