import abc
import argparse
import datetime
import glob
import os
import platform
import shutil
import subprocess
import sys
import time
from typing import List


# Windows:
# winget install Microsoft.WinDbg --accept-package-agreements  --accept-source-agreements --silent
# gdb -return-child-result -batch -ex "run" -ex "thread apply all bt" -ex "quit" --args ./${file}
# Install cdb from Add/Remove Programs > Windows Software Development Kits


class Runner(abc.ABC):
    '''
    Abstract base class for runner class.
    '''

    def __init__(self, path: str, args: List[str], 
                 timeout: int):
        """
        Parameters:
        path        The path of (PJSIP) program to run
        args        Arguments to be given to the (PJSIP) program
        timeout     Maximum run time in seconds after which program will be killed
                    and dump will be generated
        """
        self.path = os.path.abspath(path)
        '''Path to program'''
        if not os.path.exists(self.path):
            raise Exception(f'Program not found: {self.path}')
        
        self.args = args
        '''Arguments for the program'''

        self.timeout = timeout
        '''Maximum running time (secs) before we kill the program'''

        self.popen : subprocess.Popen = None
        '''Popen object when running the program, will be set later'''

    @classmethod
    def info(cls, msg, box=False):
        if box:
            print('\n' + '#'*60)
            print('##')
        print(('## ' if box else '') + 'ci-runner.py: ' + msg)
        if box:
            print('##')
            print('#'*60)

    @classmethod
    def err(cls, msg, box=False):
        if box:
            sys.stderr.write('\n' + '#'*60 + '\n')
            sys.stderr.write('##\n')
        sys.stderr.write(('## ' if box else '') + 'ci-runner.py: ' + msg + '\n')
        if box:
            sys.stderr.write('##\n')
            sys.stderr.write('#'*60 + '\n')

        
    @classmethod
    @abc.abstractmethod
    def get_dump_dir(cls) -> str:
        """
        Returns directory where dump file will be saved
        """
        pass

    @classmethod
    @abc.abstractmethod
    def install(cls):
        """
        Install crash handler for this machine
        """
        pass

    @abc.abstractmethod
    def detect_crash(self) -> bool:
        """
        Determine whether process has crashed or just exited normally.
        Returns True if it had crashed.
        """
        pass

    @abc.abstractmethod
    def process_crash(self):
        """
        Process dump file.
        """
        pass

    @abc.abstractmethod
    def terminate(self):
        """
        Terminate a process and generate dump file
        """
        pass

    def run(self):
        """
        Run the program, monitor dump file when crash happens, and terminate
        the program if it runs for longer than permitted.
        """

        self.popen = subprocess.Popen([self.path] + self.args)
        try:
            self.popen.wait(self.timeout)
        except subprocess.TimeoutExpired as e:
            self.err('Execution timeout, terminating process..', box=True)
            self.terminate()
            time.sleep(1)

        if self.popen.returncode != 0:
            self.err(f'exit code {self.popen.returncode}, waiting until crash dump is written')
            for _ in range(60):
                if self.detect_crash():
                    break
            if not self.detect_crash():
                self.err('ERROR: UNABLE TO FIND CRASH DUMP FILE!')

        if self.detect_crash():
            self.err('Processing crash info..', box=True)
            self.process_crash()

        # Propagate program's return code as our return code
        sys.exit(self.popen.returncode)


class WinRunner(Runner):
    """
    Windows runner
    """

    def __init__(self, path: str, args: List[str], 
                 timeout: int = 10):
        super().__init__(path, args, timeout=timeout)

        self.cdb_exe = self.find_cdb()
        if not self.cdb_exe:
            raise Exception('Could not find cdb.exe')
        
        self.procdump_exe = self.find_procdump()
        if not self.procdump_exe:
            raise Exception('Could not find procdump.exe')
        self.procdump_exe = os.path.abspath(self.procdump_exe)

    @classmethod
    def find_cdb(cls) -> str:
        """
        Find cdb.exe (console debugger?).
        It can be installed from Windows SDK installer:
        1a. Run Windows SDK 10 installer if you haven't installed it
         b. If you have installed it, run Windows SDK installer from Add/Remove Program ->
            Windows Software Development Kit -> Modify
        2. Select component: "Debugging Tools for Windows"
        """
        CDB_PATHS = [
            r'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe'
        ]
        for path in CDB_PATHS:
            if os.path.exists(path):
                return path
        return None

    @classmethod
    def find_procdump(cls) -> str:
        """
        Find procdump.exe.
        See https://learn.microsoft.com/en-us/sysinternals/downloads/procdump
        """
        return shutil.which('procdump')

    @classmethod
    def get_dump_dir(cls) -> str:
        """Get the actual path of the dump directory that is installed in the registry"""
        home = os.environ['userprofile']
        return os.path.abspath( os.path.join(home, 'Dumps') )

    @classmethod
    def install(cls):
        """Requires administrator privilege to write to registry"""
        import winreg

        #
        # Setup registry to tell Windows to create minidump on app crash.
        # https://learn.microsoft.com/en-us/windows/win32/wer/collecting-user-mode-dumps
        #
        HKLM = winreg.ConnectRegistry(None, winreg.HKEY_LOCAL_MACHINE)
        LD = r'SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps'
        try:
            ld = winreg.OpenKey(HKLM, LD)
        except OSError as e:
            ld = winreg.CreateKey(HKLM, LD)
            cls.info(f'Registry "LocalDumps" key created')
        
        dump_dir = cls.get_dump_dir()
        if not os.path.exists(dump_dir):
            os.makedirs(dump_dir)
            cls.info(f'Directory {dump_dir} created')

        DUMP_FOLDER = '%userprofile%\Dumps'
        try:
            val, type = winreg.QueryValueEx(ld, 'DumpFolder')
        except OSError as e:
            val, type = '', None
        if val.lower() != DUMP_FOLDER.lower() or type != winreg.REG_EXPAND_SZ:
            winreg.SetValueEx(ld, 'DumpFolder', None, winreg.REG_EXPAND_SZ, DUMP_FOLDER)
            cls.info(f'Registry "DumpFolder" set to {DUMP_FOLDER}')

        try:
            val, type = winreg.QueryValueEx(ld, 'DumpType')
        except OSError as e:
            val, type = -1, None
        MINIDUMP = 1
        if val!=MINIDUMP or type!=winreg.REG_DWORD:
            winreg.SetValueEx(ld, 'DumpType', None, winreg.REG_DWORD, MINIDUMP)
            cls.info(f'Registry "DumpType" set to {MINIDUMP}')

        winreg.CloseKey(ld)

        # Check cdb.exe and procdump.exe
        errors = []
        cdb_exe = cls.find_cdb()
        if not cdb_exe:
            errors.append('cdb.exe not found')
        procdump_exe = cls.find_procdump()
        if not procdump_exe:
            errors.append('procdump.exe not found')

        if errors:
            cls.err('ERROR: ' + ' '.join(errors))
            sys.exit(1)

        cls.info('Running infrastructure is ready')

    def get_dump_path(self) -> str:
        dump_dir = self.get_dump_dir()
        basename = os.path.basename(self.path)
        dump_file = f'{basename}.{self.popen.pid}.dmp'
        return os.path.join(dump_dir, dump_file)
        
    def detect_crash(self) -> bool:
        """
        Determine whether process has crashed or just exited normally.
        Returns True if it had crashed.
        """
        dump_path = self.get_dump_path()
        return os.path.exists(dump_path)

    def terminate(self):
        """
        Terminate a process and generate dump file
        """
        
        # procdump default dump filename is PROCESSNAME_YYMMDD_HHMMDD.
        # Since guessing the datetime can be unreliable, let's create
        # a temporary directory for procdump to store the dumpfile.
        dtime = datetime.datetime.now()
        temp_dir = os.path.join( self.get_dump_dir(), f'ci-runner-{dtime.strftime("%y%m%d-%H%M%S")}')
        os.makedirs(temp_dir)

        # Run procdump to dump the process
        procdump_p = subprocess.Popen([
                    self.procdump_exe,
                    '-accepteula', '-o', 
                    f'{self.popen.pid}',
                ],
                cwd=temp_dir,
                )
        procdump_p.wait()
        
        # We can now terminate the process
        time.sleep(1)
        self.popen.terminate()
        
        # Get the dump file
        files = glob.glob( os.path.join(temp_dir, "*.dmp") )
        if not files:
            self.err("ERROR: unable to find dump file(s) generated by procdump")
            raise Exception('procdump dump file not found')

        # Copy and rename the procdump's dump file to standard dump file location/name
        dump_file = files[-1]
        shutil.copyfile(dump_file, self.get_dump_path())

        # Don't need the temp dir anymore
        shutil.rmtree(temp_dir)

        # Now it should be detected as crash
        if not self.detect_crash():
            dump_dir = self.get_dump_dir()
            self.err("ERROR: procdump's dump file not detected")
            self.err(f'Contents of {dump_dir}:')
            files = os.listdir(dump_dir)
            self.err('  '.join(files[:20]))


    def process_crash(self):
        """
        Process dump file.
        """
        dump_path = self.get_dump_path()
        if not os.path.exists(dump_path):
            dump_dir = self.get_dump_dir()
            self.err(f'ERROR: unable to find {dump_path}')
            self.err(f'Contents of {dump_dir}:')
            files = os.listdir(dump_dir)
            self.err('  '.join(files[:20]))
            raise Exception('Dump file not found')
        
        # Execute cdb to print crash info, but print it to stderr instead
        args = [
            self.cdb_exe,
            '-z',
            dump_path,
            '-c',
            '!analyze -v; ~* k; q',
        ]
        cdb = subprocess.Popen(args)  # , stdout=sys.stderr
        cdb.wait()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-i', '--install', action='store_true',
                        help='Install crash handler on this machine')
    parser.add_argument('-t', '--timeout', type=int,
                        help='Max running time in seconds before terminated')
    parser.add_argument('prog', help='Program to run', nargs='?')
    parser.add_argument('args', nargs='*',
                        help='Arguments for the program (use -- to separate from ci-runner\'s arguments)')

    args = parser.parse_args()

    kwargs = {}
    if args.timeout is not None:
        kwargs['timeout'] = args.timeout

    sysname = platform.system()
    if sysname=='Windows':
        ci_class = WinRunner
    elif sysname=='Darwin':
        pass
    elif sysname=='Linux':
        pass
    else:
        raise Exception(f'Implement for {sysname}')

    if args.install:
        ci_class.install()

    if args.prog:
        ci_runner = ci_class(args.prog, args.args, **kwargs)
        ci_runner.run()
        # will not reach here


if __name__ == '__main__':
    main()
