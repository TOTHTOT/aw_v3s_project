#!/bin/bash
###
 # @Description: build project scripts,just need change {BUILD_APP_NAME},{DRIVER_NAME} in other project. v1.1
 # @Author: TOTHTOT
 # @Date: 2023-09-29 08:48:21
 # @LastEditTime: 2023-10-16 09:49:03
 # @LastEditors: TOTHTOT
 # @FilePath: /aw_v3s_project/04_bh1750/build_project.sh
### 

INPUT_ARGS_NUM=$#
APP_SOURCE_C=$1
DRIVER_NAME="bh1750_drive"
BUILD_APP_NAME="bh1750_app"
BUILD_NAME="${DRIVER_NAME}.ko"
MAKE_FILE_MODULES_NAME="${DRIVER_NAME}.o"

CROSS_COMPLIER_TYPE="arm-linux-gnueabihf-gcc"
NFS_MODULE_PATH="/home/yyh/Learn/aw_v3s/nfs/lib/modules/5.2.0-licheepi-zero+/"
MAKE_FILE_KERNELDIR="/home/yyh/Learn/aw_v3s/linux"
BUILD_APP_FLAG=1
# check input args number
if [ "$INPUT_ARGS_NUM" -lt 1 ]; then
	BUILD_APP_FLAG=0
fi
echo "info: clean brfore build"
make KERNELDIR=$MAKE_FILE_KERNELDIR MODULES_NAME=$MAKE_FILE_MODULES_NAME clean

echo "start build $BUILD_NAME and $BUILD_APP_NAME"
make KERNELDIR=$MAKE_FILE_KERNELDIR MODULES_NAME=$MAKE_FILE_MODULES_NAME
# 检查 make 命令的返回值
if [ $? -eq 0 ]; then
    echo "info: Make succeede"
	echo ""
	sudo cp $BUILD_NAME $NFS_MODULE_PATH
# build app
	if [ $BUILD_APP_FLAG -eq 1 ]; then
		echo "info: build app"
		echo ""
		$CROSS_COMPLIER_TYPE $APP_SOURCE_C -o $BUILD_APP_NAME
		sudo cp $BUILD_APP_NAME  $NFS_MODULE_PATH
	fi
	echo "info: clean project"
	echo ""
	make KERNELDIR=$MAKE_FILE_KERNELDIR MODULES_NAME=$MAKE_FILE_MODULES_NAME clean
	if [ $BUILD_APP_FLAG -eq 1 ]; then
		rm $BUILD_APP_NAME 
	fi
else
    echo "error: Make failed\n"
	make KERNELDIR=$MAKE_FILE_KERNELDIR MODULES_NAME=$MAKE_FILE_MODULES_NAME clean
fi