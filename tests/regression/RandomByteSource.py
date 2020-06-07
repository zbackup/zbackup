#!/usr/bin/env python3

import sys, argparse, os, time, datetime, subprocess, io, re, argparse, random
import ByteSource


class RandomByteSource(ByteSource.ByteSource):
    def __init__(self, randSeed, numBytes):
        self.randSeed = randSeed
        self.numBytes = numBytes


    def getBytes (self):
        random.seed (self.randSeed)

        return bytearray(random.getrandbits(8) for i in range(self.numBytes))

    def __str__(self):
        return "RandomBytes[" + str(self.randSeed) + "] for " + str(self.numBytes) + " bytes";

