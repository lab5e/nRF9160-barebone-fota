#
# Copyright (c) 2020 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(barebone-fota)

# NORDIC SDK APP START
target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/lte/lte.c)
target_sources(app PRIVATE src/lte/connection_info.c)
target_sources(app PRIVATE src/downloader/downloader.c)
target_sources(app PRIVATE src/fota/fota_tlv.c)
target_sources(app PRIVATE src/fota/fota.c)

# NORDIC SDK APP END
zephyr_include_directories(src)
zephyr_include_directories(src/proto)

