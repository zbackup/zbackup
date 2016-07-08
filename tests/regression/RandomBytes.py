#!/usr/bin/env python3

import sys, argparse, os, time, datetime, subprocess, io, re, argparse, random


class RandomBytes:
    def __init__(self, randSeed, numBytes):
        self.randSeed = randSeed
        self.numBytes = numBytes


    def getRandomBytes (self):
        random.seed (self.randSeed)
        
        return bytearray(random.getrandbits(8) for i in range(self.numBytes))

    def __str__(self):
        return "RandomBytes[" + str(self.randSeed) + "] for " + str(self.numBytes) + " bytes";

