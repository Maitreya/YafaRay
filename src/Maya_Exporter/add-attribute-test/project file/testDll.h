#include<maya/MPxCommand.h>

#ifndef TESTDLL
#define TESTDLL

class testDllCmd: public MPxCommand
{
public:
	virtual MStatus doIt(const MArgList& args);
	static void *creator(){return new testDllCmd;}
};

#endif