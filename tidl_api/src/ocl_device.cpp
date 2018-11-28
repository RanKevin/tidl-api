/******************************************************************************
 * Copyright (c) 2017-2018  Texas Instruments Incorporated - http://www.ti.com/
 *   All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Texas Instruments Incorporated nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/


#include <cstdlib>
#include <cassert>
using std::size_t;

#include <iostream>

#include "ocl_device.h"
#include "ocl_util.h"
#include "trace.h"
#include "../dsp/ocl_wrapper.dsp_h"

using namespace tidl;

static const char* error2string(cl_int err);
static void        errorCheck(cl_int ret, int line);

Device::Device(cl_device_type t, const DeviceIds& ids):
                device_type_m(t), device_ids_m(ids)
{
    TRACE::print("\tOCL Device: %s created\n",
              device_type_m == CL_DEVICE_TYPE_ACCELERATOR ? "DSP" :
              device_type_m == CL_DEVICE_TYPE_CUSTOM ? "EVE" : "Unknown");

    for (int i = 0; i < MAX_DEVICES; i++)
        queue_m[i] = nullptr;

}

DspDevice::DspDevice(const DeviceIds& ids, const std::string &binary_filename):
              Device(CL_DEVICE_TYPE_ACCELERATOR, ids)
{
    cl_uint num_devices_found;
    cl_device_id device_ids[MAX_DEVICES];

    cl_int errcode = clGetDeviceIDs(0,               // platform
                             device_type_m,          // device_type
                             MAX_DEVICES,            // num_entries
                             device_ids,             // devices
                             &num_devices_found);    // num_devices
    errorCheck(errcode, __LINE__);

    if (num_devices_found != 1)
        throw Exception("OpenCL DSP device not found",
                        __FILE__, __FUNCTION__, __LINE__);

    cl_int num_compute_units;
    errcode = clGetDeviceInfo(device_ids[0],
                              CL_DEVICE_MAX_COMPUTE_UNITS,
                              sizeof(num_compute_units),
                              &num_compute_units,
                              nullptr);

    if (num_compute_units == 1)
    {
        context_m = clCreateContextFromType(0,              // properties
                                            device_type_m,  // device_type
                                            0,              // pfn_notify
                                            0,              // user_data
                                            &errcode);
        errorCheck(errcode, __LINE__);

        // Queue 0 on device 0
        queue_m[0] = clCreateCommandQueue(context_m,
                                          device_ids[0],
                                          CL_QUEUE_PROFILING_ENABLE,
                                          &errcode);
        errorCheck(errcode, __LINE__);
        BuildProgramFromBinary(binary_filename, device_ids, 1);
    }
    else
    {
        const cl_uint NUM_SUB_DEVICES = 2;

        // Create 2 sub-device's, each consisting of a C66x DSP
        cl_device_partition_property properties[3] =
                                        { CL_DEVICE_PARTITION_EQUALLY, 1, 0 };

        // Query the number of sub-devices that can be created
        cl_uint n_sub_devices = 0;
        errcode = clCreateSubDevices(device_ids[0],      // in_device
                                     properties,         // properties
                                     0,                  // num_devices
                                     NULL,               // out_devices
                                     &n_sub_devices);    // num_devices_ret
        errorCheck(errcode, __LINE__);

        assert(n_sub_devices == NUM_SUB_DEVICES);

        // Create the sub-devices
        cl_device_id sub_devices[NUM_SUB_DEVICES] = {0, 0};
        errcode = clCreateSubDevices(device_ids[0],        // in_device
                                     properties,           // properties
                                     n_sub_devices,        // num_devices
                                     sub_devices,          // out_devices
                                     nullptr);             // num_devices_ret
        errorCheck(errcode, __LINE__);

        // Create a context containing the sub-devices
        context_m = clCreateContext(NULL,               // properties
                                    NUM_SUB_DEVICES,    // num_devices
                                    sub_devices,        // devices
                                    NULL,               // pfn_notify
                                    NULL,               // user_data
                                    &errcode);          // errcode_ret
        errorCheck(errcode, __LINE__);

        // Create queues to each sub-device
        for (auto id : device_ids_m)
        {
            int index = static_cast<int>(id);
            queue_m[index] = clCreateCommandQueue(context_m,
                                          sub_devices[index],
                                          CL_QUEUE_PROFILING_ENABLE,
                                          &errcode);
            errorCheck(errcode, __LINE__);
        }

        BuildProgramFromBinary(binary_filename, sub_devices, NUM_SUB_DEVICES);
    }

    errcode = clGetDeviceInfo(device_ids[0],
                                CL_DEVICE_MAX_CLOCK_FREQUENCY,
                                sizeof(freq_in_mhz_m),
                                &freq_in_mhz_m,
                                nullptr);
    errorCheck(errcode, __LINE__);
}


EveDevice::EveDevice(const DeviceIds& ids, const std::string &kernel_names):
            Device(CL_DEVICE_TYPE_CUSTOM, ids)
{
    cl_uint num_devices_found;
    cl_device_id all_device_ids[MAX_DEVICES];

    // Find all the OpenCL devices available of the given type
    cl_int errcode = clGetDeviceIDs(0,              // platform
                             device_type_m,         // device_type
                             MAX_DEVICES,           // num_entries
                             all_device_ids,        // devices
                             &num_devices_found);   // num_devices
    errorCheck(errcode, __LINE__);

    assert (num_devices_found >= device_ids_m.size());

    context_m = clCreateContextFromType(0,              // properties
                                        device_type_m,  // device_type
                                        0,              // pfn_notify
                                        0,              // user_data
                                        &errcode);
    errorCheck(errcode, __LINE__);


    // Create command queues to OpenCL devices specified by the
    // device_ids_m set.
    for (auto id : device_ids_m)
    {
        int index = static_cast<int>(id);
        queue_m[index] = clCreateCommandQueue(context_m,
                                      all_device_ids[index],
                                      CL_QUEUE_PROFILING_ENABLE|
                                      CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
                                      &errcode);
        errorCheck(errcode, __LINE__);
    }

    BuildProgramFromBinary(kernel_names, all_device_ids, device_ids_m.size());

    errcode = clGetDeviceInfo(all_device_ids[0],
                                CL_DEVICE_MAX_CLOCK_FREQUENCY,
                                sizeof(freq_in_mhz_m),
                                &freq_in_mhz_m,
                                nullptr);
    errorCheck(errcode, __LINE__);
}


bool DspDevice::BuildProgramFromBinary(const std::string &BFN,
                                       cl_device_id device_ids[],
                                       int num_devices)
{
    size_t bin_len = ocl_wrapper_dsp_bin_len;

    assert (bin_len != 0);

    // Casting to make ocl_read_binary work with clCreateProgramWithBinary
    const unsigned char *bin_arrc = reinterpret_cast <const unsigned char *>
                                    (ocl_wrapper_dsp_bin);

    size_t lengths[num_devices];
    for (int i=0; i < num_devices; i++) lengths[i] = bin_len;

    const unsigned char* binaries[num_devices];
    for (int i=0; i < num_devices; i++) binaries[i] = bin_arrc;

    cl_int err;
    program_m = clCreateProgramWithBinary(context_m,
                                          num_devices,
                                          device_ids,          // device_list
                                          lengths,
                                          binaries,
                                          0,                   // binary_status
                                          &err);
    errorCheck(err, __LINE__);

    const char *options = "";
    err = clBuildProgram(program_m, num_devices, device_ids, options, 0, 0);
    errorCheck(err, __LINE__);

    return true;
}

bool EveDevice::BuildProgramFromBinary(const std::string& kernel_names,
                                       cl_device_id device_ids[],
                                       int num_devices)
{
    cl_int err;
    cl_device_id executor_device_ids[MAX_DEVICES];

    int i = 0;
    for (auto id : device_ids_m)
        executor_device_ids[i++] = device_ids[static_cast<int>(id)];

    program_m = clCreateProgramWithBuiltInKernels(context_m,
                                          num_devices,
                                          executor_device_ids,  // device_list
                                          kernel_names.c_str(),
                                          &err);
    errorCheck(err, __LINE__);

    return true;
}

Kernel::Kernel(Device* device, const std::string& name,
               const KernelArgs& args, uint8_t device_index):
           name_m(name), device_m(device), device_index_m(device_index)
{
    TRACE::print("Creating kernel %s\n", name.c_str());
    cl_int err;
    kernel_m = clCreateKernel(device_m->program_m, name_m.c_str(), &err);
    errorCheck(err, __LINE__);

    for (int i=0; i < tidl::internal::NUM_CONTEXTS; i++)
        event_m[i] = nullptr;

    int arg_index = 0;
    for (const auto& arg : args)
    {
        if (!arg.isLocal())
        {
            if (arg.kind() == DeviceArgInfo::Kind::BUFFER)
            {
                cl_mem buffer = device_m->CreateBuffer(arg);

                clSetKernelArg(kernel_m, arg_index, sizeof(cl_mem), &buffer);
                TRACE::print("  Arg[%d]: %p\n", arg_index, buffer);

                if (buffer)
                    buffers_m.push_back(buffer);
            }
            else if (arg.kind() == DeviceArgInfo::Kind::SCALAR)
            {
                clSetKernelArg(kernel_m, arg_index, arg.size(), arg.ptr());
                TRACE::print("  Arg[%d]: %p\n", arg_index, arg.ptr());
            }
            else
            {
                assert ("DeviceArgInfo kind not supported");
            }
        }
        else
        {
            clSetKernelArg(kernel_m, arg_index, arg.size(), NULL);
            TRACE::print("  Arg[%d]: local, %d\n", arg_index, arg.size());
        }
        arg_index++;

    }
}

bool Kernel::UpdateScalarArg(uint32_t index, size_t size, const void *value)
{
    cl_int ret = clSetKernelArg(kernel_m, index, size, value);
    return ret == CL_SUCCESS;
}

Kernel& Kernel::RunAsync(uint32_t context_idx)
{
    // Execute kernel
    TRACE::print("\tKernel: device %d executing %s, context %d\n",
                 device_index_m, name_m.c_str(), context_idx);
    cl_int ret = clEnqueueTask(device_m->queue_m[device_index_m],
                               kernel_m, 0, 0, &event_m[context_idx]);
    errorCheck(ret, __LINE__);

    return *this;
}

bool Kernel::Wait(uint32_t context_idx)
{
    // Wait called without a corresponding RunAsync
    if (event_m[context_idx] == nullptr)
        return false;

    TRACE::print("\tKernel: waiting context %d...\n", context_idx);
    cl_int ret = clWaitForEvents(1, &event_m[context_idx]);
    errorCheck(ret, __LINE__);

    ret = clReleaseEvent(event_m[context_idx]);
    errorCheck(ret, __LINE__);
    event_m[context_idx] = nullptr;

    TRACE::print("\tKernel: finished execution\n");

    return true;
}

extern void CallbackWrapper(void *user_data) __attribute__((weak));

static
void EventCallback(cl_event event, cl_int exec_status, void *user_data)
{
    if (exec_status != CL_SUCCESS || user_data == nullptr)  return;
    if (CallbackWrapper)  CallbackWrapper(user_data);
}

bool Kernel::AddCallback(void *user_data, uint32_t context_idx)
{
    if (event_m[context_idx] == nullptr)
        return false;

    return clSetEventCallback(event_m[context_idx], CL_COMPLETE, EventCallback,
                              user_data) == CL_SUCCESS;
}

Kernel::~Kernel()
{
    for (auto b : buffers_m)
        device_m->ReleaseBuffer(b);

    clReleaseKernel(kernel_m);
}

cl_mem Device::CreateBuffer(const DeviceArgInfo &Arg)
{
    size_t  size     = Arg.size();
    void   *host_ptr = Arg.ptr();

    if (host_ptr == nullptr)
    {
        TRACE::print("\tOCL Create B:%p\n", nullptr);
        return nullptr;
    }

    bool hostPtrInCMEM = __is_in_malloced_region(host_ptr);

    // Conservative till we have sufficient information.
    cl_mem_flags flag = CL_MEM_READ_WRITE;

    if (hostPtrInCMEM) flag |= (cl_mem_flags)CL_MEM_USE_HOST_PTR;
    else               flag |= (cl_mem_flags)CL_MEM_COPY_HOST_PTR;

    cl_int       errcode;
    cl_mem buffer = clCreateBuffer(context_m,
                                   flag,
                                   size,
                                   host_ptr,
                                   &errcode);
    errorCheck(errcode, __LINE__);

    TRACE::print("\tOCL Create B:%p\n", buffer);

    return buffer;
}

void Device::ReleaseBuffer(cl_mem M)
{
    TRACE::print("\tOCL Release B:%p\n", M);
    clReleaseMemObject(M);
}

/// Release resources associated with an OpenCL device
Device::~Device()
{
    TRACE::print("\tOCL Device: deleted\n");
    for (unsigned int i = 0; i < device_ids_m.size(); i++)
    {
        clFinish(queue_m[i]);
        clReleaseCommandQueue (queue_m[i]);
    }

    clReleaseProgram      (program_m);
    clReleaseContext      (context_m);
}

void errorCheck(cl_int ret, int line)
{
    if (ret != CL_SUCCESS)
    {
        std::cerr << "ERROR: [ Line: " << line << "] " << error2string(ret) << std::endl;
        exit(ret);
    }
}

/// Convert OpenCL error codes to a string
const char* error2string(cl_int err)
{
    switch(err)
    {
         case   0: return "CL_SUCCESS";
         case  -1: return "CL_DEVICE_NOT_FOUND";
         case  -2: return "CL_DEVICE_NOT_AVAILABLE";
         case  -3: return "CL_COMPILER_NOT_AVAILABLE";
         case  -4: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
         case  -5: return "CL_OUT_OF_RESOURCES";
         case  -6: return "CL_OUT_OF_HOST_MEMORY";
         case  -7: return "CL_PROFILING_INFO_NOT_AVAILABLE";
         case  -8: return "CL_MEM_COPY_OVERLAP";
         case  -9: return "CL_IMAGE_FORMAT_MISMATCH";
         case -10: return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
         case -11: return "CL_BUILD_PROGRAM_FAILURE";
         case -12: return "CL_MAP_FAILURE";

         case -30: return "CL_INVALID_VALUE";
         case -31: return "CL_INVALID_DEVICE_TYPE";
         case -32: return "CL_INVALID_PLATFORM";
         case -33: return "CL_INVALID_DEVICE";
         case -34: return "CL_INVALID_CONTEXT";
         case -35: return "CL_INVALID_QUEUE_PROPERTIES";
         case -36: return "CL_INVALID_COMMAND_QUEUE";
         case -37: return "CL_INVALID_HOST_PTR";
         case -38: return "CL_INVALID_MEM_OBJECT";
         case -39: return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
         case -40: return "CL_INVALID_IMAGE_SIZE";
         case -41: return "CL_INVALID_SAMPLER";
         case -42: return "CL_INVALID_BINARY";
         case -43: return "CL_INVALID_BUILD_OPTIONS";
         case -44: return "CL_INVALID_PROGRAM";
         case -45: return "CL_INVALID_PROGRAM_EXECUTABLE";
         case -46: return "CL_INVALID_KERNEL_NAME";
         case -47: return "CL_INVALID_KERNEL_DEFINITION";
         case -48: return "CL_INVALID_KERNEL";
         case -49: return "CL_INVALID_ARG_INDEX";
         case -50: return "CL_INVALID_ARG_VALUE";
         case -51: return "CL_INVALID_ARG_SIZE";
         case -52: return "CL_INVALID_KERNEL_ARGS";
         case -53: return "CL_INVALID_WORK_DIMENSION";
         case -54: return "CL_INVALID_WORK_GROUP_SIZE";
         case -55: return "CL_INVALID_WORK_ITEM_SIZE";
         case -56: return "CL_INVALID_GLOBAL_OFFSET";
         case -57: return "CL_INVALID_EVENT_WAIT_LIST";
         case -58: return "CL_INVALID_EVENT";
         case -59: return "CL_INVALID_OPERATION";
         case -60: return "CL_INVALID_GL_OBJECT";
         case -61: return "CL_INVALID_BUFFER_SIZE";
         case -62: return "CL_INVALID_MIP_LEVEL";
         case -63: return "CL_INVALID_GLOBAL_WORK_SIZE";
         default: return "Unknown OpenCL error";
    }
}

Device::Ptr Device::Create(DeviceType core_type, const DeviceIds& ids,
                           const std::string& name)
{
    Device::Ptr p(nullptr);
    if (core_type == DeviceType::DSP)
        p.reset(new DspDevice(ids, name));
    else if (core_type == DeviceType::EVE)
        p.reset(new EveDevice(ids, name));

    return p;
}

// Minimum version of OpenCL required for this version of TIDL API
#define MIN_OCL_VERSION "01.01.17.00"
static bool CheckOpenCLVersion(cl_platform_id id)
{
    cl_int err;
    size_t length;
    err = clGetPlatformInfo(id, CL_PLATFORM_VERSION, 0, nullptr, &length);
    if (err != CL_SUCCESS) return false;

    std::unique_ptr<char> version(new char[length]);
    err = clGetPlatformInfo(id, CL_PLATFORM_VERSION, length, version.get(),
                            nullptr);
    if (err != CL_SUCCESS) return false;

    std::string v(version.get());

    if (v.substr(v.find("01."), sizeof(MIN_OCL_VERSION)) >= MIN_OCL_VERSION)
        return true;

    std::cerr << "TIDL API Error: OpenCL " << MIN_OCL_VERSION
              << " or higher required." << std::endl;

    return false;
}

static bool PlatformIsAM57()
{
    cl_platform_id id;
    cl_int err;

    err = clGetPlatformIDs(1, &id, nullptr);
    if (err != CL_SUCCESS) return false;

    if (!CheckOpenCLVersion(id))
       return false;

    // Check if the device name is AM57
    size_t length;
    err = clGetPlatformInfo(id, CL_PLATFORM_NAME, 0, nullptr, &length);
    if (err != CL_SUCCESS) return false;

    std::unique_ptr<char> name(new char[length]);

    err = clGetPlatformInfo(id, CL_PLATFORM_NAME, length, name.get(), nullptr);
    if (err != CL_SUCCESS) return false;

    std::string platform_name(name.get());

    if (platform_name.find("AM57") == std::string::npos)
        return false;

    return true;
}

// TI DL is supported on AM57x - EVE or C66x devices
uint32_t Device::GetNumDevices(DeviceType device_type)
{
    if (!PlatformIsAM57()) return 0;

    // Convert DeviceType to OpenCL device type
    cl_device_type t = (device_type == DeviceType::EVE) ?
                                    CL_DEVICE_TYPE_CUSTOM :
                                    CL_DEVICE_TYPE_ACCELERATOR;

    // Find all the OpenCL devices available
    cl_uint num_devices_found;
    cl_device_id all_device_ids[MAX_DEVICES];

    cl_int errcode = clGetDeviceIDs(0,                   // platform
                                    t,                   // device_type
                                    MAX_DEVICES,         // num_entries
                                    all_device_ids,      // devices
                                    &num_devices_found); // num_devices


    if (errcode != CL_SUCCESS)            return 0;
    if (num_devices_found == 0)           return 0;

    // DSP, return the number of compute units since we maintain a
    // queue to each compute unit (i.e. C66x DSP)
    if (t == CL_DEVICE_TYPE_ACCELERATOR)
    {
        cl_int num_compute_units;
        errcode = clGetDeviceInfo(all_device_ids[0],
                                CL_DEVICE_MAX_COMPUTE_UNITS,
                                sizeof(num_compute_units),
                                &num_compute_units,
                                nullptr);
        if (errcode != CL_SUCCESS)
            return 0;

        return num_compute_units;
    }

    // EVE, return the number of devices since each EVE is a device
    return num_devices_found;
}
