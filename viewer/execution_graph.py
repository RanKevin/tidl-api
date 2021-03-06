#!/usr/bin/env python

# Copyright (c) 2018 Texas Instruments Incorporated - http://www.ti.com/
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
# * Neither the name of Texas Instruments Incorporated nor the
# names of its contributors may be used to endorse or promote products
# derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

""" Frame Execution Graph

This script parses timestamp output generated from the TIDL API and
displays a graph of frame execution.

The script requires `matplotlib` to be installed.
"""

import argparse
import matplotlib.pyplot as mpyplot
import matplotlib.patches as mpatch

USAGE = """
    Display frame execution using trace data generated by the TIDL API.
"""

# Supported TIDL API classes
COMPONENTS = ('eop', 'eo1', 'eo2')

# APIs with timestamps
# ProcessFrameStartAsync, ProcessFrameWait, RunAsyncNext
KEY_PFSA = 'pfsa'
KEY_PFW = 'pfw'
KEY_RAN = 'ran'

KEY_START = 'start'
KEY_END = 'end'

BARH_COLORS = ('lightgray', 'green', 'blue', 'yellow',
               'black', 'orange', 'red', 'cyan')

BARH_LEGEND_STR = {'eop' : 'ExecutionObjectPipeline',
                   'eo1' : 'ExecutionObject 0',
                   'eo2' : 'ExecutionObject 1',
                   'pfw' : 'Process Frame Wait',
                   'pfsa': 'Process Frame Start Async',
                   'ran' : 'Run Async Next',
                   'total': 'Total frame time'}

class Range:
    """ Defines a range in terms of start and stop timestamps. """

    def __init__(self):
        self.start = 0
        self.end = 0

    def set_start(self, start):
        """Set the start time."""

        self.start = start

    def set_end(self, end):
        """ Set the end time."""

        self.end = end

    def subtract(self, val):
        """ Subtracl val from each start/end."""

        self.start -= val
        self.end -= val

    def get_range(self):
        """Return (start, duration)."""

        return (self.start, self.end-self.start)


class FrameInfo:
    """ All recorded events for a single frame. """

    def __init__(self, index):
        """Set up dictionaries to store timestamp data"""

        self.index = index
        self.eo_type = {}
        self.eo_id = {}

        self.data = {}
        for component in COMPONENTS:
            self.data[component] = {}

    def update(self, component, api, phase, val):
        """Update the [c][api][phase] with timestamp"""

        if component not in COMPONENTS:
            print('Invalid component: {}'.format(component))
            return

        if api not in self.data[component]:
            self.data[component][api] = Range()

        if phase == KEY_START:
            self.data[component][api].set_start(val)
        elif phase == KEY_END:
            self.data[component][api].set_end(val)
        else:
            raise Exception('Invalid key: {}'.format(phase))

    def update_eo_info(self, component, eo_type, eo_id):
        """Set the Execution Object type and index. Used to generate
           the EVEx or DSPx labels"""

        self.eo_type[component] = eo_type
        self.eo_id[component] = eo_id

    def eo_info(self):
        """Return a string corresponding to the EO info. Device numbering in
           TRM starts at 1. E.g. EVE1, EVE2, DSP1, DSP2 etc.
        """

        device_list = []
        for component in ('eo1', 'eo2'):
            device = ""
            if not self.data[component]:
                continue

            # Corresponds to DeviceType enum in inc/executor.h
            if self.eo_type[component] == 0:
                device += 'DSP'
            else:
                device += 'EVE'

            device += str(self.eo_id[component]+1)

            device_list.append(device)

        return '+'.join(device_list)

    def get_range(self, component, api):
        """Return the range for the specified component:api combination"""

        if api in self.data[component]:
            return self.data[component][api].get_range()

        return None

    def get_max_range(self):
        """Return a tuple corresponding to the maximum range"""

        return (self.min(), self.max() - self.min())

    def get_total(self, component):
        """Return the range for the specified component"""

        if not self.data[component]:
            print("{} not available".format(component))
            return None

        start = self.data[component][KEY_PFSA].start
        end = self.data[component][KEY_PFW].end
        return (start, int(end-start))

    def min(self):
        """ Return the lowest timestamp for a frame"""

        vals = []
        for component in self.data:
            for api in self.data[component]:
                vals.append(self.data[component][api].start)

        return min(vals)

    def max(self):
        """ Return the highest timestamp for a frame"""
        vals = []
        for component in self.data:
            for api in self.data[component]:
                vals.append(self.data[component][api].end)

        return max(vals)

    def subtract(self, val):
        """Subtract val from every timestamp in this frame. Use to
           adjust start of frame 0 to 0.
        """

        for component in self.data:
            for api in self.data[component]:
                self.data[component][api].subtract(val)

    def get_plot_ranges(self, components):
        """ Return a list of (component, range) tuples for all api data
            available for the specified components.
        """

        ranges = []
        for component in self.data:
            if component not in components:
                continue

            for api in self.data[component]:
                range_tuple = self.get_range(component, api)
                if range_tuple is not None:
                    label = component + ':' + api
                    ranges.append((label, range_tuple))

        # Reverse sort by duration (improves graph rendering)
        ranges.sort(key=lambda kv: kv[1][1], reverse=True)

        return ranges

    def get_plot_totals(self):
        """ Return (component, range) tuples for all available components.
            The range corresponds to the execution time for the entire
            component.
        """

        ranges = []
        for component in self.data:
            range_tuple = self.get_total(component)
            if range_tuple is not None:
                ranges.append((component, range_tuple))

        # Sort by start time stamp
        ranges.sort(key=lambda kv: kv[1][0])

        return ranges

    def get_barh_ranges(self, found, verbosity):
        """Return a list of range tuples for plotting. Also return
           corresponding labels"""

        label_range_tuples = []

        if verbosity == 0:
            if not found['eo2']:
                if found['eop']:
                    component = 'eop'
                else:
                    component = 'eo1'
                label_range_tuples.append(('total', self.get_max_range()))
                label_range_tuples.extend(self.get_plot_ranges((component)))
            else:
                label_range_tuples = self.get_plot_totals()

        elif verbosity == 1:
            label_range_tuples.append(('total', self.get_max_range()))
            for component in COMPONENTS:
                label_range_tuples.extend(self.get_plot_ranges(component))

        labels = [i[0] for i in label_range_tuples]
        ranges = [i[1] for i in label_range_tuples]

        return ranges, labels

    def __repr__(self):
        string = '<FI:'

        # Sort components based in order in COMPONENTS
        components = list(self.data.keys())
        components.sort(key=COMPONENTS.index)
        #components.sort(key=lambda kv: COMPONENTS.index(kv))

        for component in components:
            if not self.data[component]:
                continue

            string += ' {} ['.format(component)

            # Sort Ranges by start values
            items = sorted(self.data[component].items(),
                           key=lambda kv: kv[1].start)
            for api in items:
                string += '{}: {} '.format(api[0],
                                           self.get_range(component, api[0]))
            string += ']'

        string += '>'

        return string


class Frames:
    """ Per-frame data across a set of frames """

    def __init__(self, v):
        self.trace_data = {}
        self.found = {}
        for component in COMPONENTS:
            self.found[component] = False
        self.verbosity = v

    def update(self, index, key, val):
        """ Update event data for a frame """

        # Canonicalize key to lower case
        component, api, phase = key.split(':')

        if index not in self.trace_data:
            self.trace_data[index] = FrameInfo(index)

        self.trace_data[index].update(component, api, phase, val)

        self.found[component] = True

    def adjust_to_zero(self):
        """ Adjust timestamp sequence to start at 0"""

        # Get a sorted list of frame indices
        indices = self.indices()

        # Find the smallest timestamp in the 0th frame
        min_val = self.trace_data[indices[0]].min()

        # Use this value to adjust all the timestamps
        for i in indices:
            self.trace_data[i].subtract(min_val)

    def len(self):
        """Returns the number of frames available."""

        return len(self.trace_data)

    def indices(self):
        """ Returns a sorted list of frame indices."""

        indices = list(self.trace_data.keys())
        indices.sort()
        return indices

    def min_max_indices(self):
        """ Returns the minimum and maximum frame index."""

        indices = list(self.trace_data.keys())
        return (min(indices), max(indices))

    def microseconds_per_frame(self):
        """Returns the average microseconds to execute a frame."""

        # Get a sorted list of frame indices
        indices = self.indices()

        num_frames = len(indices)

        # Find the smallest timestamp in the 0th frame
        min_val = self.trace_data[indices[0]].min()

        # Find the largest timestamp in the last frame
        max_val = self.trace_data[indices[-1]].max()

        return (max_val - min_val)/num_frames


    def __repr__(self):
        string = 'Frames:\n'
        indices = self.indices()
        for i in indices:
            string += str(self.trace_data[i])
            string += "\n"

        return string


def read_data(args):
    """ Read a sequence of trace data and create a Frames object
        Each row has the following syntax: frame_index, key, value
        There is a row for each event that is recorded for a frame
        E.g.
        48,EOP:PFSA:Start,1540246078613202
    """

    frames = Frames(args.verbosity)

    with open(args.input_file) as lines:
        for line in lines:
            info = line.rstrip().split(',')
            if (len(info) != 3 and len(info) != 5):
                continue

            frame_index = int(info[0])
            data_key = info[1].lower()

            frames.update(index=frame_index, key=data_key, val=int(info[2]))

            if len(info) == 5:
                component = data_key.split(':')[0]
                frames.trace_data[frame_index].update_eo_info(component,
                                                              int(info[3]),
                                                              int(info[4]))

    frames.adjust_to_zero()

    print('Found {} frames {} in timestamp data, {} microseconds per frame'.
          format(frames.len(), frames.min_max_indices(),
                 frames.microseconds_per_frame()))

    return frames


def insert_legend(labels, axes):
    """ Create and insert a legend."""

    bars = []
    legend_strings = []

    for label in labels:
        index = labels.index(label)
        # Create rectangles of the appropriate color for each label
        bars.append(mpatch.Rectangle((0, 0), 1, 1, fc=BARH_COLORS[index]))

        # Build a legend string from a ':' separated label string
        legend_string = ""
        for tags in label.split(':'):
            legend_string += BARH_LEGEND_STR[tags] + ' '

        legend_strings.append(legend_string)

    # Insert the legend (mapping of colors to labels)
    axes.legend(bars, legend_strings, loc='upper left')


def plot(frames, filename):
    """ Use matplotlib to plot the data."""

    figure, axes = mpyplot.subplots()

    y_labels = []
    y_ticks = []
    y_index = 0
    y_increment = 5
    legend_inserted = False

    for frame_index in sorted(frames.trace_data):
        frame = frames.trace_data[frame_index]
        ranges, labels = frame.get_barh_ranges(frames.found, frames.verbosity)
        axes.broken_barh(ranges,
                         (y_index, 5),
                         facecolors=BARH_COLORS,
                         alpha=0.8)

        if not legend_inserted:
            insert_legend(labels, axes)
            legend_inserted = True

        # label string - overall time to execute the frame
        bar_label = str(int(frame.max() - frame.min()))

        # Add total time label
        axes.annotate(bar_label,
                      (frame.max(), y_index + 2),
                      xytext=(5, 0),
                      textcoords="offset points",
                      fontsize=8,
                      va='center',
                      ha='left')

        # Add EO info label
        axes.annotate(frame.eo_info(),
                      (frame.min(), y_index + 2),
                      xytext=(-5, 0),
                      textcoords="offset points",
                      fontsize=6,
                      va='center',
                      ha='right')

        y_index += y_increment
        y_labels.append(str(frame_index))
        y_ticks.append(y_index)

    axes.set_xlabel('Microseconds')
    axes.set_ylabel('Frames')
    axes.set_yticks(y_ticks)
    axes.set_yticklabels(y_labels, verticalalignment='center')
    axes.grid(True)
    axes.set_title(filename)

    # Set a default size for the displayed figure
    figure.set_size_inches(14, 8)

    mpyplot.show()


def main():
    """ Parse arguments, read input into Frames and plot the data."""

    parser = argparse.ArgumentParser(description=USAGE)
    parser.add_argument("-v", "--verbosity", type=int, choices=[0, 1],
                        default=0,
                        help="Level of graph detail. 0->Summary, 1->Details")
    parser.add_argument("input_file",
                        default='trace.log',
                        help='Path to trace log file')
    args = parser.parse_args()


    frames = read_data(args)
    plot(frames, args.input_file)

if __name__ == '__main__':
    main()
