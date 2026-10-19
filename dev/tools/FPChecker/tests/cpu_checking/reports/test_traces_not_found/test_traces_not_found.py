#!/usr/bin/env python

import subprocess
import os
import sys
import re
#sys.path.append('..')
#sys.path.append('.')
#import report
from reports import report

def setup_module(module):
    THIS_DIR = os.path.dirname(os.path.abspath(__file__))
    os.chdir(THIS_DIR)

def teardown_module(module):
    cmd = ["make clean"]
    cmdOutput = subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True)

def test_1():
    # --- Create report ----
    cmd = ["fpc-create-report"]
    try:
        cmdOutput = subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True)
    except subprocess.CalledProcessError as e:
        print(e.output)
        exit()

    assert "Trace files found: 0" in str(cmdOutput)
    assert "Exponent usage files found: 0" in str(cmdOutput)

    report.checkReportWasCreated("./fpc-report")

    Found_1 = False
    Found_2 = False
    with open("./fpc-report/index.html", 'r') as fd:
        for line in fd:
            if '<img src="default_fp64_plot.svg"' in line:
                Found_1 = True
            if '<img src="default_fp64_plot.svg"' in line:
                Found_2 = True
    
    assert Found_1
    assert Found_2

    # Check instrumented instructions are correct
    Found_3 = False
    Found_4 = False
    with open("./fpc-report/index.html", 'r') as fd:
        lines = fd.readlines()
        text = " ".join(lines)
        normalized_text = re.sub(r'\s+', ' ', text).strip()
        if '<td class="td_class_short">FP32:</td> <td class="td_class"> 0 </td>' in normalized_text:
            Found_3 = True
        if '<td class="td_class_short">FP64:</td> <td class="td_class"> 0 </td>' in normalized_text:
            Found_4 = True
    
    assert Found_3
    assert Found_4

    

