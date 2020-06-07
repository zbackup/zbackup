import sys, argparse, os, time, datetime, subprocess, io, re, argparse, random
from datetime import datetime

LOG_LEVEL = 0

def setLogLevel (new_level):
    global LOG_LEVEL

    LOG_LEVEL = new_level
    print ("Log level set to:" + str (LOG_LEVEL))


def log (log_level, *args):
    #print ("Log Level:", log_level, " vs ", LOG_LEVEL)

    if log_level <= LOG_LEVEL:
        print (datetime.now(), " : ", end='')

        for arg in args:
            print (arg, end='')

        print ()

