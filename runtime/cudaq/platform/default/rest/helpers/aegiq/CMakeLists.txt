# ============================================================================ #
# Copyright (c) 2025 NVIDIA Corporation & Affiliates.                          #
# All rights reserved.                                                         #
#                                                                              #
# This source code and the accompanying materials are made available under     #
# the terms of the Apache License 2.0 which accompanies this distribution.     #
# ============================================================================ #

target_sources(cudaq-rest-qpu PRIVATE AegiqServerHelper.cpp)
add_target_config(aegiq)

add_library(cudaq-serverhelper-aegiq SHARED AegiqServerHelper.cpp)
target_link_libraries(cudaq-serverhelper-aegiq
  PUBLIC
  cudaq-common
  fmt::fmt-header-only
)
install(TARGETS cudaq-serverhelper-aegiq DESTINATION lib)