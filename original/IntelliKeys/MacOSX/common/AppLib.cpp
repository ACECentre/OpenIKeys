#ifdef SYS_TRAY
// fred sez - I have no idea.
#include "..\win\system tray\stdafx.h"
#endif

#include "IKCommon.h"

#include "IKString.h"
#include "IKUtil.h"
#include "IKFile.h"
#include "IKOverlayList.h"
#include "IKStringArray.h"
#include "IKMsg.h"
#include "IKMessage.h"

#include "AppLib.h"

#ifdef PLAT_MACINTOSH
typedef unsigned int DWORD;
typedef char * LPSTR;

//#include "MoreFilesx.h"
#include "Search.h"

#endif


//----------------------------------------------------------
//
//  static data

static const TCHAR * BlankString			= TEXT("");

static const TCHAR * systemsFileName		= TEXT("systems.txt");
static const TCHAR * appsFileName			= TEXT("applications.txt");
static const TCHAR * disallowedFileName     = TEXT("disallowed.txt");
static const TCHAR * lockFileName			= TEXT("lock.txt");
static const TCHAR * interestFileName		= TEXT("interest.txt");

static IKString systemsPath = "";
static IKMap systemsMap;
static DWORD systemsLastLoadTime = 0;

static IKString applicationsPath = "";
static IKMap applicationsMap;
static DWORD applicationsLastLoadTime = 0;

static IKString interestPath = "";
static IKMap interestMap;
static DWORD interestLastLoadTime = 0;

static IKString disallowedPath = "";
static IKMap disallowedMap;
static DWORD disallowedLastLoadTime = 0;

static IKString lockPath = "";
static IKFile * pLockFile=0;
static int lockCount = 0;

static IKMap studentMap;
static DWORD studentLastLoadTime = 0;
static IKString studentLastGroup = TEXT("");
static IKString studentLastStudent = TEXT("");

static bool bSaveDeferred = false;
static bool bSaveNeeded   = false;
static bool bStudentSaveNeeded   = false;

int AppLibDeferSavingBlock::m_count = 0;

static bool bAllowMissingApps = false;

//---------------------------------------------------------
//
//  static functions


static IKString MakeStudentAppPrefFilePath ( IKString &group, IKString &student )
{
	//  make a path to the student's app prefs file
	IKString path = IKUtil::MakeStudentPath(group,student);
	path += (const char *) "applications.txt";
	return path;
}


static void ReloadStudentFiles(TCHAR *GroupName, TCHAR *StudentName)
{
	//  make a path to the student's file
	IKString group(GroupName);
	IKString student(StudentName);
	IKString path = MakeStudentAppPrefFilePath(group,student);

	//  see if we're supposed to do this now.
	bool bReload = false;
	if (group.CompareNoCase(studentLastGroup)==0 && student.CompareNoCase(studentLastStudent)==0)
	{
		//  same student
		unsigned int lastModTime = IKFile::GetModTimeSecs1970(path);
		bReload = lastModTime>studentLastLoadTime;
	}
	else
	{
		//  different student
		bReload = true;
	}

	if (bReload)
	{
		//  OK, we're doing it.  Lock, load, unlock.
		if (AppLibLock(1000))
		{
			studentMap.RemoveAll();
			studentMap.Read(path);

			unsigned int now = IKUtil::GetSysTimeSecs1970();
			studentLastLoadTime = now;

			studentLastGroup  = group;
			studentLastStudent = student;

			AppLibUnlock();
		}
	}
}


static void ReloadFiles ()
{
	//  not if we're currently deferring
	if (bSaveDeferred)
		return;

	//  see if we're supposed to do this now.
	unsigned int systemsModTime = IKFile::GetModTimeSecs1970(systemsPath);
	unsigned int applicationsModTime = IKFile::GetModTimeSecs1970(applicationsPath);
	unsigned int disallowedModTime = IKFile::GetModTimeSecs1970(disallowedPath);
	unsigned int interestModTime = IKFile::GetModTimeSecs1970(interestPath);
	if (systemsModTime>systemsLastLoadTime || 
	    applicationsModTime>applicationsLastLoadTime ||
	    interestModTime>interestLastLoadTime ||
	    disallowedModTime>applicationsLastLoadTime )
	{
		//  OK, we're doing it.  Lock, load, unlock.
		if (AppLibLock(1000))
		{
			unsigned int now = IKUtil::GetSysTimeSecs1970();

			systemsMap.RemoveAll();
			systemsMap.Read(systemsPath);
			systemsLastLoadTime = now;

			applicationsMap.RemoveAll();
			applicationsMap.Read(applicationsPath);
			applicationsLastLoadTime = now;
			
			interestMap.RemoveAll();
			interestMap.Read(interestPath);
			interestLastLoadTime = now;
			
			disallowedMap.RemoveAll();
			disallowedMap.Read(disallowedPath);
			disallowedLastLoadTime = now;

			AppLibUnlock();
		}
	}
}

static void SaveFiles ()
{
	//  are we deferring?
	if (bSaveDeferred)
	{
		bSaveNeeded = true;
		return;
	}

	if (AppLibLock(1000))
	{
		unsigned int now = IKUtil::GetSysTimeSecs1970();

		systemsMap.Write(systemsPath);				systemsLastLoadTime = now;
		applicationsMap.Write(applicationsPath);	applicationsLastLoadTime = now;
		interestMap.Write(interestPath);			interestLastLoadTime = now;
		disallowedMap.Write(disallowedPath);		disallowedLastLoadTime = now;

		IKFile::MakeWritable(systemsPath);
		IKFile::MakeWritable(applicationsPath);
		IKFile::MakeWritable(interestPath);
		IKFile::MakeWritable(disallowedPath);

		AppLibUnlock();
	}
}

static void SaveStudentFiles (TCHAR * GroupName, TCHAR * StudentName)
{
	//  are we deferring?
	if (bSaveDeferred)
	{
		//  are we still on the same student?
		IKString group   = GroupName;
		IKString student = StudentName;
		if (group.CompareNoCase(studentLastGroup)==0 && student.CompareNoCase(studentLastStudent)==0)
		{
			bStudentSaveNeeded = true;
			return;
		}
	}
	
	//  make a path to the student's file
	IKString group(GroupName);
	IKString student(StudentName);
	IKString path = MakeStudentAppPrefFilePath(group,student);

	if (AppLibLock(1000))
	{
		unsigned int now = IKUtil::GetSysTimeSecs1970();
		studentMap.Write(path);
		studentLastLoadTime = now;
		IKFile::MakeWritable(path);

		AppLibUnlock();
	}
}

static bool AppRecorded ( TCHAR *pAppPath )
{
	IKString AppPath = pAppPath;
#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif

	//  see if the app is in the database
	int nApps = applicationsMap.Count();
	if(nApps<=0)
		return false;

	//  encode the path
	IKString encoded = AppPath;
	encoded.EncodeSpaces();

	for (int i=0;i<nApps;i++)
	{
		IKString key, value;
		if (applicationsMap.GetNthPair(i, key, value))
		{
			//  is this it?
			if (encoded.CompareNoCase(key)==0)
			{
				return true;
			}
		}
	}

	//  not found
	return false;
}

static bool AppRecordedForStudent ( TCHAR *pAppPath  )
{
	IKString AppPath = pAppPath;
#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif
	
	//  see if the app is in the database
	int nApps = studentMap.Count();
	if(nApps<=0)
		return false;

	//  encode the path
	IKString encoded = AppPath;
	encoded.EncodeSpaces();

	for (int i=0;i<nApps;i++)
	{
		IKString key, value;
		if (studentMap.GetNthPair(i, key, value))
		{
			//  is this it?
			if (encoded.CompareNoCase(key)==0)
			{
				return true;
			}
		}
	}

	//  not found
	return false;
}

static void AppLibInitialize ()
{
	//  only do this once.
	static bool bInitialized = false;
	if (bInitialized)
		return;
	bInitialized = true;

	//  OK, do stuff.
	IKUtil::Initialize();
	
#ifdef PLAT_WINDOWS
#else
//	IKMessage::Initialize();
#endif
	
	//  make strings for the files.
	// TODO:  this breaks if IK USB is not installed.
	
	systemsPath			= IKUtil::GetPrivateFolder();
	systemsPath			+= systemsFileName;
	
	applicationsPath	= IKUtil::GetPrivateFolder();
	applicationsPath	+= appsFileName;
	
	interestPath		= IKUtil::GetPrivateFolder();
	interestPath		+= interestFileName;
	
	disallowedPath		= IKUtil::GetPrivateFolder();
	disallowedPath		+= disallowedFileName;
	
	lockPath			= IKUtil::GetPrivateFolder();
	lockPath			+= lockFileName;
	
	//  create the lock file first
	pLockFile = new IKFile;
	IKFile f;
	if (f.Open(lockPath, IKFile::modeReadWrite|IKFile::modeNoTruncate|IKFile::modeCreate))
	{
		f.Close();
	}
	IKFile::MakeWritable(lockPath);

	//  create the other files
	if (AppLibLock(1000))
	{
		//  create the files if they don't exist
		if (!IKFile::FileExists(systemsPath))
		{
			if (f.Open(systemsPath, IKFile::modeWrite|IKFile::modeCreate))
				f.Close();
		}
		if (!IKFile::FileExists(applicationsPath))
		{
			if (f.Open(applicationsPath, IKFile::modeWrite|IKFile::modeCreate))
				f.Close();
		}
		if (!IKFile::FileExists(interestPath))
		{
			if (f.Open(interestPath, IKFile::modeWrite|IKFile::modeCreate))
				f.Close();
		}
		if (!IKFile::FileExists(disallowedPath))
		{
			if (f.Open(disallowedPath, IKFile::modeWrite|IKFile::modeCreate))
				f.Close();
		}

		//  make 'em writable
		IKFile::MakeWritable(systemsPath);
		IKFile::MakeWritable(applicationsPath);
		IKFile::MakeWritable(interestPath);
		IKFile::MakeWritable(disallowedPath);

		AppLibUnlock();
	}

}

static void AddInterest ( const TCHAR *pAppPath, const TCHAR *SystemName )
{
	IKString AppPath = pAppPath;
#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif
	
	//  first encode spaces in the app path
	IKString encoded;
	encoded = AppPath;
	encoded.EncodeSpaces();
	
	//  get the interest list
	IKString mapped = interestMap.Lookup(encoded);
	IKOverlayList list(mapped);

	//  add this to the list
	list.AddEntry(IKString(SystemName));

	//  change the map.
	IKString value = list.GetEncodedList();
	interestMap.Add(encoded,value);
}

static void RemoveInterest ( TCHAR *pAppPath, TCHAR *SystemName )
{
	IKString AppPath = pAppPath;
#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif
	
	//  first encode spaces in the app path
	IKString encoded;
	encoded = AppPath;
	encoded.EncodeSpaces();
	
	//  get the interest list
	IKString mapped = interestMap.Lookup(encoded);
	IKOverlayList list(mapped);
	
	//  remove us
	list.RemoveEntry(IKString(SystemName));
	
	//  change the map.
	IKString value = list.GetEncodedList();
	interestMap.Add(encoded,value);
}

static void RemoveAllInterest ( const TCHAR *SystemName )
{	
	//  how many entries?
	int nEntries = interestMap.Count();
	
	//  look at each entry and remove the system
	for (int i=0;i<nEntries;i++)
	{
		//  get key/value pair
		IKString key, value;
		interestMap.GetNthPair(i, key, value);
		
		//  get the interest list
		IKOverlayList list(value);
		
		//  remove the entry
		list.RemoveEntry(IKString(SystemName));	
		
		//  put it back	
		value = list.GetEncodedList();
		interestMap.Add(key,value);
	}

}

static bool HasInterest ( TCHAR *pAppPath, TCHAR *SystemName )
{
	IKString AppPath = pAppPath;
#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif
	
	//  first encode spaces in the app path
	IKString encoded;
	encoded = AppPath;
	encoded.EncodeSpaces();
	
	//  get the interest list
	IKString mapped = interestMap.Lookup(encoded);
	IKOverlayList list(mapped);
	
	//  find our system in the list
	IKString s = SystemName;
	bool bFound = ( list.Find(s) != -1 );
	
	return bFound;
}


//-------------------------------------------------------------------
//
//  exposed functions

int AppLibAddSystem ( const TCHAR * SystemName, const TCHAR * IdentifyingFileName )
{
	AppLibInitialize();
	ReloadFiles();

	//  first encode spaces in the system name
	IKString encoded;
	encoded = SystemName;
	encoded.EncodeSpaces();

	//  look it up.
	IKString value = systemsMap.Lookup(encoded);
	if (!value.IsEmpty())
	{
		//  already there
		return APPLIB_SYSTEM_ALREADY_ADDED;
	}

	//  add it
	bool bAdded = systemsMap.Add(encoded,IKString(IdentifyingFileName));
	if (!bAdded)
		return APPLIB_ERROR;

	//  save the files
	SaveFiles();

	return APPLIB_SYSTEM_ADDED;
}

//  AppLibCanListApplication returns true if the app should
//  be shown in the UI, and
//  false if it should not. 

//  the list comes from a set of text file items
//  of the form S_SYSTRAY_SKIP_APP_nnn.

bool AppLibCanListApplication ( TCHAR * AppName )
{
	//  init
	AppLibInitialize();

	//  make the list of skippable apps
	//  just once.
	static bool bGotList = false;
	static IKStringArray list;
	if (!bGotList)
	{
		int i=0;
		while (true)
		{
			i++;
			IKString key;
			key.Format("S_SYSTRAY_SKIP_APP_%d",i);
			IKString name = DATAS(key,TEXT(""));
			if (name.IsEmpty())
				break;
			name.Lower();
			list.Add(name);
		}
		bGotList = true;
	}

	//  get the friendly name of the given app
	IKString s(AppName);
	IKString friendly = IKUtil::GetAppFriendlyName(s);
	friendly.Lower();

	//  consult the list
	for (int i=0;i<list.GetSize();i++)
	{
		if (friendly.Find(list.GetAt(i))>=0)
			return false;
	}

	return true;
}

int AppLibRemoveSystem ( const TCHAR * SystemName )
{
	AppLibInitialize();
	ReloadFiles();

	//  encode it
	IKString encoded(SystemName);
	encoded.EncodeSpaces();

	//  look it up as a doublecheck
	IKString s = systemsMap.Lookup(encoded);
	if (s.IsEmpty())
		return APPLIB_SYSTEM_ALREADY_REMOVED;

	//  remove it
	systemsMap.Remove(encoded);
	
	//  remove from interest for all apps
	RemoveAllInterest (SystemName);

	//  save and reload
	SaveFiles();
	ReloadFiles();

	//  look it up as a doublecheck
	s = systemsMap.Lookup(encoded);
	if (!s.IsEmpty())
		return APPLIB_ERROR;

	return APPLIB_SYSTEM_REMOVED;
}

int AppLibCountSystems ()
{
	AppLibInitialize();
	ReloadFiles();

	return systemsMap.Count();
}

TCHAR * AppLibGetNthSystem ( int n )
{
	AppLibInitialize();
	ReloadFiles();

	static IKString key, value;
	if (!systemsMap.GetNthPair(n,key,value)) {
		key = BlankString;
    }
    
	key.DecodeSpaces();
	TCHAR *result = (TCHAR *)key;
	return result;
}

int AppLibAddApplication ( const TCHAR * pAppPath )
{
	IKString AppPath = pAppPath;
#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif
	
	AppLibInitialize();
	ReloadFiles();

	//  is it already here?
	if (AppRecorded(AppPath))
		return APPLIB_APP_ALREADY_ADDED;

	//  does it exist?
	if (!IKFile::FileExists(AppPath))
		if (!bAllowMissingApps)
			return APPLIB_ERROR;

	//  encode spaces in the app name
	IKString encoded = AppPath;
	encoded.EncodeSpaces();

	//  add it
	bool bAdded = applicationsMap.Add(encoded,IKString(TEXT("")));
	if (!bAdded)
		return APPLIB_ERROR;

	//  save the files
	SaveFiles();

	return APPLIB_APP_ADDED;
}

int AppLibRemoveApplication ( const TCHAR * pAppPath, const TCHAR * SystemName )
{
	IKString AppPath = pAppPath;
#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif
	
	AppLibInitialize();
	ReloadFiles();

	//  remove it
	IKString encoded(AppPath);
	encoded.EncodeSpaces();
	applicationsMap.Remove(encoded);
	
	//  TOOD:  remove from interest

	//  save and reload
	SaveFiles();
	ReloadFiles();

	//  look it up as a doublecheck
	IKString s = applicationsMap.Lookup(encoded);
	if (!s.IsEmpty())
		return APPLIB_ERROR;

	return APPLIB_APP_REMOVED;
}

int AppLibTakeOwnership ( const TCHAR * pAppPath, const TCHAR * SystemName )
{
	IKString AppPath = pAppPath;
#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif
	
	AppLibInitialize();
	ReloadFiles();

	//  must be added first
	if (!AppRecorded(AppPath))
		return APPLIB_ERROR;

	//  do I already own this?
	int result = AppLibHasOwnership ( AppPath, SystemName );
	if (result==APPLIB_APP_OWNED)
		return APPLIB_ALREADY_OWNED;

	//  encode the app
	IKString encoded(AppPath);
	encoded.EncodeSpaces();

	//  take it
	if (!applicationsMap.Add(encoded,SystemName))
		return APPLIB_ERROR;
		
	//  add to interest
	AddInterest ( AppPath, SystemName );

	SaveFiles();

	//  reload and check ownership
	ReloadFiles();
	result = AppLibHasOwnership ( AppPath, SystemName );
	if (result != APPLIB_APP_OWNED)
		return APPLIB_ERROR;

	return APPLIB_TOOK_OWNERSHIP;
}

int AppLibHasOwnership ( const TCHAR * pAppPath, const TCHAR * SystemName )
{
	IKString AppPath = pAppPath;
#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif
	
	AppLibInitialize();
	ReloadFiles();

	//  must be added first
	if (!AppRecorded(AppPath))
		return APPLIB_ERROR;

	//  encode the app
	IKString encoded(AppPath);
	encoded.EncodeSpaces();

	//  look it up
	IKString value = applicationsMap.Lookup(encoded);
	if (value.IsEmpty())
		return APPLIB_APP_NOT_OWNED;

	//  is the owner me?
	if (value.CompareNoCase(SystemName)!=0)
		return APPLIB_APP_NOT_OWNED;

	return APPLIB_APP_OWNED;
}

TCHAR * AppLibGetOwner ( const TCHAR * pAppPath )
{	
	IKString AppPath = pAppPath;
#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif
	
	AppLibInitialize();
	ReloadFiles();

	//  encode the path
	IKString encoded = AppPath;
	encoded.EncodeSpaces();

	//  look it up
	static IKString value;
	value = applicationsMap.Lookup(encoded);

	//  return it
	TCHAR *result = (TCHAR *)value;
	return result;
}

int AppLibReleaseOwnership ( const TCHAR * pAppPath, const TCHAR * SystemName )
{
	IKString AppPath = pAppPath;
#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif
	
	AppLibInitialize();
	ReloadFiles();

	//  must be added first
	if (!AppRecorded(AppPath))
		return APPLIB_ERROR;

	//  cannot release if I do not own.
	int result = AppLibHasOwnership ( AppPath, SystemName );
	if (result!=APPLIB_APP_OWNED)
		return APPLIB_RELEASE_NOT_OWNER;

	//  encode the app
	IKString encoded(AppPath);
	encoded.EncodeSpaces();

	//  release it
	if (!applicationsMap.Add(encoded,TEXT("")))
		return APPLIB_ERROR;
		
	//  remove from interest
	//RemoveInterest ( AppPath, SystemName );

	SaveFiles();

	//  reload and check ownership.  Should be blank
	ReloadFiles();

	IKString value = applicationsMap.Lookup(encoded);
	if (!value.IsEmpty())
		return APPLIB_ERROR;

	return APPLIB_RELEASED_OWNERSHIP;
}

int AppLibCountApplications ()
{
	AppLibInitialize();
	ReloadFiles();

	return applicationsMap.Count();
}

TCHAR * AppLibGetNthApplication ( int n )
{
	AppLibInitialize();
	ReloadFiles();

	static IKString key, value;
	if (!applicationsMap.GetNthPair(n,key,value)) {
	//	return (BlankString);
        key = BlankString;
    }

	key.DecodeSpaces();
	TCHAR *result = (TCHAR *)key;
	return result;
}

bool AppLibLock ( int timeout )
{
	AppLibInitialize();

	if (lockCount>0)
		return true;

	//  attempt to exclusively open a file within the timeout period.
	unsigned int start = IKUtil::GetCurrentTimeMS();
	while (start+timeout > IKUtil::GetCurrentTimeMS())
	{
		if (!!pLockFile->Open(lockPath, IKFile::modeReadWrite | IKFile::modeNoTruncate | IKFile::shareExclusive ))
		{
			lockCount++;
			IKASSERT(lockCount==1);

			return true;
		}

		IKUtil::Sleep(50);
	}

	return false;
}

bool AppLibUnlock ()
{
	AppLibInitialize();

	IKASSERT(lockCount==1);

	if (lockCount>0)
	{
		pLockFile->Close();
		lockCount--;
	}

	return true;
}

TCHAR * AppLibGetAppFriendlyName ( const TCHAR * pAppPath )
{
	IKString AppPath = pAppPath;
#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif
	
	AppLibInitialize();
	ReloadFiles();

	static IKString friendly;

	IKString strPath(AppPath);
	friendly = IKUtil::GetAppFriendlyName(strPath);

	return (TCHAR *) friendly;
}

int AppLibMaintenance ()
{
	AppLibInitialize();
	ReloadFiles();

	//  TODO

	return APPLIB_ERROR;
}


TCHAR *	AppLibGetStudentAttachedOverlay	( TCHAR * pAppPath, TCHAR * GroupName, TCHAR * StudentName )
{
	IKString AppPath = pAppPath;
	static IKString value;

#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif
	
	AppLibInitialize();
	ReloadFiles();
	ReloadStudentFiles(GroupName,StudentName);

	//  get the mapped string for this app
	IKString encoded(AppPath);
	encoded.EncodeSpaces();
	IKString mapped = studentMap.Lookup(encoded);
	if (mapped.IsEmpty())  {
		value = BlankString;
        return value;
    }

	//  get the overlay list.  Error if there are non.
	IKOverlayList list(mapped);
	if (list.GetCount()<=0) {
		value = BlankString;
        return value;
    }

	//  get the selected one.  Might be blank, indicating that none is
	//  selected.
	value = list.GetSelectedOverlay();
	return (TCHAR *) value;
}

int AppLibCountAttachedOverlays ( TCHAR * pAppPath, TCHAR * GroupName, TCHAR * StudentName )
{
	IKString AppPath = pAppPath;
#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif
	
	AppLibInitialize();
	ReloadFiles();
	ReloadStudentFiles(GroupName,StudentName);

	//  get the mapped string for this app
	IKString encoded(AppPath);
	encoded.EncodeSpaces();
	IKString mapped = studentMap.Lookup(encoded);
	if (mapped.IsEmpty())
		return -1;  //  indicates that the app is not in the user's list

	//  get the overlay list.
	IKOverlayList list(mapped);

	//  return the number.
	return list.GetCount();
}

int	 AppLibGetSelectedOverlay ( TCHAR * pAppPath, TCHAR * GroupName, TCHAR * StudentName )
{
	IKString AppPath = pAppPath;
#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif
	
	AppLibInitialize();
	ReloadFiles();
	ReloadStudentFiles(GroupName,StudentName);

	//  get the mapped string for this app
	IKString encoded(AppPath);
	encoded.EncodeSpaces();
	IKString mapped = studentMap.Lookup(encoded);
	if (mapped.IsEmpty())
		return 0;

	//  get the overlay list.
	IKOverlayList list(mapped);

	//  tell he caller
	return list.GetNumSelected();

}

TCHAR *	AppLibGetNumberedOverlay ( TCHAR * pAppPath, TCHAR * GroupName, TCHAR * StudentName, int OverlayNumber )
{
	IKString AppPath = pAppPath;
	static IKString value;

#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif
	
	AppLibInitialize();
	ReloadFiles();
	ReloadStudentFiles(GroupName,StudentName);

	//  get the mapped string for this app
	IKString encoded(AppPath);
	encoded.EncodeSpaces();
	IKString mapped = studentMap.Lookup(encoded);
	if (mapped.IsEmpty()) {
		value = BlankString;
        return value;
    }

	//  get the overlay list.
	IKOverlayList list(mapped);

	//  get the numbered value
	value = list.GetNumberedEntry(OverlayNumber);
	return (TCHAR *)value;

}

static void GetCurrentStudent ( IKString &group, IKString &student )
{
	//  load preferences
	IKString prefsFile = IKUtil::GetPrivateFolder();
	prefsFile += DATAS(TEXT("Preferences_File"),TEXT("preferences.txt"));
	IKPrefs prefs;
	prefs.Read(prefsFile);

	//  get group and student name
	group   = prefs.GetValueString(TEXT("Group"),  TEXT(""));
	student = prefs.GetValueString(TEXT("Student"),TEXT(""));
	if (group.IsEmpty() && student.IsEmpty())
		group = DATAS(TEXT("Guest"),TEXT("Guest"));
}

int AppLibAddApplicationForCurrentStudent ( TCHAR * pAppPath )
{
	IKString group, student;
	GetCurrentStudent ( group, student );
	int result = AppLibAddApplicationForStudent ( pAppPath, (TCHAR *)group, (TCHAR *)student );
	return result;
}

int AppLibAddApplicationForStudent ( TCHAR * pAppPath, TCHAR * GroupName, TCHAR * StudentName )
{
	IKString AppPath = pAppPath;
#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif
	
	//  first add globally
	int result = AppLibAddApplication(AppPath);

	AppLibInitialize();
	ReloadFiles();
	ReloadStudentFiles(GroupName,StudentName);

	//  is it already here?
	if (AppRecordedForStudent(AppPath))
		return APPLIB_APP_ALREADY_ADDED;

	//  does it exist?
	if (!IKFile::FileExists(AppPath))
		if (!bAllowMissingApps)
			return APPLIB_ERROR;

	//  encode spaces in the app name
	IKString encoded = AppPath;
	encoded.EncodeSpaces();

	//  add it
	bool bAdded = studentMap.Add(encoded,IKString(TEXT("0|"))); //  if adding the first time, empty list
	if (!bAdded)
		return APPLIB_ERROR;

	//  save the student files
	SaveStudentFiles(GroupName,StudentName);

	return APPLIB_APP_ADDED;

}

int AppLibRemoveApplicationForStudent ( TCHAR * pAppPath, TCHAR * GroupName, TCHAR * StudentName )
{
	IKString AppPath = pAppPath;
#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif
	
	AppLibInitialize();
	ReloadFiles();
	ReloadStudentFiles(GroupName,StudentName);
	
	//  encode spaces in the app name
	IKString encoded = AppPath;
	encoded.EncodeSpaces();
	
	//  remove entry from the student map
	studentMap.Remove(encoded);
	
	//  save the student files
	SaveStudentFiles(GroupName,StudentName);

	return APPLIB_APP_REMOVED;
}



int	AppLibSetSelectedOverlay ( TCHAR * pAppPath, TCHAR * GroupName, TCHAR * StudentName, int OverlayNumber)
{
	IKString AppPath = pAppPath;
#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif
	
	AppLibInitialize();
	ReloadFiles();
	ReloadStudentFiles(GroupName,StudentName);

	//  get the mapped string for this app
	IKString encoded(AppPath);
	encoded.EncodeSpaces();
	IKString mapped = studentMap.Lookup(encoded);
	if (mapped.IsEmpty())
		return APPLIB_ERROR;

	//  get the overlay list.
	IKOverlayList list(mapped);

	//  set t new value
	int result = APPLIB_ERROR;
	if (OverlayNumber>list.GetCount())
	{
		list.SetNumSelected(0);
		result = APPLIB_NO_CURRENT_OVERLAY;
	}
	else
	{
		list.SetNumSelected(OverlayNumber);
		result = APPLIB_CHANGED_CURRENT_OVERLAY;
	}

	//  change the map.
	IKString value = list.GetEncodedList();
	studentMap.Add(encoded,value);

	//  save the student files
	SaveStudentFiles(GroupName,StudentName);

	return result;

}


int AppLibAttachOverlayForStudent ( TCHAR * pAppPath, TCHAR * GroupName, TCHAR * StudentName, TCHAR *OverlayPath )
{
	IKString AppPath = pAppPath;
#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif
	
	AppLibInitialize();
	ReloadFiles();
	ReloadStudentFiles(GroupName,StudentName);

	// does the app exist?
	if (!IKFile::FileExists(AppPath))
		if (!bAllowMissingApps)
			return APPLIB_ERROR;

	//  get the mapped string for this app
	IKString encoded(AppPath);
	encoded.EncodeSpaces();
	IKString mapped = studentMap.Lookup(encoded);
	if (mapped.IsEmpty())
		return APPLIB_ERROR;

	//  get the overlay list.
	IKOverlayList list(mapped);

	//  add this to the list
	list.AddEntry(IKString(OverlayPath));

	//  change the map.
	IKString value = list.GetEncodedList();
	studentMap.Add(encoded,value);

	//  save the student files
	SaveStudentFiles(GroupName,StudentName);

	return APPLIB_ATTACHED;
}

int AppLibDetachOverlayForStudent ( TCHAR * pAppPath, TCHAR * GroupName, TCHAR * StudentName, TCHAR *OverlayPath )
{
	IKString AppPath = pAppPath;
#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif
	
	AppLibInitialize();
	ReloadFiles();
	ReloadStudentFiles(GroupName,StudentName);

	//  get the mapped string for this app
	IKString encoded(AppPath);
	encoded.EncodeSpaces();
	IKString mapped = studentMap.Lookup(encoded);
	if (mapped.IsEmpty())
		return APPLIB_ERROR;

	//  get the overlay list.
	IKOverlayList list(mapped);
	
	//  make a new list from the old one with the given
	//  overlay removed.
	IKOverlayList newList;
	for (int i=0;i<list.GetCount();i++)
	{
		IKString value;
		bool bSelected;
		list.GetEntry(i+1,value,bSelected);
		if (value.CompareNoCase(OverlayPath)!=0)
		{
			newList.AddEntry(value,bSelected);
		}
	}

	//  change the map.
	IKString newValue = newList.GetEncodedList();
	studentMap.Add(encoded,newValue);

	//  save the student files
	SaveStudentFiles(GroupName,StudentName);

	return APPLIB_ATTACHED;
}

bool AppLibIsSystemPresent ( TCHAR * SystemName )
{
	AppLibInitialize();
	ReloadFiles();

	int nSystems = systemsMap.Count();
	if(nSystems<=0)
		return false;

	//  encode the path
	IKString encoded = SystemName;
	encoded.EncodeSpaces();

	for (int i=0;i<nSystems;i++)
	{
		IKString key, value;
		if (systemsMap.GetNthPair(i, key, value))
		{
			//  is this it?
			if (encoded.CompareNoCase(key)==0)
			{
				//  does the identifying file exist?
				if (IKFile::FileExists ( value ))
					return true;
			}
		}
	}

	//  not found
	return false;
}


void AppLibAddDisallowedApps ()
{
	AppLibInitialize();
	ReloadFiles();

	AppLibDeferSavingBlock blk;

	//  where is the folder?
	IKString preinstallFolder = IKUtil::GetPrivateFolder();
	preinstallFolder += DATAS(TEXT("APP_OVERLAYS_SRC_FOLDER"),TEXT("Application Overlays"));
	preinstallFolder += IKUtil::GetPathDelimiter();

	//  what's the map called?
	IKString mapFile = preinstallFolder;
	mapFile += DATAS(TEXT("APP_DISALLOWED_SRC_MAP"),TEXT("disallowed.txt"));

	//  load the map file
	IKMap map;
	bool bRead = map.Read(mapFile);
	if (!bRead)
		return;
	int nApps = map.Count();
	if (nApps<=0)
		return;

	//  enumerate the apps in the map file
	for (int i=0;i<nApps;i++)
	{
		IKString appPath, encodedOverlays;
		if (map.GetNthPair(i,appPath,encodedOverlays))
		{
			//  add the app globally
			IKString path = appPath;
			path.DecodeSpaces();
			AppLibSetAppAllowed ( path, false );
		}
	}
}


#ifdef PLAT_MACINTOSH_X

#include <pthread.h>

static char command[1024];

static void* myThreadFunction(void* input)
{
	int result = system(command);
	return 0;
}

#endif

void AppLibFloatingMessage ( TCHAR * Message )
{
#if 0  //  NO FLOATING MESSAGE 7/25/06
	
#ifdef PLAT_WINDOWS

	IKString command;
	command = IKUtil::GetPrivateFolder();
	command += TEXT("Balloon.exe");
	command += " ";
	command += Message;

	//  start the program
	STARTUPINFO startupInfo;
	memset( &startupInfo, 0, sizeof ( startupInfo ) );
	startupInfo.cb = sizeof ( startupInfo );
	PROCESS_INFORMATION processInfo;
	BOOL bStart = CreateProcess ( NULL, (TCHAR *)command, NULL, NULL, 
		                          false, HIGH_PRIORITY_CLASS,
								  NULL, NULL, &startupInfo, 
								  &processInfo );
	if(bStart)
	{
		::CloseHandle ( processInfo.hThread );
		::CloseHandle ( processInfo.hProcess );
	}

#else

#ifdef PLAT_MACINTOSH_X
	//  create a command string
	strcpy(command,"");
	
	strcat(command,"\"");
	strcat(command,"/Applications/IntelliTools/IntelliKeys USB/Private/Balloon.app/Contents/MacOS/Balloon");
	strcat(command,"\"");
	
	strcat(command," ");
	strcat(command,"\"");
	strcat(command,Message);
	strcat(command,"\"");
	
	//  create a thread for the system command
	pthread_t		myThread;
    pthread_create(&myThread,NULL,myThreadFunction,NULL);

#else
	IKString title = DATAS(TEXT("intellikeys_usb"),TEXT("Intellikeys USB"));
	IKUtil::DisplayAlert(title,Message);
#endif
	
#endif

#endif  //  if 0

}

static const char *pMsgWndName = "IKUSB Balloon Messaging Window";

void AppLibKillFloatingMessage ()
{
#ifdef PLAT_WINDOWS

	//  find the window and post a message to it
	TCHAR *channel = TEXT("balloon");
	if (IKMessage::IsOwnerAlive(channel))
	{
		HWND hwnd = ::FindWindow(pMsgWndName,NULL);
		if (hwnd)
		{
			static DWORD lastTry = 0;
			DWORD now = IKUtil::GetCurrentTimeMS();
			if (now-lastTry>1000)
				::PostMessage(hwnd,WM_CLOSE,0,0);
			lastTry = now;
		}
	}

#else

	const TCHAR *channel = TEXT("balloon");
	if (IKMessage::IsOwnerAlive(channel))
		IKMessage::Send(channel,kQueryKillBalloon,0,0,0,0);
#endif
}

static bool IsAppAllowed ( IKString path )
{
	//  encode path
	IKString encoded = path;
	encoded.EncodeSpaces();

	//  see if this app is one of them
	int nApps = disallowedMap.Count();
	for (int i=0;i<nApps;i++)
	{
		IKString key, value;
		if (disallowedMap.GetNthPair(i, key, value))
		{
			//  is this it?
			if (encoded.CompareNoCase(key)==0)
			{
				return false;
			}

			//  does it match partially?
			IKString part = encoded.Right(key.GetLength());
			if (part.CompareNoCase(key)==0)
			{
				return false;
			}
		}
	}

	//  not found
	return true;
}

bool AppLibIsAppAllowed ( TCHAR * pAppPath )
{
	//  get ready
	AppLibInitialize();
	ReloadFiles();

	//  return true if there are no disallowed apps
	int nApps = disallowedMap.Count();
	if(nApps<=0)
		return true;
		
	//  do the check
	IKString AppPath = pAppPath;
#ifdef PLAT_WINDOWS
	return IsAppAllowed(AppPath);
#else
	//  for mac, check both forms.
	
	bool b1 = true;
	bool b2 = true;
	if (AppPath.GetAt(0)=='/')
	{
		b1 = IsAppAllowed(AppPath);
		AppPath = IKFile::UnixToHFS (AppPath);
		b2 = IsAppAllowed(AppPath);
	}
	else
	{
		b1 = IsAppAllowed(AppPath);
		AppPath = IKFile::HFSToUnix (AppPath);
		b2 = IsAppAllowed(AppPath);
	}

	return (b1 && b2);
#endif

}

void AppLibSetAppAllowed ( TCHAR * AppPath, bool bAllowed )
{
	AppLibInitialize();
	ReloadFiles();
	
	//  encode the path
	IKString encoded = AppPath;
	encoded.EncodeSpaces();

	if (bAllowed)
	{
		//  should be allowed, so remove from list
		disallowedMap.Remove(encoded);
	}
	else
	{
		//  should NOT be allowed, so add it.
		disallowedMap.Add ( encoded, "" );
	}
	
	//  save.
	SaveFiles();
}

static bool bStandardAppsLoaded = false;
static IKStringArray standardApps;

#ifdef PLAT_WINDOWS

#define IS_KEY_LEN 256

static IKString ProgramFilesDir ()
{
	DWORD lRet;
	HKEY hKey;

	IKString value;

	lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion"), 0, KEY_READ, &hKey);
	if (lRet==ERROR_SUCCESS)
	{
		
		TCHAR szProgramFiles[IS_KEY_LEN];
		DWORD dwSize = sizeof(szProgramFiles);
		DWORD dwType;
		lRet = RegQueryValueEx(hKey, TEXT("ProgramFilesDir"), NULL, &dwType, (LPBYTE)&szProgramFiles, &dwSize);
		if (lRet==ERROR_SUCCESS)
		{
			value = szProgramFiles;
		}

		RegCloseKey(hKey);
	}

	return value;
}


static void EnumerateApps ()
{
	IKString strProgramFiles = ProgramFilesDir();

    TCHAR wpath[255];
	::GetWindowsDirectory(wpath,255);
	if (wpath[IKString::strlen(wpath)-1]!=TEXT('\\'))
		IKString::strcat(wpath,TEXT("\\"));
	IKString strSystemRoot   = wpath;

	DWORD lRet;
	HKEY hKey;

	for (int npass=0;npass<2;npass++)
	{
		if (npass==0)
			lRet = RegOpenKeyEx(HKEY_CLASSES_ROOT, TEXT("Applications"), 0, KEY_READ, &hKey);
		else
			lRet = RegOpenKeyEx(HKEY_CLASSES_ROOT, NULL,                 0, KEY_READ, &hKey);


		if (lRet==ERROR_SUCCESS)
		{
			DWORD dwIndex = 0;
			DWORD cbName = IS_KEY_LEN;
			TCHAR szSubKeyName[IS_KEY_LEN];
			while ((lRet=RegEnumKeyEx(hKey, dwIndex, szSubKeyName, &cbName, NULL, NULL, NULL, NULL)) != ERROR_NO_MORE_ITEMS)
			{
				IKString key = szSubKeyName;
				key += "\\shell\\Open\\command";
				HKEY hItem;
				lRet = RegOpenKeyEx(hKey, (TCHAR *)key, 0, KEY_READ, &hItem);
				if (lRet== ERROR_SUCCESS)
				{
					TCHAR szDisplayName[IS_KEY_LEN];
					DWORD dwSize = sizeof(szDisplayName);
					DWORD dwType;
					lRet = RegQueryValueEx(hItem, NULL, NULL, &dwType, (LPBYTE)&szDisplayName, &dwSize);
					if (lRet==ERROR_SUCCESS)
					{
						//  get the first parameter
						IKString s = szDisplayName;

						IKString s2 = s;
						s2.Lower();

						int i = s2.Find(".exe");

						if (i>=0)
							s = s.Left(i+4);
						if (s.GetAt(0)=='\"')
							s = s.Mid(1);

						if (IKUtil::IsWin2KOrGreater())
						{
							if (s2.Find("notepad.exe")>=0)
							{
								s = TEXT("%systemroot%\\system32\\notepad.exe");
							}
						}

						s.Substitute(TEXT("%programfiles%"),strProgramFiles);
						s.Substitute(TEXT("%systemroot%"),  strSystemRoot);
						s.Substitute(TEXT("\\\\"),  TEXT("\\"));

						bool bInclude = true;
						if (s.CompareNoCase(TEXT("rundll32.exe"))==0)
							bInclude = false;
						if (s.CompareNoCase(TEXT("rundll32"))==0)
							bInclude = false;
						if (s.Left(2)=="%1")
							bInclude = false;

						if (bInclude)
						{
							if (IKFile::FileExists(s))
							{
								IKUtil::GetTrueFileName (s);
								if (standardApps.Find(s)<0)
									standardApps.Add(s);

							}
						}
					}
				}

				dwIndex++;
				cbName = IS_KEY_LEN;
			}

			RegCloseKey(hKey);
		}
	
	}


	//  more apps.

	IKString path = IKUtil::GetPrivateFolder();
	path += DATAS(TEXT("APP_OVERLAYS_SRC_FOLDER"),TEXT("Application Overlays"));
	path += IKUtil::GetPathDelimiter();
	path += "MoreApps.txt";
	IKMap map;
	map.Read(path);

	int nApps = map.Count();
	if (nApps>0)
	{
		//  enumerate the apps in the map file
		for (int i=0;i<nApps;i++)
		{
			IKString key, value;
			map.GetNthPair(i,key,value);
			key.DecodeSpaces();

			key.Substitute(TEXT("%programfiles%"),strProgramFiles);
			key.Substitute(TEXT("%systemroot%"),  strSystemRoot);						
			key.Substitute(TEXT("\\\\"),  TEXT("\\"));

			if (IKFile::FileExists(key))
			{
				IKUtil::GetTrueFileName (key);
				if (standardApps.Find(key)<0)
					standardApps.Add(key);
			}
		}
	}

}

#else

void GetFullPath (long DirID, short vRefNum, TCHAR *path);

static Boolean MyIterateFilter
(
	Boolean containerChanged,
	ItemCount currentLevel,
	const FSCatalogInfo *catalogInfo,
	const FSRef *ref,
	const FSSpec *spec,
	const HFSUniStr255 *name,
	void *yourDataPtr)
{
	IKString *pArray = (IKString *)yourDataPtr;
	
	//  URL from FSRef
	CFURLRef url = CFURLCreateFromFSRef ( kCFAllocatorDefault, ref );
	
	//  file system rep
	TCHAR buffer[255];
	Boolean b = CFURLGetFileSystemRepresentation ( url, true, (unsigned char *)buffer, 255L );
	
	//  HFS
	TCHAR hfs[255];
	GetFullPath ( spec->parID, spec->vRefNum, hfs );
	int l = IKString::strlen(hfs);
	for (int i=0;i<spec->name[0];i++)
	{
		hfs[l+i] = spec->name[i+1];
		hfs[l+i+1] = 0;
	}
	IKString strHFS = hfs;
	
	//  is this a package?
	IKString pkgFile = strHFS;
	pkgFile += (const char *) ":Contents:PkgInfo";
	if (IKFile::FileExists(pkgFile))
	{
		standardApps.Add (strHFS);
	}
	
	return false;
}

static void EnumerateApps ()
{
	//  start here
	const char *start = "/Applications/";
	CFStringRef startPath = CFStringCreateWithCString(kCFAllocatorDefault,start,kCFStringEncodingUTF8);

//    NSLog(@"AppLib::EnumerateApps startPath %@", startPath);
    
	//  make an FSRef out of the start
	FSRef startRef;
	CFURLRef startURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, startPath, kCFURLPOSIXPathStyle, false);
	CFURLGetFSRef(startURL, &startRef);
	
//	IKStringArray array;
//	OSErr err = FSIterateContainer ( &startRef, 1, 
//									kFSCatInfoFinderInfo|kFSCatInfoFinderXInfo,
//									true, true, (IterateContainerFilterProcPtr)MyIterateFilter, (void *)&array );

}

#endif

static void InitStandardApps()
{
	if (!bStandardAppsLoaded)
	{
		//  get from the system
		EnumerateApps();

		//  add any that users have added
		//  that also exist
		int n = AppLibCountApplications();
		for (int i=0;i<n;i++)
		{
			IKString app = AppLibGetNthApplication(i);
			if (standardApps.Find(app)<0)
				if (IKFile::FileExists(app))
					standardApps.Add(app);
		}

		bStandardAppsLoaded= true;
	}
}

int AppLibCountStandardApps ( )
{
	InitStandardApps();

	return standardApps.GetSize();

	return 0;
}

TCHAR * AppLibGetNthStandardApp ( int n )
{
	static IKString s;

	InitStandardApps();

	if (n>=standardApps.GetSize()) {
		s = BlankString;
        return s;
    }

	IKString entry = standardApps[n];
	int i = entry.Find('|');

	s = entry.Mid(i+1);
	return (TCHAR *)s;
}


TCHAR * AppLibGetTruePath ( TCHAR * pAppPath )
{
	IKString AppPath = pAppPath;
#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif
	
	static IKString s;
	s = AppPath;
	IKUtil::GetTrueFileName(s);
	return (TCHAR *)s;
}

void AppLibSetSystemInterest ( TCHAR * AppPath, TCHAR * SystemName, bool bInterest )
{
	AppLibInitialize();
	ReloadFiles();

	if (bInterest)
		AddInterest ( AppPath, SystemName );
	else
		RemoveInterest ( AppPath, SystemName );
		
	SaveFiles();
}

bool AppLibHasSystemInterest ( TCHAR * pAppPath, TCHAR * SystemName )
{
	IKString AppPath = pAppPath;
#ifdef PLAT_MACINTOSH
	if (AppPath.GetAt(0)=='/')
		AppPath = IKFile::UnixToHFS (AppPath);
#endif
	
	AppLibInitialize();
	ReloadFiles();

	IKString s = SystemName;
		
	if (s.CompareNoCase(TEXT("IntelliKeys USB"))==0)
		return true;
	
	return HasInterest(AppPath,SystemName);
}

void AppLibDeferSaving ( bool bDefer )
{
	if (bDefer)
	{
		//  defer requested
		bSaveDeferred = true;
	}
	else
	{
		//  defer cancelled.
		bSaveDeferred = false;
		
		//  if saving was needed, do it
		if (bSaveNeeded)
		{
			SaveFiles();
			bSaveNeeded = false;
		}
		if (bStudentSaveNeeded)
		{
			SaveStudentFiles((TCHAR *)studentLastGroup,(TCHAR *)studentLastStudent);
			bStudentSaveNeeded = false;
		}
		ReloadFiles();
	}
}

bool AppLibIsOverlayPreinstalled ( TCHAR * OverlayPath )
{
	IKString path = OverlayPath;
	path.Lower();
	
	int i = path.Find ( TEXT("private") );
	int j = path.Find ( TEXT("application overlays") );
	
	if (i>=0 && j>=0)
		return true;
		
	return false;	
}

//--------------------------------
//
//  AppLibUpdateAppPath finds references in the database to an application's path,
//  and changes it a new path.
//
int AppLibUpdateAppPath ( TCHAR * oldPath, TCHAR * newPath )
{
	//  refresh databases
	AppLibInitialize();
	ReloadFiles();

	//  encode the paths
	IKString oldKey = oldPath;
	oldKey.EncodeSpaces();
	IKString newKey = newPath;
	newKey.EncodeSpaces();
	
	//  update global app and interest maps
	applicationsMap.ModifyKey(oldKey,newKey);
	interestMap.ModifyKey(oldKey,newKey);
	SaveFiles();
	
	//  update Guest
	IKString path;
	IKMap map;
	path = IKUtil::GetGuestFolder();
	path += (const char *) "applications.txt";
	map.Read(path);
	map.ModifyKey(oldKey,newKey);
	map.Write();
	
	//  update all other users
	IKStringArray groups;
	IKUtil::BuildFolderArray(groups,IKUtil::GetUsersFolder());
	int g;
	for (g=0;g<groups.GetSize();g++)
	{
		IKString group = groups.GetAt(g);
		if (group.Right(7).CompareNoCase(":Guest:")!=0)
		{
			IKStringArray users;
			IKUtil::BuildFolderArray(users,group);
			int u;
			for (u=0;u<users.GetSize();u++)
			{
				path = users.GetAt(u);
				path += (const char *) "applications.txt";
				map.Read(path);
				map.ModifyKey(oldKey,newKey);
				map.Write();				
			}
		}
	}
	
	//  update preinstalls
	IKString start = IKUtil::GetAppOverlaysFolder();
	groups.RemoveAll();
	IKUtil::BuildFolderArray(groups,start);
	for (g=0;g<groups.GetSize();g++)
	{
		path = groups.GetAt(g);
		path += (const char *) "applications.txt";
		map.Read(path);
		map.ModifyKey(oldKey,newKey);
		map.Write();				
	}

	return APPLIB_PATH_UPDATED;
}

bool AppLibIsIntelliSwitchInstalled ( )
{
	IKString path = IKUtil::GetPrivateFolder();
	path += (const char *) "ISInstall.txt";
	return IKFile::FileExists(path);
}

bool AppLibIsIntellikeysInstalled ( )
{
	IKString path = IKUtil::GetPrivateFolder();
	path += (const char *) "ISInstall.txt";
	return IKFile::FileExists(path);
}

void AppLibAddPreinstalledOverlays ( TCHAR * GroupName, TCHAR * StudentName, bool bForce )
{
	AppLibInitialize();
	ReloadFiles();
	ReloadStudentFiles(GroupName,StudentName);

	AppLibDeferSavingBlock blk;
        
	//  where to begin?
	IKString preinstallFolder = IKUtil::GetPrivateFolder();
	preinstallFolder += DATAS(TEXT("APP_OVERLAYS_SRC_FOLDER"),TEXT("Application Overlays"));
	preinstallFolder += IKUtil::GetPathDelimiter();

	// find all of the folders at this level.
	IKStringArray folders;
	IKUtil::BuildFolderArray ( folders, (TCHAR *)preinstallFolder );

	//  consider each folder
	for (int nFolder=0;nFolder<folders.GetSize();nFolder++)
	{
		IKString folder = folders[nFolder];

		//  what's the map called?
		IKString mapFile = folder;
		mapFile += DATAS(TEXT("APP_OVERLAYS_SRC_MAP"),TEXT("applications.txt"));

		//  load the map file
		IKMap map;
		bool bRead = map.Read(mapFile);
		if (bRead)
		{
			int nApps = map.Count();
			if (nApps>0)
			{
				//  enumerate the apps in the map file
				for (int i=0;i<nApps;i++)
				{
					IKString appPath, encodedOverlays;
					if (map.GetNthPair(i,appPath,encodedOverlays))
					{
						//  decode the app path first
						appPath.DecodeSpaces();

						//  TODO: TESTING!!!
						//IKString xx = IKUtil::GetAppFriendlyName ( appPath );

#ifdef PLAT_WINDOWS
						//  if the name begins with '%win%', patch in the
						//  current windows path
						if (appPath.Left(5).CompareNoCase("%win%")==0)
						{
							char path[256];
							GetWindowsDirectory(path,256);
							IKString s = path;
							if (appPath.GetAt(5)!='\\')
								s += "\\";
							s += appPath.Mid(5);
							appPath = s;
						}

						//  if the name begins with '%program files%', patch in the
						//  current program files path
						if (appPath.Left(15).CompareNoCase("%program files%")==0)
						{
							IKString s = ProgramFilesDir();
							if (appPath.GetAt(15)!='\\')
								s += "\\";
							s += appPath.Mid(15);
							appPath = s;
						}

						//  if the name begins with '%root%', patch in the
						//  current hard drive
						if (appPath.Left(6).CompareNoCase("%root%")==0)
						{
							IKString s = "C:";
							if (appPath.GetAt(6)!='\\')
								s += "\\";
							s += appPath.Mid(6);
							appPath = s;
						}
#endif
						
#ifdef PLAT_MACINTOSH_X
						//  if the name begins with "/", translate to HFS.
						if (appPath.GetAt(0)=='/')
						{
							IKString s = IKFile::UnixToHFS ( appPath);
							appPath = s;
						}
#endif

						bAllowMissingApps = true;

						int result;

						int result1 = AppLibAddApplication ( appPath );
						int result2 = AppLibAddApplicationForStudent ( appPath, GroupName, StudentName );

						if (result2 == APPLIB_APP_ADDED || bForce)
						{
							//  if we added it for the student,
							//  thenattach overlays and do ownership.

							//  enumerate the list of overlays
							IKOverlayList list(encodedOverlays);
							int nOverlays = list.GetCount();
							int nSelected = list.GetNumSelected();
							if (nOverlays>0)
							{
								for (int j=0;j<nOverlays;j++)
								{
									IKString overlayName = list.GetNumberedEntry(j+1);
									if (!overlayName.IsEmpty())
									{
										//  add this overlay for this app and student
										IKString overlayPath = folder;
										overlayPath += overlayName;
										result = AppLibAttachOverlayForStudent ( appPath, GroupName, StudentName, overlayPath );
									}

									if (j+1==nSelected)
									{
										//  this one is indicated as selected.
										//  if we can, we should take ownership
										//  and select it for this user.
										IKString owner = AppLibGetOwner(appPath);
										if (owner.IsEmpty() || owner.CompareNoCase("IntelliKeys USB")==0)
										{
											result = AppLibTakeOwnership(appPath,(const char *) "IntelliKeys USB");
											result = ::AppLibSetSelectedOverlay(appPath,GroupName,StudentName,nSelected);
										}
									}
								}
							}
						}

						bAllowMissingApps = false;
					}
				}
			}
		}
	}
}

static void PackString( TCHAR *string, BYTE *data, int *ndata )
{
	data[0] = IKString::strlen(string);
	IKString::strcpy((TCHAR *)&(data[1]), string);
	*ndata = IKString::strlen(string) + 1;
}

int AppLibIsSystemActive ( TCHAR * SystemName )
{
	//  init
	IKUtil::Initialize();
	IKMsg::Initialize();

	//  ask the engine
	BYTE data[1000]; int ndata; PackString(SystemName, data, &ndata);
	int result = IKMsg::Send("engine", kQueryIsSystemActive, data, ndata, 0, 0);

	if (result==kResponseSystemActive)
		return APPLIB_SYSTEM_ACTIVE;

	if (result==kResponseSystemInactive)
		return APPLIB_SYSTEM_INACTIVE;

	return APPLIB_ERROR;
}

int AppLibSetSystemActive ( TCHAR * SystemName )
{
	//  init
	IKUtil::Initialize();
	IKMsg::Initialize();

	//  ask the engine
	BYTE data[1000]; int ndata; PackString(SystemName, data, &ndata);
	int result = IKMsg::Send("engine", kQuerySetSystemActive, data, ndata, 0, 0);

	if (result == kResponseNoError)
		return APPLIB_SYSTEM_ACTIVE;

	return APPLIB_ERROR;
}

int AppLibSetSystemInactive ( TCHAR * SystemName )
{
	//  init
	IKUtil::Initialize();
	IKMsg::Initialize();

	//  ask the engine
	BYTE data[1000]; int ndata; PackString(SystemName, data, &ndata);
	int result = IKMsg::Send("engine", kQuerySetSystemInactive, data, ndata, 0, 0);

	if (result == kResponseNoError)
		return APPLIB_SYSTEM_INACTIVE;

	return APPLIB_ERROR;
}

bool AppLibShowDiscoverAnyway ()
{
	int val = DATAI(TEXT("show_discover_anyway"),0);
	return !!val;
}



