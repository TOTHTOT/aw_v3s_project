#!/bin/bash
###
 # @Description: build project scripts,just need change {BUILD_NAME},{BUILD_APP_NAME},{MAKE_FILE_MODULES_NAME} in other project.
 # @Author: TOTHTOT
 # @Date: 2023-09-29 08:48:21
 # @LastEditTime: 2023-10-05 10:42:30
 # @LastEditors: TOTHTOT
 # @FilePath: /aw_v3s_project/03_dht11/build_project.sh
### 

INPUT_ARGS_NUM=$#
APP_SOURCE_C=$1
BUILD_NAME="dht11_drive.ko"
BUILD_APP_NAME="dht11_app"
MAKE_FILE_MODULES_NAME="dht11_drive.o"

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