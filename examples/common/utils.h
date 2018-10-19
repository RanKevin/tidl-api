/******************************************************************************
 * Copyright (c) 2018, Texas Instruments Incorporated - http://www.ti.com/
 * All rights reserved.
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
#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include "executor.h"
#include "execution_object.h"
#include "execution_object_pipeline.h"
#include "configuration.h"

using tidl::Executor;
using tidl::ExecutionObject;
using tidl::ExecutionObjectPipeline;
using tidl::Configuration;
using tidl::DeviceType;

Executor* CreateExecutor(DeviceType dt, int num, const Configuration& c,
                         int layer_group_id);

bool ReadFrame(ExecutionObject*     eo,
               int                  frame_idx,
               const Configuration& configuration,
               std::istream&        input_file);

bool ReadFrame(ExecutionObjectPipeline* eop,
               int                      frame_idx,
               const Configuration&     configuration,
               std::istream&            input_file);

bool WriteFrame(const ExecutionObject* eo, std::ostream& output_file);

void ReportTime(const ExecutionObject* eo);

bool CheckFrame(const ExecutionObject* eo, const char *ref_output);
bool CheckFrame(const ExecutionObjectPipeline *eop, const char *ref_output);

const char* ReadReferenceOutput(const std::string& name);

void AllocateMemory(const std::vector<ExecutionObject *>& eos);
void FreeMemory(const std::vector<ExecutionObject *>& eos);
void AllocateMemory(const std::vector<ExecutionObjectPipeline *>& eops);
void FreeMemory(const std::vector<ExecutionObjectPipeline *>& eops);
