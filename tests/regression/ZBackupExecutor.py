#!/usr/bin/env python3

import sys, argparse, os, time, datetime, subprocess, io, re, argparse, random


EMPTY_BYTES = bytearray ()


class ZBackupExecutor:
    def __init__(self, directory, zbackup, encryptionKey):    
        self.directory = directory
        self.zbackup = zbackup

        if encryptionKey is None:
            self.encryptionArgument = "--non-encrypted" 
        else:
            self.encryptionArgument = "--password-file "  + encryptionKey
            

    def __str__(self):
        return "ZBackupExecutor[" + self.zbackup + " " + self.encryptionArgument + " " + self.directory + "]";


    def executeZBackup (self, arguments, bytesToSTDIN):
        cCmd = self.zbackup + " " + self.encryptionArgument + " --silent " + arguments
        print ("        executeZBackup:" + cCmd)

        proc = subprocess.Popen(cCmd, shell = True, stdin = subprocess.PIPE, stdout = subprocess.PIPE)
        (stdout_data, stderr_data) = proc.communicate(input = bytesToSTDIN)

        proc.wait()           

        return stdout_data


    def getSizeInBytes (self):
        cmd="du -s %s" % self.directory
        size=subprocess.Popen([cmd], stdout=subprocess.PIPE, shell=True).communicate()[0].split()[0]

        return int(size) * 1024


    def getFilePath (self, fileName):
        fullFileName = self.directory + "/backups/" + fileName
        dirName = os.path.dirname(fullFileName)

        if not os.path.exists(dirName):
            print ("mkdir " + dirName)
            os.mkdir(dirName)

        return fullFileName     


    def writeZBackupFile (self, randomBytes, fileName):
        theBytes = randomBytes.getRandomBytes ()
        
        self.executeZBackup ("backup " + self.getFilePath (fileName), theBytes)



    def readZBackupFile (self, randomBytes, fileName):
        restoredBytes = self.executeZBackup ("restore " + self.getFilePath (fileName), EMPTY_BYTES)

        if not restoredBytes == randomBytes.getRandomBytes ():
            raise Exception("Bytes don't match")   


    def initZBackup (self):
        ignoreBytes = self.executeZBackup ("init " + self.directory, EMPTY_BYTES)


    def compactZBackupIndex (self):
        print ("Compact the zbackup index")
        ignoreBytes = self.executeZBackup (" gc -O gc.concat " + self.directory, EMPTY_BYTES)


    def garbageCollectZBackup (self):
        print ("Garbage collect zbackup")
        ignoreBytes = self.executeZBackup ("gc " + self.directory, EMPTY_BYTES)


    def removeFile (self, fileName):
        print ("Remove file " + fileName)
        
        fullFileName = self.getFilePath (fileName)
        os.remove (fullFileName)


