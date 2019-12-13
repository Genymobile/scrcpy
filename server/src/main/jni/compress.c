#include "compress.h"

JNIEXPORT jbyteArray JNICALL Java_com_genymobile_scrcpy_JpegEncoder_test(JNIEnv* env, jobject thiz){
    LOGI("jni");
}

JNIEXPORT jbyteArray JNICALL Java_com_genymobile_scrcpy_JpegEncoder_compress(JNIEnv* env, jobject thiz, jobject buffer, jint width, jint pitch, jint height, jint quality){
    jbyteArray result = NULL;
    unsigned long jpegSize = 0;
    int subsampling = TJSAMP_420;
    jbyte* imgBuffer = NULL, *jpegBuffer = NULL;
    if(NULL == (imgBuffer=(*env)->GetDirectBufferAddress(env, buffer))){
        LOGI("imgBuffer is null");
        return NULL;
    }
    unsigned long maxSize = tjBufSize(pitch, height, subsampling);
    if(NULL == (jpegBuffer = tjAlloc(maxSize))){
        LOGI("jpegBuffer is null");
        return NULL;
    }
    tjhandle handle = tjInitCompress();
    if(NULL == handle){
        LOGI("tjInitCompress error: %s", tjGetErrorStr2(handle));
        tjDestroy(handle);
        handle = NULL;
        return NULL;
    }
    if (0 == tjCompress2(
        handle,
        (unsigned char*)imgBuffer,
        width,
        pitch*4,
        height,
        TJPF_RGBA,//PixelFormat.RGBA_8888
        &jpegBuffer,
        &jpegSize,
        subsampling,
        quality,
        TJFLAG_FASTDCT | TJFLAG_NOREALLOC
    )){
        //LOGI("size %d", jpegSize);
/*test
        FILE *jpegFile = NULL;
        if ((jpegFile = fopen("/sdcard/test.jpg", "wb")) == NULL){
            LOGI("opening output file");
        }
        if (fwrite(jpegBuffer, jpegSize, 1, jpegFile) < 1){
            LOGI("writing output file");
        }
        fclose(jpegFile);  jpegFile = NULL;
*/
        result = (*env)->NewByteArray(env, (int)jpegSize);
        (*env)->SetByteArrayRegion(env, result, 0, (int)jpegSize, jpegBuffer);
    }else{
        LOGI("tjCompress2 error");
        tjDestroy(handle);
        handle = NULL;
        tjFree(jpegBuffer);
        jpegBuffer = NULL;
        return NULL;
    }
    tjDestroy(handle);
    handle = NULL;
    tjFree(jpegBuffer);
    jpegBuffer = NULL;
    return result;
}

