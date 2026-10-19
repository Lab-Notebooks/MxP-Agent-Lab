import glob
import json
import sys
import os

def loadReport(fileName):
  f = open(fileName,'r')
  data = json.load(f)
  f.close()
  return data

# ----- Regular reports ------
def findReportFile(path):
  reports = glob.glob(path+'/fpc_*.json')
  return reports[0]

def numberReportFiles(path):
  reports = glob.glob(path+'/fpc_*.json')
  return len(reports)

# ------ Histogram reports -------
def findHistogramFile(path):
  reports = glob.glob(path+'/exponent_usage_*.json')
  return reports[0]

def checkReportWasCreated(directory_path):
    # Check report dir was created
    assert os.path.exists(directory_path) and os.path.isdir(directory_path)

    # Check index.html was created
    file_path = os.path.join(directory_path, "index.html")
    assert os.path.exists(file_path) and os.path.isfile(file_path)

if __name__ == '__main__':
  fileName = sys.argv[1]
  loadReport(fileName)
