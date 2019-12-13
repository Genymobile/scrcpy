# 标注需要支持的CMake最小版本
cmake_minimum_required(VERSION 3.4.1)


#设置生成的 so 动态库最后输出的路径
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/jniLibs/${ANDROID_ABI})


# 初始化目录变量
set(libturbojpeg_dir ${CMAKE_SOURCE_DIR}/libs/libturbojpeg)
set(INC_DIR ${libturbojpeg_dir}/include)
set(libturbojpeg_lib_dir ${libturbojpeg_dir}/prebuilt)


# 添加头文件目录
include_directories(${INC_DIR})


# 设置资源路径
set(SOURCE_FILES src/main/jni/compress.c)


# add_library 定义需要编译的代码库 (1.名称, 2.类型, 3.包含的源码)
add_library(compress SHARED ${SOURCE_FILES})
add_library(libturbojpeg SHARED IMPORTED)
set_target_properties(libturbojpeg PROPERTIES IMPORTED_LOCATION ${libturbojpeg_lib_dir}/${ANDROID_ABI}/libturbojpeg.so)


# 可以写多个 find_library
# 定义当前代码库需要依赖的系统或者第三方库文件 (1.给一个名字, 2.真正要查找的)
find_library(log-lib log)
find_library(graphics jnigraphics)

# target_link_libraries设置最终编译的目标代码库
# add_library 生成的
# find_library 找到的系统库
target_link_libraries(compress libturbojpeg ${log-lib} ${graphics})

