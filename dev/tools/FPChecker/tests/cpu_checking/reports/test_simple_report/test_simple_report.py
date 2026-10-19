#!/usr/bin/env python

import subprocess
import os
import sys
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
    # --- compile code ---
    cmd = ["make"]
    try:
        cmdOutput = subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True)
    except subprocess.CalledProcessError as e:
        print(e.output)
        exit()

    # --- run code ---
    cmd = ["./main"]
    try:
        cmdOutput = subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True)
    except subprocess.CalledProcessError as e:
        print(e.output)
        exit()

    found = False
    fileName = None
    fileName = report.findReportFile('.fpc_logs')
    assert fileName, "Traces found"

    # --- Create report ----
    cmd = ["fpc-create-report"]
    try:
        cmdOutput = subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True)
    except subprocess.CalledProcessError as e:
        print(e.output)
        exit()

    report.checkReportWasCreated("./fpc-report")

    Found_1 = False
    Found_2 = False
    Found_3 = False
    Found_4 = False
    with open("./fpc-report/index.html", 'r') as fd:
        for line in fd:
            if '<a href="./positive_infinity/positive_infinity.html">4</a>' in line:
                Found_1 = True
            if '<a href="./latent_positive_infinity/latent_positive_infinity.html">4</a>' in line:
                Found_2 = True
            if '<img src="default_fp64_plot.svg"' in line:
                Found_3 = True
            if '<img src="default_fp32_plot.svg"' in line:
                Found_4 = True
    
    assert Found_1
    assert Found_2
    assert Found_3
    assert Found_4

