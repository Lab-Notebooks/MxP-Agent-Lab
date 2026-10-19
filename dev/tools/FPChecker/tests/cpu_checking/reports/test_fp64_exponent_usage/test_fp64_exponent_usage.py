#!/usr/bin/env python

import subprocess
import os
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
    # --- compile code ---
    cmd = ["make"]
    try:
        cmdOutput = subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True)
    except subprocess.CalledProcessError as e:
        print(e.output)
        exit()

    # --- run code ---
    cmd = ["FPC_EXPONENT_USAGE=1 ./main"]
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
    with open("./fpc-report/index.html", 'r') as fd:
        for line in fd:
            if '<img src="default_fp32_plot.svg"' in line:
                Found_1 = True
            if '<img src="histogram_fp64.svg"' in line:
                Found_2 = True
    
    assert Found_1
    assert Found_2

    # Check instrumented instructions are correct
    Found_3 = False
    with open("./fpc-report/index.html", 'r') as fd:
        lines = fd.readlines()
        text = " ".join(lines)
        normalized_text = re.sub(r'\s+', ' ', text).strip()
        if '<td class="td_class_short">FP64:</td> <td class="td_class"> 20 </td>' in normalized_text:
            Found_3 = True
    
    assert Found_3

    

