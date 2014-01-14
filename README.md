**NOTICE: There are known bugs that may cause OpenXLT to incorrectly convert XLTek data at this time. We are actively working to fix these issues.**

OpenXLT
=======
*OpenXLT* is a free, open-source C++ program for the conversion of EEG data from the Natus XLTek file format to formats readable by other programs. Currently, only the .mat file format is supported for exporting.

### Usage
For the compilation to work correctly, your system must have Matlab installed and the proper Environment variables set. See [this page](http://www.mathworks.com/help/matlab/matlab_external/compiling-engine-applications-with-the-mex-command.html) for instructions.

On a UNIX system, execute the following:
```
$ ./main <dirname>
```
where dirname contains the XLTek binary files to convert. Currently, the dirname must have a trailing slash or you will get an error.

On Windows, open a command prompt and CD into the directory with your binary and execute:
```
main.exe <dirname>
```
In this case, the dirname must NOT have a trailing backslash. This limitation will be fixed in future versions.

### Support
Having trouble with OpenXLT? File a bug report.
