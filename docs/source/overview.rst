********
Overview
********

Software Architecture
+++++++++++++++++++++

:numref:`TIDL API Software Architecture` shows the TIDL API software architecture and how it fits into the software ecosystem on AM57x. The TIDL API leverages OpenCL APIs to deploy translated network models. It provides the following services:

* Make the application's input data available in memories associated with the :term:`compute core`.
* Initialize and run the :term:`layer groups<Layer group>` associated with the network on compute cores
* Make the output data available to the application

.. _`TIDL API Software Architecture`:

.. figure:: images/tidl-api.png
    :align: center

    TIDL API Software Architecture

The TIDL API consists of 4 C++ classes and associated methods: ``Configuration``, ``Executor``, ``ExecutionObject``, and ``ExecutionObjectPipeline``. Refer :ref:`using-tidl-api` and :ref:`api-documentation` for details.

Terminology
+++++++++++
.. glossary::
    :sorted:

    Network
        A description of the layers used in a Deep Learning model and the connections between the layers. The network is generated by the TIDL import tool and used by the TIDL API. Refer `Processor SDK Linux Software Developer's Guide (TIDL chapter)`_ for creating TIDL network and parameter binary files from TensorFlow and Caffe. A network consists of one or more Layer Groups.

    Parameter binary
        A binary file with weights generated by the TIDL import tool and used by the TIDL API.

    Layer
        A layer consists of mathematical operations such as filters, rectification linear unit (ReLU) operations, downsampling operations (usually called average pooling, max pooling or striding), elementwise additions, concatenations, batch normalization and fully connected matrix multiplications. Refer `Processor SDK Linux Software Developer's Guide (TIDL chapter)`_ for a list of supported layers.

    Layer Group
        A collection of interconnected layers. Forms a unit of execution. The Execution Object "runs" a layer group on a compute core i.e. it performs the mathematical operations associated with the layers in the layer group on the input and generates one or more outputs.

    Compute core
        A single EVE or C66x DSP. An Execution Object manages execution on one compute core. Also referred to as a **device** in OpenCL. Sitara AM5749 has 4 compute cores: EVE1, EVE2, DSP1 and DSP2.

    Executor
        A TIDL API class. The executor is responsible for initializing Execution Objects with a Configuration. The Executor is also responsible for initialzing the OpenCL runtime. Refer :ref:`api-ref-executor` for available methods.

    ExecutionObject
    EO
        A TIDL API class. Manages the execution of a layer group on a compute core. There is an EO associated with each compute core. The EO leverages the OpenCL runtime to manage execution. TIDL API implementation leverages the OpenCL runtime to offload network processing. Refer :ref:`api-ref-eo` for a description of the ExecutionObject class and methods.

    ExecutionObjectPipeline
    EOP
        A TIDL API class. Two use cases:

        * Pipeline execution of a single input frame across multiple Execution Objects (:ref:`use-case-2`).
        * Double buffering using the input and output buffer associated with an instance of ExecutionObjectPipeline (:ref:`use-case-3`).

        Refer :ref:`api-ref-eop` for a description of the ExecutionObjectPipeline class and methods.

    Configuration
        A TIDL API class. Used to specify a configuration for the Executor, including pointers to the network and parameter binary files. Refer :ref:`api-ref-configuration` for a description of the Configuration class and methods.

    Frame
        A buffer representing 2D data, typically an image.


.. _Processor SDK Linux Software Developer's Guide (TIDL chapter): http://software-dl.ti.com/processor-sdk-linux/esd/docs/latest/linux/Foundational_Components_TIDL.html
