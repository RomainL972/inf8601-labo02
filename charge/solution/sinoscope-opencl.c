#define CL_USE_DEPRECATED_OPENCL_1_2_APIS

#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "sinoscope.h"

typedef struct sinoscope_int_params {
    cl_uint width;
    cl_uint height;
    cl_uint taylor;
    cl_uint interval;
} sinoscope_int_params_t;

int sinoscope_opencl_init(sinoscope_opencl_t* opencl, cl_device_id opencl_device_id, unsigned int width,
                          unsigned int height) {
    cl_int error = CL_SUCCESS;

    cl_context context = clCreateContext(NULL, 1, &opencl_device_id, NULL, NULL, &error);
    if (error != CL_SUCCESS || context == NULL) {
        LOG_ERROR("clCreateContext failed (%d)", error);
        goto fail_exit;
    }

    char* kernel_code = NULL;
    size_t kernel_len = 0;

    if (opencl_load_kernel_code(&kernel_code, &kernel_len) < 0) {
        LOG_ERROR("opencl_load_kernel_code failed");
        goto fail_release_context;
    }

    cl_program program = clCreateProgramWithSource(context, 1, (const char**)&kernel_code, &kernel_len, &error);
    free(kernel_code);
    if (error != CL_SUCCESS || program == NULL) {
        LOG_ERROR("clCreateProgramWithSource failed (%d)", error);
        goto fail_release_context;
    }

    error = clBuildProgram(program, 1, &opencl_device_id, opencl_build_options(), NULL, NULL);
    if (error != CL_SUCCESS) {
        opencl_print_build_log(program, opencl_device_id);
        goto fail_release_program;
    }

    cl_kernel kernel = clCreateKernel(program, "sinoscope_kernel", &error);
    if (error != CL_SUCCESS || kernel == NULL) {
        opencl_print_build_log(program, opencl_device_id);
        goto fail_release_program;
    }

    size_t buffer_size = (size_t)width * height * 3;
    cl_mem buffer      = clCreateBuffer(context, CL_MEM_WRITE_ONLY, buffer_size, NULL, &error);
    if (error != CL_SUCCESS || buffer == NULL) {
        LOG_ERROR("clCreateBuffer failed (%d)", error);
        goto fail_release_kernel;
    }

    cl_command_queue queue = clCreateCommandQueue(context, opencl_device_id, 0, &error);
    if (error != CL_SUCCESS || queue == NULL) {
        LOG_ERROR("clCreateCommandQueue failed (%d)", error);
        goto fail_release_buffer;
    }

    /* The kernel keeps its own reference on the program. */
    clReleaseProgram(program);

    opencl->device_id = opencl_device_id;
    opencl->context   = context;
    opencl->buffer    = buffer;
    opencl->kernel    = kernel;
    opencl->queue     = queue;

    return 0;

fail_release_buffer:
    clReleaseMemObject(buffer);
fail_release_kernel:
    clReleaseKernel(kernel);
fail_release_program:
    clReleaseProgram(program);
fail_release_context:
    clReleaseContext(context);
fail_exit:
    return -1;
}

void sinoscope_opencl_cleanup(sinoscope_opencl_t* opencl) {
    if (opencl == NULL) {
        return;
    }

    if (opencl->queue != NULL) {
        clFinish(opencl->queue);
        clReleaseCommandQueue(opencl->queue);
    }

    if (opencl->buffer != NULL) {
        clReleaseMemObject(opencl->buffer);
    }

    if (opencl->kernel != NULL) {
        clReleaseKernel(opencl->kernel);
    }

    if (opencl->context != NULL) {
        clReleaseContext(opencl->context);
    }

    opencl->device_id = NULL;
    opencl->context   = NULL;
    opencl->queue     = NULL;
    opencl->buffer    = NULL;
    opencl->kernel    = NULL;
}

int sinoscope_image_opencl(sinoscope_t* sinoscope) {
    if (sinoscope == NULL || sinoscope->opencl == NULL) {
        LOG_ERROR("OpenCL resources not created");
        goto fail_exit;
    }

    sinoscope_opencl_t* opencl = sinoscope->opencl;

    sinoscope_int_params_t ints = {
        .width    = sinoscope->width,
        .height   = sinoscope->height,
        .taylor   = sinoscope->taylor,
        .interval = sinoscope->interval,
    };

    cl_int error = CL_SUCCESS;

    error |= clSetKernelArg(opencl->kernel, 0, sizeof(cl_mem), &opencl->buffer);
    error |= clSetKernelArg(opencl->kernel, 1, sizeof(ints), &ints);
    error |= clSetKernelArg(opencl->kernel, 2, sizeof(float), &sinoscope->interval_inverse);
    error |= clSetKernelArg(opencl->kernel, 3, sizeof(float), &sinoscope->time);
    error |= clSetKernelArg(opencl->kernel, 4, sizeof(float), &sinoscope->max);
    error |= clSetKernelArg(opencl->kernel, 5, sizeof(float), &sinoscope->phase0);
    error |= clSetKernelArg(opencl->kernel, 6, sizeof(float), &sinoscope->phase1);
    error |= clSetKernelArg(opencl->kernel, 7, sizeof(float), &sinoscope->dx);
    error |= clSetKernelArg(opencl->kernel, 8, sizeof(float), &sinoscope->dy);
    if (error != CL_SUCCESS) {
        LOG_ERROR("clSetKernelArg failed (%d)", error);
        goto fail_exit;
    }

    size_t work_size = (size_t)sinoscope->width * sinoscope->height;

    error = clEnqueueNDRangeKernel(opencl->queue, opencl->kernel, 1, NULL, &work_size, NULL, 0, NULL, NULL);
    if (error != CL_SUCCESS) {
        LOG_ERROR("clEnqueueNDRangeKernel failed (%d)", error);
        goto fail_exit;
    }

    error = clEnqueueReadBuffer(opencl->queue, opencl->buffer, CL_TRUE, 0, sinoscope->buffer_size, sinoscope->buffer, 0,
                                NULL, NULL);
    if (error != CL_SUCCESS) {
        LOG_ERROR("clEnqueueReadBuffer failed (%d)", error);
        goto fail_exit;
    }

    return 0;

fail_exit:
    return -1;
}
