/******************************************************************************
 * Copyright (c) 2017-2018 Texas Instruments Incorporated - http://www.ti.com/
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *      * Neither the name of Texas Instruments Incorporated nor the
 *        names of its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *  THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

/*! @file execution_object.h */

#pragma once

#include <memory>

namespace tidl {

class Kernel;
class Device;
class LayerOutput;

typedef std::vector<std::unique_ptr<const LayerOutput>> LayerOutputs;

/*! @class ExecutionObject
    @brief Runs the TIDL network on an OpenCL device
*/

class ExecutionObject
{
    public:

        //! @private
        // Used by the Executor to construct an ExecutionObject
        ExecutionObject(Device* d, uint8_t device_index,
                        const  ArgInfo& create_arg,
                        const  ArgInfo& param_heap_arg,
                        size_t extmem_heap_size,
                        bool   internal_input);
        //! @private
        ~ExecutionObject();

        //! Specify the input and output buffers used by the EO
        //! @param in buffer used for input.
        //! @param out buffer used for output.
        void SetInputOutputBuffer (const ArgInfo& in, const ArgInfo& out);

        //! Returns a pointer to the input buffer set via SetInputOutputBuffer
        char* GetInputBufferPtr() const;

        //! Returns size of the input buffer
        size_t GetInputBufferSizeInBytes() const;

        //! @brief Set the frame index of the frame currently processed by the
        //! ExecutionObject. Used for trace/debug messages
        //! @param idx index of the frame
        void  SetFrameIndex(int idx);

        //! Returns the index of a frame being processed (set by SetFrameIndex)
        int   GetFrameIndex() const;

        //! Returns a pointer to the output buffer
        char* GetOutputBufferPtr() const;

        //! Returns the number of bytes written to the output buffer
        size_t GetOutputBufferSizeInBytes() const;

        //! @brief Start processing a frame. The call is asynchronous and returns
        //! immediately. Use ExecutionObject::ProcessFrameWait to wait
        bool ProcessFrameStartAsync();

        //! Wait for the execution object to complete processing a frame
        //! @return false if ExecutionObject::ProcessFrameWait was called
        //! without a corresponding call to
        //! ExecutionObject::ProcessFrameStartAsync.
        bool ProcessFrameWait();

        //! @brief return the number of cycles taken *on the device* to
        //! execute the process call
        //! @return Number of cycles to process a frame on the device.
        uint64_t GetProcessCycles() const;

        //! @brief return the number of milliseconds taken *on the device* to
        //! execute the process call
        //! @return Number of milliseconds to process a frame on the device.
        float    GetProcessTimeInMilliSeconds() const;

        //! Write the output buffer for each layer to a file
        //! <filename_prefix>_<ID>_HxW.bin
        void WriteLayerOutputsToFile(const std::string& filename_prefix=
                                     "trace_dump_") const;

        //! Returns a LayerOutput object corresponding to a layer.
        //! Caller is responsible for deleting the LayerOutput object.
        //! @see LayerOutput
        //! @param layer_index The layer index of the layer
        //! @param output_index The output index of the buffer for a given
        //!                     layer. Defaults to 0.
        const LayerOutput* GetOutputFromLayer(uint32_t layer_index,
                                              uint32_t output_index=0) const;

        //! Get output buffers from all layers
        const LayerOutputs* GetOutputsFromAllLayers() const;

        //! @private
        // Used by the Executor
        enum class CallType { INIT, PROCESS, CLEANUP };
        bool RunAsync(CallType ct);
        bool Wait    (CallType ct);

        ExecutionObject()                                  = delete;
        ExecutionObject(const ExecutionObject&)            = delete;
        ExecutionObject& operator=(const ExecutionObject&) = delete;

        void EnableOutputBufferTrace();

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_m;
};


/*! @class LayerOutput
    @brief Describes the output of a layer in terms of its shape. Also
    includes a pointer to the data.
*/
class LayerOutput
{
    public:
        LayerOutput(int layer_index, int output_index, int buffer_id,
                    int num_roi_m, int num_channels, size_t height,
                    size_t width, const char* data);
        ~LayerOutput();

        int    LayerIndex()       const { return layer_index_m; }
        int    NumberOfChannels() const { return num_channels_m; }
        size_t Height()           const { return height_m; }
        size_t Width()            const { return width_m; }
        size_t Size()             const { return height_m * width_m *
                                                 num_channels_m; }
        const char* Data()        const { return data_m; }

    private:
        int layer_index_m;
        int output_index_m;
        int buffer_id_m;
        int num_roi_m;
        int num_channels_m;
        size_t height_m;
        size_t width_m;
        const char* data_m;
};


} // namespace tidl
