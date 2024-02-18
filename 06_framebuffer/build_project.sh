#!/bin/bash
###
# @Description: build project scripts,just need change {BUILD_APP_NAME},{DRIVER_NAME} in other project. v1.2
# @Author: TOTHTOT
# @Date: 2023-09-29 08:48:21
 # @LastEditTime: 2024-02-18 21:50:57
 # @LastEditors: TOTHTOT
 # @FilePath: /project/06_framebuffer/build_project.sh
###

INPUT_ARGS_NUM=$#
APP_SOURCE_C=$2
DRIVER_NAME=""
BUILD_APP_NAME="framebuffer_app"
BUILD_NAME="${DRIVER_NAME}.ko"
MAKE_FILE_MODULES_NAME="${DRIVER_NAME}.o"

CROSS_COMPLIER_TYPE="arm-linux-gnueabihf-gcc"
NFS_MODULE_PATH="/home/yyh/Learn/aw_v3s/nfs/lib/modules/5.2.0-licheepi-zero+"
MAKE_FILE_KERNELDIR="/home/yyh/Learn/aw_v3s/kernel/aw_v3s_linux"
BUILD_APP_FLAG=1

# Parse input arguments
while getopts ":Ada" opt; do
    case ${opt} in
    A)
        BUILD_APP_FLAG=1
        ;;
    d)
        BUILD_APP_FLAG=0
        ;;
    a)
        BUILD_APP_FLAG=2
        ;;
    \?)
        echo "Invalid option: $OPTARG"
        exit 1
        ;;
    esac
done

shift $((OPTIND - 1))

# check input args number
if [ "$INPUT_ARGS_NUM" -lt 1 ]; then
    BUILD_APP_FLAG=0
fi

if [ $BUILD_APP_FLAG -eq 0 ] || [ $BUILD_APP_FLAG -eq 1 ]; then
    echo "info: clean before build"
    make KERNELDIR=$MAKE_FILE_KERNELDIR MODULES_NAME=$MAKE_FILE_MODULES_NAME clean

    echo "start build $BUILD_NAME and $BUILD_APP_NAME"
    make KERNELDIR=$MAKE_FILE_KERNELDIR MODULES_NAME=$MAKE_FILE_MODULES_NAME
fi

# 检查 make 命令的返回值
if [ $? -eq 0 ]; then
    if [ $BUILD_APP_FLAG -eq 0 ] || [ $BUILD_APP_FLAG -eq 1 ]; then
        echo "info: Make succeeded"
        echo ""
        sudo cp $BUILD_NAME $NFS_MODULE_PATH
    fi
    # build app
    if [ $BUILD_APP_FLAG -eq 1 ] || [ $BUILD_APP_FLAG -eq 2 ]; then
        echo "info: build app"
        echo ""
        $CROSS_COMPLIER_TYPE $APP_SOURCE_C -o $BUILD_APP_NAME
        sudo cp $BUILD_APP_NAME $NFS_MODULE_PATH
    fi
    if [ $BUILD_APP_FLAG -eq 0 ] || [ $BUILD_APP_FLAG -eq 1 ]; then
        echo "info: clean project"
        echo ""
        make KERNELDIR=$MAKE_FILE_KERNELDIR MODULES_NAME=$MAKE_FILE_MODULES_NAME clean
    fi
    if [ $BUILD_APP_FLAG -eq 1 ] || [ $BUILD_APP_FLAG -eq 2 ]; then
        rm $BUILD_APP_NAME
    fi
else
    echo "error: Make failed\n"
    make KERNELDIR=$MAKE_FILE_KERNELDIR MODULES_NAME=$MAKE_FILE_MODULES_NAME clean
fi
