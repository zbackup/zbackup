#!/usr/bin/env python3

import sys, argparse, os, time, datetime, subprocess, io, re, argparse, random
import ByteSource


class FileByteSource(ByteSource.ByteSource):
    def __init__(self, rootdir, filename):
        self.rootdir = rootdir
        self.filename = filename


    def getBytes (self):
        with open(self.rootdir + "/" + self.filename, "rb") as f:
            return  bytearray(f.read())


    def __str__(self):
        return "FileByteSource[" + self.rootdir + '/' + self.filename + "]";

