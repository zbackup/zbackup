#!/usr/bin/env python3

import sys, argparse, os, time, datetime, subprocess, io, re, argparse, random
import ByteSource, RandomByteSource, FileByteSource, ZBackupExecutor, Logger


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

        Logger.log (1, "Testing zbackupExec = " + str(zbackupExec))
                

    def deleteBackupFile (self, fileName):
        self.zbackupExec.removeFile (fileName);
        del self.backupsByFilename[fileName] # So we don't try to restore later


    def addBackupToList (self, bytesource, fileName, testStep):
        Logger.log (1, "Adding backup " + fileName + " " + testStep)

        self.backupsByFilename[fileName] = bytesource;
        self.zbackupExec.writeZBackupFile (bytesource, fileName)


    def checkBackups (self, minSizeIncrease = None, maxSizeIncrease = None):
        Logger.log (1, "    Checking backup " + str(len(self.backupsByFilename)) + " files")

        for fileName, bytesource in self.backupsByFilename.items ():
            Logger.log (2, "    Checking backup " + fileName + " " + str(bytesource))
            self.zbackupExec.readZBackupFile (bytesource, fileName)
            
        newSize = self.zbackupExec.getSizeInBytes ()

        if minSizeIncrease and maxSizeIncrease:
            minSize = self.previousSize + minSizeIncrease
            maxSize = self.previousSize + maxSizeIncrease

            Logger.log (2, "    Checking new size " + str(newSize) + " within " + str((minSize, maxSize)))

            if newSize < minSize:
                raise Exception("Backup too small")   

            if newSize > maxSize:
                raise Exception("Backup too large")   
            
        self.previousSize = newSize


    def initTests (self):
        self.zbackupExec.initZBackup ()


    def executeTest (self):
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


    def sampleFilesTest (self, samplesdir, randomgenerator, loops, add_per_loop, delete_per_loop):
        samplefiles = {}

        for root, dirs, files in os.walk(samplesdir, topdown=False):
            for name in files:
                relative = root[len(samplesdir):]
                #print (root, relative, name)
                samplefiles[relative + "/" + name] = FileByteSource.FileByteSource (samplesdir, relative + "/" + name)

                
        for loop in range (loops):
            Logger.log (0, "Test Loop " + str(loop) + " Adding Files")

            for file_to_add in randomgenerator.sample (list(samplefiles), add_per_loop):
                if self.zbackupExec.getFileExists (file_to_add):
                    Logger.log (1, "    Deleting before adding again:" + file_to_add)
                    self.deleteBackupFile (file_to_add);

                self.addBackupToList (samplefiles[file_to_add], file_to_add, "Add sample file in loop:" + str(loop))

            self.checkBackups ()

            Logger.log (0, "Test Loop " + str(loop) + " Deleting Files")

            for file_to_delete in randomgenerator.sample (list(self.backupsByFilename), delete_per_loop):
                self.deleteBackupFile (file_to_delete);
                
            self.checkBackups ()
            self.zbackupExec.garbageCollectZBackup ()
            self.checkBackups ()
            self.zbackupExec.compactZBackupIndex ()
            self.checkBackups ()


        #print (samplefiles)

def main():  
    parser = argparse.ArgumentParser(description='Unit tests the zfs system.')

    parser.add_argument ("--zbackup", dest='zbackup', nargs=1, help='The zbackup program to test.')
    parser.add_argument ("--tmp", dest='tmp', nargs=1, help='The directory to use for backups')
    parser.add_argument ("--samples", dest='samples', nargs=1, help='The directory to use for backups')
    parser.add_argument ("--key", nargs=1, help='Encryption key to use for zbackup encryption')
    parser.add_argument ("--log-level", nargs=1, type = int, help='The log level to use.  0 is the default, up to 2 is supported')
    parser.add_argument ("--seed", nargs=1, type=int, help='The seed used for random number generation')
    parser.add_argument ("--loops", nargs=1, type=int, help='The seed used for random number generation')
    parser.add_argument ("--add-per-loop", nargs=1, type=int, help='The seed used for random number generation')
    parser.add_argument ("--delete-per-loop", nargs=1, type=int, help='The seed used for random number generation')

    args = parser.parse_args()

    if args.key:
        key = args.key[0]
    else:
        key = None

    if args.log_level:
        Logger.setLogLevel (args.log_level[0])

    Logger.log (3, 'argv:' + str(sys.argv) + ' ' + str(args))

    test = ZBackupTest(ZBackupExecutor.ZBackupExecutor (directory = args.tmp[0], zbackup = args.zbackup[0], encryptionKey = key))
    test.initTests ()
    #test.executeTest ()
    test.sampleFilesTest (samplesdir = args.samples[0], 
                          randomgenerator = random.Random(args.seed[0]), 
                          loops= args.loops[0], 
                          add_per_loop = args.add_per_loop[0], 
                          delete_per_loop = args.delete_per_loop[0])

if __name__ == "__main__":
    
    main()

