"""
Simple usage example for PyReduce
Loads a ScopeSim simulated MICADO dataset (with updated spectral layout and updated line lists), and runs the full extraction. 
"""

import os.path
import pyreduce
from pyreduce import datasets


# define parameters
instrument = "MICADO"
target = ""
night = ""
mode = ""
steps = (
     # "bias",
     "flat",
     "orders",
     "curvature",
     # # "scatter",
     # "norm_flat", 
     "wavecal",
     # "science",
     # "continuum",
     # "finalize",
)

# some basic settings
# Expected Folder Structure: base_dir/datasets/MICADO/*.fits.gz
# Feel free to change this to your own preference, values in curly brackets will be replaced with the actual values {}

# Define the path for the base, input and output directories
# The data (with fixed header keywords) can be fetched from https://www.dropbox.com/sh/e3lnvtkmyjveajk/AABPHxeUdDO5AnkWCAjbM0e1a?dl=0 and stored in input_dir

#PC
base_dir ="/media/data/Dropbox/Dropbox/WORKING/iMICADO/Working/WORKING_PyReduce/DATA/datasets/MICADO/raw_new/"# an example path which you should change to your prefereed one 

input_dir = "HK/"
output_dir = "reduced_new/"

config = pyreduce.configuration.get_configuration_for_instrument(instrument, plot=1)


#configuring parameters of individual steps here overwrites those defined in the the settings_MICADO.json file. 
# Once you are staified with the chosen parameter, you can update it in settings_MICADO.json.


# config["orders"]["noise"] = 100
# config["curvature"]["extraction_width"] = 360 
# config["wavecal"]["extraction_width"] = 360
# config["science"]["extraction_width"] = 360


pyreduce.reduce.main(
    instrument,
    target,
    night,
    mode,
    steps,
    base_dir=base_dir,
    input_dir=input_dir,
    output_dir=output_dir,
    configuration=config,
    # order_range=(5, 6),
)
