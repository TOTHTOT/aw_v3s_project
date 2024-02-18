# aw_v3s_project
 基于荔枝派的一些项目
## 编译脚本说明
 - 修改 NFS_MODULE_PATH(共享的nfs目录驱动存放位置,编译完成复制到这边) 和 MAKE_FILE_KERNELDIR(内核代码目录)
 - 使用方法
    1. ./build_project.sh -A 编译驱动和应用程序
    2. ./build_project.sh -a 编译应用程序
    3. ./build_project.sh -d 编译驱动