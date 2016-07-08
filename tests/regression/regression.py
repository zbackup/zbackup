#!/usr/bin/env python3

import sys, argparse, os, time, datetime, subprocess, io, re, argparse, random
import RandomBytes, ZBackupExecutor


# Regression test zbackup.  Usage:
# regression.py --zbackup ~/Projects/zbackup/zbackup --tmp /tmp/zbackup-test
#
#  Still TODO are:
# commands:
# - encrypted repositories
# - backup dir / file storage rather than normal stdin / stdout
# - nbd
# - import / export
# - inspect
# - passwd
# - config
# 
# -o options
#  None tested
#
# -O options
#  gc.concat tested
#  remainder not tested

THOUSAND = 1000
MILLION = THOUSAND * THOUSAND


class ZBackupTest:
    def __init__(self, zbackupExec):    
        self.zbackupExec = zbackupExec
        self.previousSize = 0
        self.backupsByFilename = {}

        print ("Testing zbackupExec = " + str(zbackupExec))
                

    def deleteBackupFile (self, fileName):
        self.zbackupExec.removeFile (fileName);
        del self.backupsByFilename[fileName] # So we don't try to restore later


    def addBackupToList (self, randomNumberGen, fileName, testStep):
        print ("Adding backup " + fileName + " " + str(randomNumberGen) + " " + testStep)

        self.backupsByFilename[fileName] = randomNumberGen;
        self.zbackupExec.writeZBackupFile (randomNumberGen, fileName)


    def checkBackups (self, minSizeIncrease, maxSizeIncrease):
        for fileName, randomNumberGen in self.backupsByFilename.items ():
            print ("    Checking backup " + fileName + " " + str(randomNumberGen))
            self.zbackupExec.readZBackupFile (randomNumberGen, fileName)
            
        newSize = self.zbackupExec.getSizeInBytes ()
        minSize = self.previousSize + minSizeIncrease
        maxSize = self.previousSize + maxSizeIncrease

        print ("    Checking new size " + str(newSize) + " within " + str((minSize, maxSize)))

        if newSize < minSize:
            raise Exception("Backup too small")   
            
        if newSize > maxSize:
            raise Exception("Backup too large")   
            
        self.previousSize = newSize


    def executeTest (self):
        self.zbackupExec.initZBackup ()

        self.addBackupToList (RandomBytes.RandomBytes (1, 10 * MILLION), "seed1/backup1", "Add first file")
        self.checkBackups (9 * MILLION, 10.1 * MILLION)

        self.addBackupToList (RandomBytes.RandomBytes (1, 11 * MILLION), "seed1/backup2", "Should only store 1M more")
        self.checkBackups (900 * THOUSAND, 1100 * THOUSAND) # Only stored the additional 1M bytes

        self.addBackupToList (RandomBytes.RandomBytes (2, 10 * MILLION), "seed2/backup3", "Completely new 10M")
        self.checkBackups (9 * MILLION, 10.1 * MILLION) 

        self.addBackupToList (RandomBytes.RandomBytes (2, 20 * MILLION), "seed2/backup4", "20M, 10M on top of last 10M")
        self.checkBackups (9 * MILLION, 10.2 * MILLION)  # Only stored the additional 10M bytes

        self.addBackupToList (RandomBytes.RandomBytes (2, 20 * MILLION), "seed2/backup5", "Exactly the same as before")
        self.checkBackups (0, 100 * THOUSAND)  # 100% duplicated

        self.addBackupToList (RandomBytes.RandomBytes (2, 20 * MILLION), "seed2/backup6", "Exactly the same as before")
        self.checkBackups (0, 100 * THOUSAND)  # 100% duplicated

        self.addBackupToList (RandomBytes.RandomBytes (1, 1 * MILLION), "seed1/backup7", "1M from the start of the first step")
        self.checkBackups (0, 100 * THOUSAND)  # 100% duplicated

        self.zbackupExec.compactZBackupIndex ()
        self.checkBackups (-100 * THOUSAND, 0)  # Should get a little smaller

        self.zbackupExec.garbageCollectZBackup ()
        self.checkBackups (0, 0)  # Shouldn't do anything

        self.deleteBackupFile ("seed1/backup1");
        self.checkBackups (-5 * THOUSAND, 0)  # Most of the data is stored by seed1/backup2
        self.zbackupExec.garbageCollectZBackup ()
        self.checkBackups (-100 * THOUSAND, 0)  # Most of the data is stored by seed1/backup2

        self.deleteBackupFile ("seed1/backup2");
        self.checkBackups (-5 * THOUSAND, 0)  # Deleting the backup file does almost nothing
        self.zbackupExec.garbageCollectZBackup ()
        self.checkBackups (-10.2 * MILLION, -9 * MILLION)  # All except the 1M used in seed1/backup7

        self.deleteBackupFile ("seed1/backup7");
        self.checkBackups (-5 * THOUSAND, 0)  # Deleting the backup file does almost nothing
        self.zbackupExec.garbageCollectZBackup ()
        self.checkBackups (-1.2 * MILLION, -900 * THOUSAND)  # Should free the 1M used in seed1/backup7



def main():  
    parser = argparse.ArgumentParser(description='Unit tests the zfs system.')

    parser.add_argument ("--zbackup", dest='zbackup', nargs=1, help='The zbackup program to test.')
    parser.add_argument ("--tmp", dest='tmp', nargs=1, help='The directory to use for backups')

    print ('argv:' + str(sys.argv) + ' ' + str(len(sys.argv)))

    args = parser.parse_args()

    test = ZBackupTest(ZBackupExecutor.ZBackupExecutor (directory = args.tmp[0], zbackup = args.zbackup[0], encryptionKey = None))
    test.executeTest ()

if __name__ == "__main__":
    
    main()

