/*
MobileRobots Advanced Robotics Interface for Applications (ARIA)
Copyright (C) 2004, 2005 ActivMedia Robotics LLC
Copyright (C) 2006, 2007, 2008, 2009 MobileRobots Inc.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published by
     the Free Software Foundation; either version 2 of the License, or
     (at your option) any later version.

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
     GNU General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with this program; if not, write to the Free Software
     Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

If you wish to redistribute ARIA under different terms, contact 
MobileRobots for information about a commercial version of ARIA at 
robots@mobilerobots.com or 
MobileRobots Inc, 10 Columbia Drive, Amherst, NH 03031; 800-639-9481
*/

#define _GNU_SOURCE 1 // for isnormal() and other newer (non-ansi) C functions

#include "ArExport.h"
#include "ariaOSDef.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#ifdef WIN32
#include <windows.h>  // for timeGetTime() and mmsystem.h
#include <mmsystem.h> // for timeGetTime()
#else
#include <sys/time.h>
#include <stdarg.h>
#include <unistd.h>
#include <utime.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#endif


#include "ariaInternal.h"
#include "ariaTypedefs.h"
#include "ariaUtil.h"

#include "ArSick.h"
#include "ArUrg.h"
#include "ArLMS1XX.h"

#include "ArSerialConnection.h"
#include "ArTcpConnection.h"

#ifdef WIN32
#include <io.h>
AREXPORT const char *ArUtil::COM1 = "COM1";
AREXPORT const char *ArUtil::COM2 = "COM2";
AREXPORT const char *ArUtil::COM3 = "COM3";
AREXPORT const char *ArUtil::COM4 = "COM4";
AREXPORT const char *ArUtil::COM5 = "COM5";
AREXPORT const char *ArUtil::COM6 = "COM6";
AREXPORT const char *ArUtil::COM7 = "COM7";
AREXPORT const char *ArUtil::COM8 = "COM8";
AREXPORT const char *ArUtil::COM9 = "COM9";
AREXPORT const char *ArUtil::COM10 = "COM10";
AREXPORT const char *ArUtil::COM11 = "COM11";
AREXPORT const char *ArUtil::COM12 = "COM12";
AREXPORT const char *ArUtil::COM13 = "COM13";
AREXPORT const char *ArUtil::COM14 = "COM14";
AREXPORT const char *ArUtil::COM15 = "COM15";
AREXPORT const char *ArUtil::COM16 = "COM16";
#else // ifndef WIN32
AREXPORT const char *ArUtil::COM1 = "/dev/ttyS0";
AREXPORT const char *ArUtil::COM2 = "/dev/ttyS1";
AREXPORT const char *ArUtil::COM3 = "/dev/ttyS2";
AREXPORT const char *ArUtil::COM4 = "/dev/ttyS3";
AREXPORT const char *ArUtil::COM5 = "/dev/ttyS4";
AREXPORT const char *ArUtil::COM6 = "/dev/ttyS5";
AREXPORT const char *ArUtil::COM7 = "/dev/ttyS6";
AREXPORT const char *ArUtil::COM8 = "/dev/ttyS7";
AREXPORT const char *ArUtil::COM9 = "/dev/ttyS8";
AREXPORT const char *ArUtil::COM10 = "/dev/ttyS9";
AREXPORT const char *ArUtil::COM11 = "/dev/ttyS10";
AREXPORT const char *ArUtil::COM12 = "/dev/ttyS11";
AREXPORT const char *ArUtil::COM13 = "/dev/ttyS12";
AREXPORT const char *ArUtil::COM14 = "/dev/ttyS13";
AREXPORT const char *ArUtil::COM15 = "/dev/ttyS14";
AREXPORT const char *ArUtil::COM16 = "/dev/ttyS15";
#endif  // WIN32

AREXPORT const char *ArUtil::TRUESTRING = "true";
AREXPORT const char *ArUtil::FALSESTRING = "false";

// const double eps = std::numeric_limits<double>::epsilon();  
const double ArMath::ourEpsilon = 0.00000001; 

#ifdef WIN32
// max returned by rand()
const long ArMath::ourRandMax = RAND_MAX;
#else
// max returned by lrand48()
const long ArMath::ourRandMax = 2147483648;// 2^31, per lrand48 man page
#endif

#ifdef WIN32
const char  ArUtil::SEPARATOR_CHAR = '\\';
const char *ArUtil::SEPARATOR_STRING = "\\";
const char  ArUtil::OTHER_SEPARATOR_CHAR = '/';
#else
const char  ArUtil::SEPARATOR_CHAR = '/';
const char *ArUtil::SEPARATOR_STRING = "/";
const char  ArUtil::OTHER_SEPARATOR_CHAR = '\\';
#endif

#ifdef WIN32
ArMutex ArUtil::ourLocaltimeMutex;
#endif

/**
   This sleeps for the given number of milliseconds... Note in linux it 
   tries to sleep for 10 ms less than the amount given, which should wind up 
   close to correct... 
   Linux is broken in this regard and sleeps for too long...
   it sleeps for the ceiling of the current 10 ms range,
   then for an additional 10 ms... so:
   11 to 20 ms sleeps for 30 ms...
   21 to 30 ms sleeps for 40 ms...
   31 to 40 ms sleeps for 50 ms... 
   this continues on up to the values we care about of..
   81 to 90 ms sleeps for 100 ms...
   91 to 100 ms sleeps for 110 ms...
   so we'll sleep for 10 ms less than we want to, which should put us about 
   right... guh
   @param ms the number of milliseconds to sleep for
*/
AREXPORT void ArUtil::sleep(unsigned int ms)
{
#ifdef WIN32
  Sleep(ms);
#else // ifndef win32
  if (ms > 10)
    ms -= 10;
  usleep(ms * 1000);
#endif // linux

}

/**
   Get the time in milliseconds, counting from some arbitrary point.
   This time is only valid within this run of the program.
   @return millisecond time
*/
AREXPORT unsigned int ArUtil::getTime(void)
{
// the good unix way
#if defined(_POSIX_TIMERS) && defined(_POSIX_MONOTONIC_CLOCK)
  struct timespec tp;
  if (clock_gettime(CLOCK_MONOTONIC, &tp) == 0)
    return tp.tv_nsec / 1000000 + (tp.tv_sec % 1000000)*1000;
// the old unix way as a fallback
#endif // if it isn't the good way
#if !defined(WIN32)
  struct timeval tv;
  if (gettimeofday(&tv,NULL) == 0)
    return tv.tv_usec/1000 + (tv.tv_sec % 1000000)*1000;//windows
  else
    return 0;
#elif defined(WIN32)
  return timeGetTime();
#endif
}

/*
   Takes a string and splits it into a list of words. It appends the words
   to the outList. If there is nothing found, it will not touch the outList.
   @param inString the input string to split
   @param outList the list in which to store the words that are found
*/
/*
AREXPORT void ArUtil::splitString(std::string inString,
				  std::list<std::string> &outList)
{
  const char *start, *end;

  // Strip off leading white space
  for (end=inString.c_str(); *end && isspace(*end); ++end)
    ;
  while (*end)
  {
    // Mark start of the word then find end
    for (start=end; *end && !isspace(*end); ++end)
      ;
    // Store the word
    if (*start && ((*end && isspace(*end)) || !*end))
      outList.push_back(std::string(start, (int)(end-start)));
    for (; *end && isspace(*end); ++end)
      ;
  }
}
*/



#ifdef WIN32

/**
   @return size in bytes. -1 on error.
   @param fileName name of the file to size
*/
AREXPORT long ArUtil::sizeFile(std::string fileName)
{
  struct _stat buf;

  if (_stat(fileName.c_str(), &buf) < 0)
    return(-1);

  if (!(buf.st_mode | _S_IFREG))
    return(-1);

  return(buf.st_size);
}

/**
   @return size in bytes. -1 on error.
   @param fileName name of the file to size
*/
AREXPORT long ArUtil::sizeFile(const char * fileName)
{
  struct _stat buf;

  if (_stat(fileName, &buf) < 0)
    return(-1);

  if (!(buf.st_mode | _S_IFREG))
    return(-1);

  return(buf.st_size);
}

#else // !WIN32

AREXPORT long ArUtil::sizeFile(std::string fileName)
{
  struct stat buf;

  if (stat(fileName.c_str(), &buf) < 0)
  {
    perror("stat");
    return(-1);
  }

  if (!S_ISREG(buf.st_mode))
    return(-1);

  return(buf.st_size);
}


/**
   @return size in bytes. -1 on error.
   @param fileName name of the file to size
*/
AREXPORT long ArUtil::sizeFile(const char * fileName)
{
  struct stat buf;

  if (stat(fileName, &buf) < 0)
  {
    perror("stat");
    return(-1);
  }

  if (!S_ISREG(buf.st_mode))
    return(-1);

  return(buf.st_size);
}

#endif // else !WIN32

/**
   @return true if file is found
   @param fileName name of the file to size
*/
AREXPORT bool ArUtil::findFile(const char *fileName)
{
  FILE *fp;

  if ((fp=ArUtil::fopen(fileName, "r")))
  {
    fclose(fp);
    return(true);
  }
  else
    return(false);
}

/*
   Works for \ and /. Returns true if something was actualy done. Sets
   fileOut to be what ever the answer is.
   @return true if the path contains a file
   @param fileIn input path/fileName
   @param fileOut output fileName
*/
/*AREXPORT bool ArUtil::stripDir(std::string fileIn, std::string &fileOut)
{
  const char *ptr;

  for (ptr=fileIn.c_str(); *ptr; ++ptr)
    ;
  for (--ptr; (ptr > fileIn.c_str()) && (*ptr != '/') && (*ptr != '\\'); --ptr)
    ;
  if ((*ptr == '/') || (*ptr == '\\'))
  {
    fileOut=ptr+1;
    return(true);
  }
  else
  {
    fileOut=fileIn;
    return(false);
  }
}
*/
/*
   Works for \ and /. Returns true if something was actualy done. Sets
   fileOut to be what ever the answer is.
   @return true if the file contains a path
   @param fileIn input path/fileName
   @param fileOut output path
*/
/*
AREXPORT bool ArUtil::stripFile(std::string fileIn, std::string &fileOut)
{
  const char *start, *end;

  for (start=end=fileIn.c_str(); *end; ++end)
  {
    if ((*end == '/') || (*end == '\\'))
    {
      start=end;
      for (; *end && ((*end == '/') || (*end == '\\')); ++end)
        ;
    }
  }

  if (start < end)
  {
    fileOut.assign(fileIn, 0, start-fileIn.c_str());
    return(true);
  }

  fileOut=fileIn;
  return(false);
}
*/
AREXPORT bool ArUtil::stripQuotes(char *dest, const char *src, size_t destLen)
{
  size_t srcLen = strlen(src);
  if (destLen < srcLen + 1)
  {
    ArLog::log(ArLog::Normal, "ArUtil::stripQuotes: destLen isn't long enough to fit copy its %d should be %d", destLen, srcLen + 1);
    return false;
  }
  // if there are no quotes to strip just copy and return
  if (srcLen < 2 || 
      (src[0] != '"' || src[srcLen - 1] != '"'))
  {
    strcpy(dest, src);
    return true;
  }
  // we have quotes so chop of the first and last char
  strncpy(dest, &src[1], srcLen - 1);
  dest[srcLen - 2] = '\0';
  return true;
}

/** Append a directory separator character to the given path string, depending on the
 * platform.  On Windows, a backslash ('\\') is added. On other platforms, a 
 * forward slash ('/') is appended. If there is no more allocated space in the
 * path string, no character will be appended.
   @param path the path string to append a slash to
   @param pathLength maximum length allocated for path string
*/
AREXPORT void ArUtil::appendSlash(char *path, size_t pathLength)
{
  // first check boundary
  size_t len;
  len = strlen(path);
  if (len > pathLength - 2)
    return;

  if (len == 0 || (path[len - 1] != '\\' && path[len - 1] != '/'))
  {
#ifdef WIN32
    path[len] = '\\';
#else
    path[len] = '/';
#endif
    path[len + 1] = '\0';
  }
}

AREXPORT void ArUtil::appendSlash(std::string &path)
{
  // first check boundary
  size_t len = path.length();
  if ((len == 0) || 
      (path[len - 1] != SEPARATOR_CHAR && path[len - 1] != OTHER_SEPARATOR_CHAR)) {
    path += SEPARATOR_STRING;
  }

} // end method appendSlash

/**
   @param path the path in which to fix the orientation of the slashes
   @param pathLength the maximum length of path
*/
AREXPORT void ArUtil::fixSlashes(char *path, size_t pathLength)
{
#ifdef WIN32
  fixSlashesBackward(path, pathLength);
#else
  fixSlashesForward(path, pathLength);
#endif
}

/**
   @param path the path in which to fix the orientation of the slashes
   @param pathLength how long that path is at max
*/
AREXPORT void ArUtil::fixSlashesBackward(char *path, size_t pathLength)
{
  for (size_t i=0; path[i] != '\0' && i < pathLength; i++)
  {
    if (path[i] == '/')
      path[i]='\\';
  }
}

/**
   @param path the path in which to fix the orientation of the slashes
   @param pathLength how long that path is at max
*/
AREXPORT void ArUtil::fixSlashesForward(char *path, size_t pathLength)
{

  for (size_t i=0; path[i] != '\0' && i < pathLength; i++)
  {
    if (path[i] == '\\')
      path[i]='/';
  }
}

/**
   @param path the path in which to fix the orientation of the slashes
*/
AREXPORT void ArUtil::fixSlashes(std::string &path) 
{
  for (size_t i = 0; i < path.length(); i++)
  {
    if (path[i] == OTHER_SEPARATOR_CHAR)
      path[i]= SEPARATOR_CHAR;
  }
}

/**
   This function will take the 'baseDir' and put the 'insideDir' after
   it so that it winds up with 'baseDir/insideDir/'.  It will take
   care of slashes, making sure there is one between them and one at
   the end, and the slashes will match what the operating system
   expects.

   @param dest the place to put the result
   @param destLength the length of the place to put the results
   @param baseDir the directory to start with
   @param insideDir the directory to place after the baseDir 
**/
AREXPORT void ArUtil::addDirectories(char *dest, size_t destLength, 
				     const char *baseDir,
				     const char *insideDir)
{
  // start it off
  strncpy(dest, baseDir, destLength - 1);
  // make sure we have a null term
  dest[destLength - 1] = '\0';
  // toss on that slash
  appendSlash(dest, destLength);
  // put on the inside dir
  strncat(dest, insideDir, destLength - strlen(dest) - 1);
  // now toss on that slash
  appendSlash(dest, destLength);
  // and now fix up all the slashes
  fixSlashes(dest, destLength);
}

/** 
    This compares two strings, it returns an integer less than, equal to, 
    or greater than zero  if  str  is  found, respectively, to be less than, to
    match, or be greater than str2.
    @param str the string to compare
    @param str2 the second string to compare
    @return an integer less than, equal to, or greater than zero if str is 
    found, respectively, to be less than, to match, or be greater than str2.
*/
AREXPORT int ArUtil::strcmp(std::string str, std::string str2)
{
  return ::strcmp(str.c_str(), str2.c_str());
}

/** 
    This compares two strings, it returns an integer less than, equal to, 
    or greater than zero  if  str  is  found, respectively, to be less than, to
    match, or be greater than str2.
    @param str the string to compare
    @param str2 the second string to compare
    @return an integer less than, equal to, or greater than zero if str is 
    found, respectively, to be less than, to match, or be greater than str2.
*/
AREXPORT int ArUtil::strcmp(std::string str, const char *str2)
{
  return ::strcmp(str.c_str(), str2);
}

/** 
    This compares two strings, it returns an integer less than, equal to, 
    or greater than zero  if  str  is  found, respectively, to be less than, to
    match, or be greater than str2.
    @param str the string to compare
    @param str2 the second string to compare
    @return an integer less than, equal to, or greater than zero if str is 
    found, respectively, to be less than, to match, or be greater than str2.
*/
AREXPORT int ArUtil::strcmp(const char *str, std::string str2)
{
  return ::strcmp(str, str2.c_str());
}

/** 
    This compares two strings, it returns an integer less than, equal to, 
    or greater than zero  if  str  is  found, respectively, to be less than, to
    match, or be greater than str2.
    @param str the string to compare
    @param str2 the second string to compare
    @return an integer less than, equal to, or greater than zero if str is 
    found, respectively, to be less than, to match, or be greater than str2.
*/
AREXPORT int ArUtil::strcmp(const char *str, const char *str2)
{
  return ::strcmp(str, str2);
}


/** 
    This compares two strings ignoring case, it returns an integer
    less than, equal to, or greater than zero if str is found,
    respectively, to be less than, to match, or be greater than str2.
    @param str the string to compare @param str2 the second string to
    compare @return an integer less than, equal to, or greater than
    zero if str is found, respectively, to be less than, to match, or
    be greater than str2.  */
AREXPORT int ArUtil::strcasecmp(std::string str, std::string str2)
{
  return ::strcasecmp(str.c_str(), str2.c_str());
}

/** 
    This compares two strings ignoring case, it returns an integer
    less than, equal to, or greater than zero if str is found,
    respectively, to be less than, to match, or be greater than str2.
    @param str the string to compare @param str2 the second string to
    compare @return an integer less than, equal to, or greater than
    zero if str is found, respectively, to be less than, to match, or
    be greater than str2.  */
AREXPORT int ArUtil::strcasecmp(std::string str, const char *str2)
{
  return ::strcasecmp(str.c_str(), str2);
}

/** 
    This compares two strings ignoring case, it returns an integer
    less than, equal to, or greater than zero if str is found,
    respectively, to be less than, to match, or be greater than str2.
    @param str the string to compare @param str2 the second string to
    compare @return an integer less than, equal to, or greater than
    zero if str is found, respectively, to be less than, to match, or
    be greater than str2.  */
AREXPORT int ArUtil::strcasecmp(const char *str, std::string str2)
{
  return ::strcasecmp(str, str2.c_str());
}

/** 
    This compares two strings ignoring case, it returns an integer
    less than, equal to, or greater than zero if str is found,
    respectively, to be less than, to match, or be greater than str2.
    @param str the string to compare @param str2 the second string to
    compare @return an integer less than, equal to, or greater than
    zero if str is found, respectively, to be less than, to match, or
    be greater than str2.  */
AREXPORT int ArUtil::strcasecmp(const char *str, const char *str2)
{
  return ::strcasecmp(str, str2);
}


AREXPORT int ArUtil::strcasequotecmp(const std::string &str1, 
                                     const std::string &str2)
{
  
  int len1 = str1.length();
  size_t pos1 = 0;
  if ((len1 >= 2) && (str1[0] == '\"') && (str1[len1 - 1] == '\"')) {
    pos1 = 1;
  }
  int len2 = str2.length();
  size_t pos2 = 0;
  if ((len2 >= 2) && (str2[0] == '\"') && (str2[len2 - 1] == '\"')) {
    pos2 = 1;
  }

  /* Unfortunately gcc2 does't support the 5 argument version of std::string::compare()... 
   * (Furthermore, note that it's 3-argument compare has the arguments in the wrong order.)
   */
#if defined(__GNUC__) && (__GNUC__ <= 2) && (__GNUC_MINOR__ <= 96)
#warning Using GCC 2.96 or less so must use nonstandard std::string::compare method.
  int cmp = str1.compare(str2.substr(pos2, len2 - 2 * pos2), pos1, len1 - 2 * pos1);
#else
  int cmp = str1.compare(pos1, 
	 	  	 len1 - 2 * pos1,
                         str2,
                         pos2, 
			 len2 - 2 * pos2);
#endif

  return cmp;

} // end method strcasequotecmp

/**
   This copies src into dest but puts a \ before any spaces in src,
   escaping them... its mostly for use with ArArgumentBuilder... 
   make sure you have at least maxLen spaces in the arrays that you're passing 
   as dest... this allocates no memory
**/
AREXPORT void ArUtil::escapeSpaces(char *dest, const char *src, size_t maxLen)
{
  size_t i, adj, len;

  len = strlen(src);
  // walk it, when we find one toss in the slash and incr adj so the
  // next characters go in the right space
  for (i = 0, adj = 0; i < len && i + adj < maxLen; i++)
  {
    if (src[i] == ' ')
    {
      dest[i+adj] = '\\';
      adj++;
    }
    dest[i+adj] = src[i];
  }
  // make sure its null terminated
  dest[i+adj] = '\0';
}

/**
   This copies src into dest but makes it lower case make sure you
   have at least maxLen arrays that you're passing as dest... this
   allocates no memory
**/
AREXPORT void ArUtil::lower(char *dest, const char *src, size_t maxLen)
{
  size_t i;
  size_t len;
  
  len = strlen(src);
  for (i = 0; i < len && i < maxLen; i++)
    dest[i] = tolower(src[i]);
  dest[i] = '\0';

}


AREXPORT bool ArUtil::isOnlyAlphaNumeric(const char *str)
{
  unsigned int ui;
  unsigned int len;
  if (str == NULL)
    return true;
  for (ui = 0, len = strlen(str); ui < len; ui++)
  {
    if (!isalpha(str[ui]) && !isdigit(str[ui]) && str[ui] != '\0')
      return false;
  }
  return true;
}

AREXPORT bool ArUtil::isStrEmpty(const char *str)
{
	if (str == NULL) {
		return true;
	}
	if (str[0] == '\0') {
		return true;
	}
	return false;

} // end method isStrEmpty


AREXPORT const char *ArUtil::convertBool(int val)
{
  if (val)
    return TRUESTRING;
  else
    return FALSESTRING;
}

AREXPORT double ArUtil::atof(const char *nptr)
{
  if (strcasecmp(nptr, "inf") == 0)
    return HUGE_VAL;
  else if (strcasecmp(nptr, "-inf") == 0)
    return -HUGE_VAL;
  else
	return ::atof(nptr);
}


AREXPORT void ArUtil::functorPrintf(ArFunctor1<const char *> *functor,
				    char *str, ...)
{
  char buf[10000];
  va_list ptr;
  va_start(ptr, str);
  //vsprintf(buf, str, ptr);
  vsnprintf(buf, sizeof(buf) - 1, str, ptr);
  buf[sizeof(buf) - 1] = '\0';
  functor->invoke(buf);
  va_end(ptr);
}


AREXPORT void ArUtil::writeToFile(const char *str, FILE *file)
{
  fputs(str, file);
}


/** 
   This function reads a string from a file.
   The file can contain spaces or tabs, but a '\\r'
   or '\\n' will be treated as the end of the string, and the string
   cannot have more characters than the value given by strLen.  This is mostly for internal use
   with Linux to determine the Aria directory from a file in /etc, but
   will work with Linux or Windows. 

   @param fileName name of the file in which to look
   @param str the string to copy the file contents into
   @param strLen the maximum allocated length of str
**/
AREXPORT bool ArUtil::getStringFromFile(const char *fileName, 
					char *str, size_t strLen)
{
  FILE *strFile;
  unsigned int i;
  
  str[0] = '\0';
  
  if ((strFile = ArUtil::fopen(fileName, "r")) != NULL)
  {
    fgets(str, strLen, strFile);
    for (i = 0; i < strLen; i++)
    {
      if (str[i] == '\r' || str[i] == '\n' || str[i] == '\0')
      {
	str[i] = '\0';
	fclose(strFile);
	break;
      }
    }
  }
  else
  {
    str[0] = '\0';
    return false;
  }
  return true;
}

/**
 * Look up the given value under the given key, within the given registry root
 * key.

   @param root the root key to use, one of the REGKEY enum values

   @param key the name of the key to find

   @param value the value name in which to find the string

   @param str where to put the string found, or if it could not be
   found, an empty (length() == 0) string

   @param len the length of the allocated memory in str

   @return true if the string was found, false if it was not found or if there was a problem such as the string not being long enough 
 **/

AREXPORT bool ArUtil::getStringFromRegistry(REGKEY root,
						   const char *key,
						   const char *value,
						   char *str,
						   int len)
{
#ifndef WIN32
  return false;
#else // WIN32

  HKEY hkey;
  int err;
  unsigned long numKeys;
  unsigned long longestKey;
  unsigned long numValues;
  unsigned long longestValue;
  unsigned long longestDataLength;
  char *valueName;
  unsigned long valueLength;
  unsigned long type;
  char *data;
  unsigned long dataLength;
  HKEY rootKey;


  switch (root)
  {
  case REGKEY_CLASSES_ROOT:
    rootKey = HKEY_CLASSES_ROOT;
    break;
  case REGKEY_CURRENT_CONFIG:
    rootKey = HKEY_CURRENT_CONFIG;
    break;
  case REGKEY_CURRENT_USER:
    rootKey = HKEY_CURRENT_USER;
    break;
  case REGKEY_LOCAL_MACHINE:
    rootKey = HKEY_LOCAL_MACHINE;
    break;
  case REGKEY_USERS:
    rootKey=HKEY_USERS;
    break;
  default:
    ArLog::log(ArLog::Terse, 
	       "ArUtil::getStringFromRegistry: Bad root key given.");
    return false;
  }


  if ((err = RegOpenKeyEx(rootKey, key, 0, KEY_READ, &hkey)) == ERROR_SUCCESS)
  {
    //printf("Got a key\n");
    if (RegQueryInfoKey(hkey, NULL, NULL, NULL, &numKeys, &longestKey, NULL, 
			&numValues, &longestValue, &longestDataLength, NULL, NULL) == ERROR_SUCCESS)
    {
	/*
      printf("Have %d keys longest is %d, have %d values longest name is %d, longest data is %d\n",
	     numKeys, longestKey, numValues, longestValue, longestDataLength);
	*/	 
      data = new char[longestDataLength+2];
      valueName = new char[longestValue+2];
      for (unsigned long i = 0; i < numValues; ++i)
      {
	dataLength = longestDataLength+1;
	valueLength = longestValue+1;
	if ((err = RegEnumValue(hkey, i, valueName, &valueLength, NULL, 
				&type, (unsigned char *)data, &dataLength)) == ERROR_SUCCESS)
	{
		//printf("Enumed value %d, name is %s, value is %s\n", i, valueName, data);
	  if (strcmp(value, valueName) == 0)
	  {
	    if (len < dataLength)
	    {
	      ArLog::log(ArLog::Terse,"ArUtil::getStringFromRegistry: str passed in not long enough for data.");
	      delete data;
	      delete valueName;
	      return false;
	    }
	    strncpy(str, data, len);
	    delete data;
	    delete valueName;
	    return true;
	  }
	}
	/*
	else
		printf("Couldn't enum value %d cause %d\n",i,  err);
		*/
	    }
      delete data;
      delete valueName;
    }
	/*
    else
      printf("QueryInfoKey failed\n");
	  */
  }
  /*
  else
    printf("No key %d\n", err);
  */
  return false;
#endif
}
  
#if defined(_POSIX_TIMERS) && defined(_POSIX_MONOTONIC_CLOCK)
bool ArTime::ourMonotonicClock = true;
#endif 

AREXPORT void ArTime::setToNow(void)
{
// if we have the best way of finding time use that
#if defined(_POSIX_TIMERS) && defined(_POSIX_MONOTONIC_CLOCK)
  if (ourMonotonicClock)
  {
    struct timespec timeNow;
    if (clock_gettime(CLOCK_MONOTONIC, &timeNow) == 0)
    {
      mySec = timeNow.tv_sec;
      myMSec = timeNow.tv_nsec / 1000000;
      return;
    }
    else
    {
      ourMonotonicClock = false;
      ArLog::logNoLock(ArLog::Terse, "ArTime::setToNow: invalid return from clock_gettime.");
    }
  }
#endif
// if our good way didn't work use the old ways
#ifndef WIN32
  struct timeval timeNow;
  
  if (gettimeofday(&timeNow, NULL) == 0)
  {
    mySec = timeNow.tv_sec;
    myMSec = timeNow.tv_usec / 1000;
  }
  else
    ArLog::logNoLock(ArLog::Terse, "ArTime::setToNow: invalid return from gettimeofday.");
// thats probably not available in windows, so this is the one we've been using
#else
      /* this should be the better way, but it doesn't really work...
	 this would be seconds from 1970, but it is based on the
	 hardware timer or something and so winds up not being updated
	 all the time and winds up being some number of ms < 20 ms off
      struct _timeb startTime;
      _ftime(&startTime);
      mySec = startTime.time;
      myMSec = startTime.millitm;*/
      // so we're going with just their normal function, msec since boot
  long timeNow;
  timeNow = timeGetTime();
  mySec = timeNow / 1000;
  myMSec = timeNow % 1000;
// but if the good way isn't available use the old way...
#endif
      
}

AREXPORT ArRunningAverage::ArRunningAverage(size_t numToAverage)
{
  myNumToAverage = numToAverage;
  myTotal = 0;
  myNum = 0;
}

AREXPORT ArRunningAverage::~ArRunningAverage()
{

}

AREXPORT double ArRunningAverage::getAverage(void) const
{
  return myTotal / myNum;
}

AREXPORT void ArRunningAverage::add(double val)
{
  myTotal += val;
  myNum++;
  myVals.push_front(val);
  if (myVals.size() > myNumToAverage || myNum > myNumToAverage)
  {
    myTotal -= myVals.back();
    myNum--;
    myVals.pop_back();
  }
}

AREXPORT void ArRunningAverage::clear(void)
{
  while (myVals.size() > 0)
    myVals.pop_back();
  myNum = 0;
  myTotal = 0;
}

AREXPORT size_t ArRunningAverage::getNumToAverage(void) const
{
  return myNumToAverage;
}

AREXPORT void ArRunningAverage::setNumToAverage(size_t numToAverage)
{
  myNumToAverage = numToAverage;
  while (myVals.size() > myNumToAverage)
  {
    myTotal -= myVals.back();
    myNum--;
    myVals.pop_back();
  }
}

AREXPORT size_t ArRunningAverage::getCurrentNumAveraged(void)
{
  return myNum;
}

#ifndef WIN32

AREXPORT ArDaemonizer::ArDaemonizer(int *argc, char **argv) :
  myParser(argc, argv),
  myLogOptionsCB(this, &ArDaemonizer::logOptions)
{
  myIsDaemonized = false;
  Aria::addLogOptionsCB(&myLogOptionsCB);
}

AREXPORT ArDaemonizer::~ArDaemonizer()
{

}

AREXPORT bool ArDaemonizer::daemonize(void)
{
  if (myParser.checkArgument("-daemonize") ||
      myParser.checkArgument("-d"))
  {
    return forceDaemonize();
  }
  else
    return true;

}

/**
   This returns true if daemonizing worked, returns false if it
   didn't... the parent process exits here if forking worked.
 **/
AREXPORT bool ArDaemonizer::forceDaemonize(void)
{
    switch (fork())
    {
    case 0: // child process just return
      myIsDaemonized = true;
      fclose(stdout);
      fclose(stderr);
      return true;
    case -1: // error.... fail
      printf("Can't fork");
      ArLog::log(ArLog::Terse, "ArDaemonizer: Can't fork");
      return false;
    default: // parent process
      printf("Daemon started\n");
      exit(0);
    }
}

AREXPORT void ArDaemonizer::logOptions(void) const
{
  ArLog::log(ArLog::Terse, "Options for Daemonizing:");
  ArLog::log(ArLog::Terse, "-daemonize");
  ArLog::log(ArLog::Terse, "-d");
  ArLog::log(ArLog::Terse, "");
}

#endif // WIN32


std::map<ArPriority::Priority, std::string> ArPriority::ourPriorityNames;
std::string ArPriority::ourUnknownPriorityName;
bool ArPriority::ourStringsInited = false;

AREXPORT const char *ArPriority::getPriorityName(Priority priority) 
{

  if (!ourStringsInited)
  {
    ourPriorityNames[IMPORTANT] = "Basic";
    ourPriorityNames[NORMAL]    = "Intermediate";
    ourPriorityNames[TRIVIAL]   = "Advanced";
    ourPriorityNames[DETAILED]  = "Advanced";

    ourPriorityNames[EXPERT]    = "Expert";
    ourPriorityNames[FACTORY]   = "Factory";

    ourUnknownPriorityName      = "Unknown";
    ourStringsInited = true;
  }
  std::map<ArPriority::Priority, std::string>::iterator iter = 
                                                  ourPriorityNames.find(priority);
  if (iter != ourPriorityNames.end()) {
    return iter->second.c_str();
  }
  else {
    return ourUnknownPriorityName.c_str();
  }
}

AREXPORT void ArUtil::putCurrentYearInString(char* s, size_t len)
{
  struct tm t;
  ArUtil::localtime(&t);
  snprintf(s, len, "%4d", 1900 + t.tm_year);
  s[len-1] = '\0';
}
AREXPORT void ArUtil::putCurrentMonthInString(char* s, size_t len)
{

  struct tm t;
  ArUtil::localtime(&t);
  snprintf(s, len, "%02d", t.tm_mon + 1);
  s[len-1] = '\0';
}
AREXPORT void ArUtil::putCurrentDayInString(char* s, size_t len)
{
  struct tm t;
  ArUtil::localtime(&t);
  snprintf(s, len, "%02d", t.tm_mday);
  s[len-1] = '\0';
}
AREXPORT void ArUtil::putCurrentHourInString(char* s, size_t len)
{
  struct tm t;
  ArUtil::localtime(&t);
  snprintf(s, len, "%02d", t.tm_hour);
  s[len-1] = '\0';
}
AREXPORT void ArUtil::putCurrentMinuteInString(char* s, size_t len)
{
  struct tm t; 
  ArUtil::localtime(&t);
  snprintf(s, len, "%02d", t.tm_min);
  s[len-1] = '\0';
}
AREXPORT void ArUtil::putCurrentSecondInString(char* s, size_t len)
{
  struct tm t;
  ArUtil::localtime(&t);
  snprintf(s, len, "%02d", t.tm_sec);
  s[len-1] = '\0';
}


AREXPORT bool ArUtil::localtime(const time_t *timep, struct tm *result) 
{
#ifdef WIN32
  ourLocaltimeMutex.lock();
  struct tm *r = ::localtime(timep);
  if(r == NULL) { 
    ourLocaltimeMutex.unlock();
    return false;
  }
  *result = *r; // copy the 'struct tm' object before unlocking.
  ourLocaltimeMutex.unlock();
  return true;
#else
  return (::localtime_r(timep, result) != NULL);
#endif
}

/** Call ArUtil::localtime() with the current time obtained by calling
* time(NULL).
*  @return false on error (e.g. invalid input), otherwise true.
*/
AREXPORT bool ArUtil::localtime(struct tm *result) 
{ 
  time_t now = time(NULL);
  return ArUtil::localtime(&now, result); 
}

AREXPORT ArCallbackList::ArCallbackList(
	const char *name, ArLog::LogLevel logLevel, bool singleShot)
{
  myName = name;
  mySingleShot = singleShot;
  setLogLevel(logLevel);
  std::string mutexName;
  mutexName = "ArCallbackList::";
  mutexName += name;
  mutexName += "::myDataMutex";
  myDataMutex.setLogName(mutexName.c_str());
}

AREXPORT  ArCallbackList::~ArCallbackList()
{

}

AREXPORT void ArCallbackList::addCallback(
	ArFunctor *functor, int position)
{
  myDataMutex.lock();
  myList.insert(
	  std::pair<int, ArFunctor *>(-position, 
				      functor));
  myDataMutex.unlock();
}

AREXPORT void ArCallbackList::remCallback(ArFunctor *functor)
{
  myDataMutex.lock();
  std::multimap<int, ArFunctor *>::iterator it;

  for (it = myList.begin(); it != myList.end(); it++)
  {
    if ((*it).second == functor)
    {
      myList.erase(it);
      myDataMutex.unlock();
      remCallback(functor);
      return;
    }
  }
  myDataMutex.unlock();
}


AREXPORT void ArCallbackList::setName(const char *name)
{
  myDataMutex.lock();
  myName = name;
  myDataMutex.unlock();
}

AREXPORT void ArCallbackList::setNameVar(const char *name, ...)
{
  char arg[2048];
  va_list ptr;
  va_start(ptr, name);
  vsnprintf(arg, sizeof(arg), name, ptr);
  arg[sizeof(arg) - 1] = '\0';
  va_end(ptr);
  return setName(arg);
}

AREXPORT void ArCallbackList::setSingleShot(bool singleShot)
{
  myDataMutex.lock();
  mySingleShot = singleShot;
  myDataMutex.unlock();
}

AREXPORT void ArCallbackList::setLogLevel(ArLog::LogLevel logLevel)
{
  myDataMutex.lock();
  myLogLevel = logLevel;
  myDataMutex.unlock();
}

AREXPORT void ArCallbackList::invoke(void)
{
  myDataMutex.lock();

  std::multimap<int, ArFunctor *>::iterator it;
  ArFunctor *functor;

  ArLog::log(myLogLevel, "%s: Starting calls", myName.c_str());

  for (it = myList.begin(); 
       it != myList.end(); 
       it++)
  {
    functor = (*it).second;
    if (functor == NULL) 
      continue;

    if (functor->getName() != NULL && functor->getName()[0] != '\0')
      ArLog::log(myLogLevel, "%s: Calling functor '%s' at %d", 
		 myName.c_str(), functor->getName(), -(*it).first);
    else
      ArLog::log(myLogLevel, "%s: Calling unnamed functor at %d", 
		 myName.c_str(), -(*it).first);
    functor->invoke();
  }

  ArLog::log(myLogLevel, "%s: Ended calls", myName.c_str());

  if (mySingleShot)
  {
    ArLog::log(myLogLevel, "%s: Clearing callbacks", myName.c_str());
    myList.clear();
  }
  myDataMutex.unlock();
}

#ifndef WIN32
/**
   @param baseDir the base directory to work from
   @param fileName the fileName to squash the case from
   
   @param result where to put the result
   @param resultLen length of the result

   @return true if it could find the file, the result is in result,
   false if it couldn't find the file
**/
AREXPORT bool ArUtil::matchCase(const char *baseDir, 
					   const char *fileName,
					   char *result,
					   size_t resultLen)
{

  /***
  ArLog::log(ArLog::Normal, 
             "ArUtil::matchCase() baseDir = \"%s\" fileName = \"%s\"",
             baseDir,
             fileName);
  ***/

  DIR *dir;
  struct dirent *ent;

  char separator;  
#ifndef WIN32
  separator = '/';
#else
  separator = '\\';
#endif

  result[0] = '\0';

  std::list<std::string> split = splitFileName(fileName);
  std::list<std::string>::iterator it = split.begin();
  std::string finding = (*it);

  /*
  for (it = split.begin(); it != split.end(); it++)
  {
    printf("@@@@@@@@ %s\n", (*it).c_str());
  }
  */
  
  // how this works is we start at the base dir then read through
  // until we find what the next name we need, if entry is a directory
  // and we're not at the end of our string list then we change into
  // that dir and the while loop keeps going, if the entry isn't a
  // directory and matchs and its the last in our string list we've
  // found what we want
  if ((dir = opendir(baseDir)) == NULL)
  {
    ArLog::log(ArLog::Normal, 
	       "ArUtil: No such directory '%s' for base", 
	       baseDir);
    return false;
  }

  if (finding == ".")
  {
    it++;
    if (it != split.end())
    {
      finding = (*it);
    }
    else
    {
      ArLog::log(ArLog::Normal, 
		             "ArUtil: No file or directory given (base = %s file = %s)", 
		             baseDir,
                 fileName);
      closedir(dir);

      // KMC NEED TO DETERMINE WHICH IS CORRECT.
      // The following change appears to be necessary for maps, but is still
      // undergoing testing....
      //   Just return the given ".". (This is necessary to find maps in the local 
      //   directory under some circumstances.)
      //   snprintf(result, resultLen, finding.c_str());
      //   return true;  

      return false;

      
    }
  }

  while ((ent = readdir(dir)) != NULL)
  {
    // ignore some of these
    if (ent->d_name[0] == '.')
    {
      //printf("Ignoring %s\n", ent->d_name[0]);
      continue;
    }
    //printf("NAME %s finding %s\n", ent->d_name, finding.c_str());
    
    // we've found what we were looking for
    if (ArUtil::strcasecmp(ent->d_name, finding) == 0)
    {
      size_t lenOfResult;
      lenOfResult = strlen(result);

      // make sure we can put the filename in
      if (strlen(ent->d_name) > resultLen - lenOfResult - 2)
      {
	ArLog::log(ArLog::Normal, 
		   "ArUtil::matchCase: result not long enough");
	closedir(dir);
	return false;
      }
      //printf("Before %s", result);
      if (lenOfResult != 0)
      {
	result[lenOfResult] = separator;
	result[lenOfResult+1] = '\0';
      }
      // put the filename in
      strcpy(&result[strlen(result)], ent->d_name);
      //printf("after %s\n", result);
      // see if we're at the end
      it++;
      if (it != split.end())
      {
	//printf("Um.........\n");
	finding = (*it);
	std::string wholeDir;
	wholeDir = baseDir;
	wholeDir += result;
	closedir(dir);
	//printf("'%s' '%s' '%s'\n", baseDir, result, wholeDir.c_str());
	if ((dir = opendir(wholeDir.c_str())) == NULL)
	{
	  ArLog::log(ArLog::Normal, 
		     "ArUtil::matchCase: Error going into %s", 
		     result);
	  return false;
	}
      }
      else
      {
	//printf("\n########## Got it %s\n", result);
	closedir(dir);
	return true;
      }
    }
  }
  ArLog::log(ArLog::Normal, 
	     "ArUtil::matchCase: %s doesn't exist in %s", fileName, 
	     baseDir);
  //printf("!!!!!!!! %s", finding.c_str());
  closedir(dir);
  return false;
} 

#endif // !WIN32


AREXPORT bool ArUtil::getDirectory(const char *fileName, 
					     char *result, size_t resultLen)
{
  char separator;  
#ifndef WIN32
  separator = '/';
#else
  separator = '\\';
#endif
  
  if (fileName == NULL || fileName[0] == '\0' || resultLen == 0)
  {
    ArLog::log(ArLog::Normal, "ArUtil: getDirectory, bad setup");
    return false;
  }
  
  // just play in the result buffer
  strncpy(result, fileName, resultLen - 1);
  // make sure its nulled
  result[resultLen - 1] = '\0';
  char *toPos;
  ArUtil::fixSlashes(result, resultLen);
  // see where the last directory is
  toPos = strrchr(result, separator);
  // if there's no divider it must just be a file name
  if (toPos == NULL)
  {
    result[0] = '\0';
    return true;
  }
  // otherwise just toss a null into the last separator and we're done
  else
  {    
    *toPos = '\0';
    return true;
  }
}

AREXPORT bool ArUtil::getFileName(const char *fileName, 
					 char *result, size_t resultLen)
{
  char separator;  
#ifndef WIN32
  separator = '/';
#else
  separator = '\\';
#endif
  
  if (fileName == NULL || fileName[0] == '\0' || resultLen == 0)
  {
    ArLog::log(ArLog::Normal, "ArUtil: getFileName, bad setup");
    return false;
  }

  char *str;
  size_t fileNameLen = strlen(fileName);
  str = new char[fileNameLen + 1];
  //printf("0 %s\n", fileName);
  // just play in the result buffer
  strncpy(str, fileName, fileNameLen);
  // make sure its nulled
  str[fileNameLen] = '\0';
  //printf("1 %s\n", str);

  char *toPos;
  ArUtil::fixSlashes(str, fileNameLen + 1);
  //printf("2 %s\n", str);
  // see where the last directory is
  toPos = strrchr(str, separator);
  // if there's no divider it must just be a file name
  if (toPos == NULL)
  {
    // copy the filename in and make sure it has a null
    strncpy(result, str, resultLen - 1);
    result[resultLen - 1] = '\0';
    //printf("3 %s\n", result);
    delete[] str;
    return true;
  }
  // otherwise take the section from that separator to the end
  else
  {
    strncpy(result, &str[toPos - str + 1], resultLen - 2);
    result[resultLen - 1] = '\0';
    //printf("4 %s\n", result);
    delete[] str;
    return true;
  }
}

#ifndef WIN32
/**
   This function assumes the slashes are all heading the right way already.
**/
std::list<std::string> ArUtil::splitFileName(const char *fileName)
{
  std::list<std::string> split;
  if (fileName == NULL)
    return split;

  char separator;  
#ifndef WIN32
  separator = '/';
#else
  separator = '\\';
#endif

  size_t len;
  size_t i;
  size_t last;
  bool justSepped;
  char entry[2048];
  for (i = 0, justSepped = false, last = 0, len = strlen(fileName); 
       ; 
       i++)
  {
    
    if ((fileName[i] == separator && !justSepped) 
	|| fileName[i] == '\0' || i >= len)
    {
      if (i - last > 2047)
      {
	ArLog::log(ArLog::Normal, "ArUtil::splitFileName: some directory or file too long");
      }
      if (!justSepped)
      {
	strncpy(entry, &fileName[last], i - last);
	entry[i-last] = '\0';
	split.push_back(entry);

	justSepped = true;
      }
      if (fileName[i] == '\0' || i >= len)
	return split;
    }
    else if (fileName[i] == separator && justSepped)
    {
      justSepped = true;
      last = i;
    }
    else if (fileName[i] != separator && justSepped)
    {
      justSepped = false;
      last = i;
    }
  }
  ArLog::log(ArLog::Normal, "ArUtil::splitFileName: file str ('%s') happened weird", fileName);
  return split;
}



#endif // !WIN32


AREXPORT bool ArUtil::changeFileTimestamp(const char *fileName, 
                                          time_t timestamp) 
{
  if (ArUtil::isStrEmpty(fileName)) {
    ArLog::log(ArLog::Normal,
               "Cannot change date on file with empty name");
    return false;
  }
#ifdef WIN32

  FILETIME fileTime;

  HANDLE hFile = CreateFile(fileName,
                            GENERIC_READ | GENERIC_WRITE,
                             0,NULL,
                             OPEN_EXISTING,
                             0,NULL);

  if (hFile == NULL) {
    return false;
  }


  // The following is extracted from the MSDN article "Converting a time_t Value
  // to a File Time".
  LONGLONG temp = Int32x32To64(timestamp, 10000000) + 116444736000000000;
  fileTime.dwLowDateTime = (DWORD) temp;
  fileTime.dwHighDateTime = temp >> 32;

  SetFileTime(hFile, 
              &fileTime, 
              (LPFILETIME) NULL,  // don't change last access time (?)
              &fileTime);

  CloseHandle(hFile);

#else // unix
        
  char timeBuf[500];
  strftime(timeBuf, sizeof(timeBuf), "%c", ::localtime(&timestamp));
  ArLog::log(ArLog::Normal,
             "Changing file %s modified time to %s",
             fileName,
             timeBuf);


  // time_t newTime = mktime(&timestamp);
  struct utimbuf fileTime;
  fileTime.actime  = timestamp;
  fileTime.modtime = timestamp;
  utime(fileName, &fileTime);

#endif // else unix

  return true;

} // end method changeFileTimestamp



AREXPORT void ArUtil::setFileCloseOnExec(int fd, bool closeOnExec)
{
#ifndef WIN32
  if (fd <= 0)
    return;

  int flags;

  if ((flags = fcntl(fd, F_GETFD)) < 0)
  {
    ArLog::log(ArLog::Normal, "ArUtil::setFileCloseOnExec: Cannot use F_GETFD in fnctl on fd %d", fd);
    return;
  }

  if (closeOnExec)
    flags |= FD_CLOEXEC;
  else
    flags &= ~FD_CLOEXEC;

  if (fcntl(fd, F_SETFD, flags) < 0)
  {
    ArLog::log(ArLog::Normal, "ArUtil::setFileCloseOnExec: Cannot use F_GETFD in fnctl on fd %d", fd);
    return;
  }
#endif
}

AREXPORT void ArUtil::setFileCloseOnExec(FILE *file, bool closeOnExec)
{
  if (file != NULL)
    setFileCloseOnExec(fileno(file));
}

AREXPORT FILE *ArUtil::fopen(const char *path, const char *mode, 
			     bool closeOnExec)
{
  FILE *file;
  file = ::fopen(path, mode);
  setFileCloseOnExec(file, closeOnExec);
  return file;
}

AREXPORT int ArUtil::open(const char *pathname, int flags, 
			  bool closeOnExec)
{
  int fd;
  fd = ::open(pathname, flags);
  setFileCloseOnExec(fd, closeOnExec);
  return fd;
}

AREXPORT int ArUtil::open(const char *pathname, int flags, mode_t mode, 
			  bool closeOnExec)
{
  int fd;
  fd = ::open(pathname, flags, mode);
  setFileCloseOnExec(fd, closeOnExec);
  return fd;
}

AREXPORT int ArUtil::creat(const char *pathname, mode_t mode, 
			   bool closeOnExec)
{
  int fd;
  fd = ::creat(pathname, mode);
  setFileCloseOnExec(fd, closeOnExec);
  return fd;
}

AREXPORT FILE *ArUtil::popen(const char *command, const char *type, 
			     bool closeOnExec)
{
  FILE *file;
#ifndef WIN32
  file = ::popen(command, type);
#else
  file = _popen(command, type);
#endif
  setFileCloseOnExec(file, closeOnExec);
  return file;
}

AREXPORT bool ArUtil::floatIsNormal(double f)
{
#ifdef WIN32
	  return (!::_isnan(f) && ::_finite(f));
#else
	  return isnormal(f);
#endif
}


AREXPORT long ArMath::randomInRange(long m, long n)
{
    // simple method
    return m + random() / (ourRandMax / (n - m + 1) + 1);
    // alternate method is to use drand48, multiply and round (does Windows have
    // drand48?), or keep trying numbers until we get one in range.
}

AREXPORT double ArMath::epsilon() { return ourEpsilon; }
AREXPORT long ArMath::getRandMax() { return ourRandMax; }

ArGlobalRetFunctor2<ArLaser *, int, const char *> 
ArLaserCreatorHelper::ourLMS2xxCB(&ArLaserCreatorHelper::createLMS2xx);
ArGlobalRetFunctor2<ArLaser *, int, const char *> 
ArLaserCreatorHelper::ourUrgCB(&ArLaserCreatorHelper::createUrg);

ArLaser *ArLaserCreatorHelper::createLMS2xx(int laserNumber, 
					    const char *logPrefix)
{
  return new ArLMS2xx(laserNumber);
}

ArRetFunctor2<ArLaser *, int, const char *> *ArLaserCreatorHelper::getCreateLMS2xxCB(void)
{
  return &ourLMS2xxCB;
}

ArLaser *ArLaserCreatorHelper::createUrg(int laserNumber, const char *logPrefix)
{
  return new ArUrg(laserNumber);
}

ArRetFunctor2<ArLaser *, int, const char *> *ArLaserCreatorHelper::getCreateUrgCB(void)
{
  return &ourUrgCB;
}

ArGlobalRetFunctor2<ArLaser *, int, const char *> 
ArLaserCreatorHelperLMS1XX::ourLMS1XXCB(&ArLaserCreatorHelperLMS1XX::createLMS1XX);

ArLaser *ArLaserCreatorHelperLMS1XX::createLMS1XX(int laserNumber, 
						  const char *logPrefix)
{
  return new ArLMS1XX(laserNumber);
}

ArRetFunctor2<ArLaser *, int, const char *> *ArLaserCreatorHelperLMS1XX::getCreateLMS1XXCB(void)
{
  return &ourLMS1XXCB;
}


ArGlobalRetFunctor3<ArDeviceConnection *, const char *, const char *, const char *> 
ArDeviceConnectionCreatorHelper::ourSerialCB(
	&ArDeviceConnectionCreatorHelper::createSerialConnection);
ArGlobalRetFunctor3<ArDeviceConnection *, const char *, const char *, const char *> 
ArDeviceConnectionCreatorHelper::ourTcpCB(
	&ArDeviceConnectionCreatorHelper::createTcpConnection);
ArLog::LogLevel ArDeviceConnectionCreatorHelper::ourSuccessLogLevel;

ArDeviceConnection *ArDeviceConnectionCreatorHelper::createSerialConnection(
	const char *port, const char *defaultInfo, const char *logPrefix)
{
  ArSerialConnection *serConn = new ArSerialConnection;
  
  std::string serPort;
  if (strcasecmp(port, "COM1") == 0)
    serPort = ArUtil::COM1;
  else if (strcasecmp(port, "COM2") == 0)
    serPort = ArUtil::COM2;
  else if (strcasecmp(port, "COM3") == 0)
    serPort = ArUtil::COM3;
  else if (strcasecmp(port, "COM4") == 0)
    serPort = ArUtil::COM4;
  else if (strcasecmp(port, "COM5") == 0)
    serPort = ArUtil::COM5;
  else if (strcasecmp(port, "COM6") == 0)
    serPort = ArUtil::COM6;
  else if (strcasecmp(port, "COM7") == 0)
    serPort = ArUtil::COM7;
  else if (strcasecmp(port, "COM8") == 0)
    serPort = ArUtil::COM8;
  else if (strcasecmp(port, "COM9") == 0)
    serPort = ArUtil::COM9;
  else if (strcasecmp(port, "COM10") == 0)
    serPort = ArUtil::COM10;
  else if (strcasecmp(port, "COM11") == 0)
    serPort = ArUtil::COM11;
  else if (strcasecmp(port, "COM12") == 0)
    serPort = ArUtil::COM12;
  else if (strcasecmp(port, "COM13") == 0)
    serPort = ArUtil::COM13;
  else if (strcasecmp(port, "COM14") == 0)
    serPort = ArUtil::COM14;
  else if (strcasecmp(port, "COM15") == 0)
    serPort = ArUtil::COM15;
  else if (strcasecmp(port, "COM16") == 0)
    serPort = ArUtil::COM16;
  else if (port != NULL)
    serPort = port;
  
  ArLog::log(ourSuccessLogLevel, "%Set serial port to open %s", 
	     logPrefix, serPort.c_str());
  serConn->setPort(serPort.c_str());
  return serConn;
  /*  
      This code is commented out because it created problems with demo
      (or any other program that used ArLaserConnector::connectLasers
      with addAllLasersToRobot as true)

  int ret;
  
  if ((ret = serConn->open(serPort.c_str())) == 0)
  {
    ArLog::log(ourSuccessLogLevel, "%sOpened serial port %s", 
	       logPrefix, serPort.c_str());
    return serConn;
  }
  else
  {
    ArLog::log(ArLog::Normal, "%sCould not open serial port %s (from %s), because %s", 
	       logPrefix, serPort.c_str(), port,
	       serConn->getOpenMessage(ret));
    delete serConn;
    return NULL;
  }
  */
}

ArRetFunctor3<ArDeviceConnection *, const char *, const char *, const char *> *
ArDeviceConnectionCreatorHelper::getCreateSerialCB(void)
{
  return &ourSerialCB;
}

ArDeviceConnection *ArDeviceConnectionCreatorHelper::createTcpConnection(
	const char *port, const char *defaultInfo, const char *logPrefix)
{
  ArTcpConnection *tcpConn = new ArTcpConnection;
  int ret;

  tcpConn->setPort(port, atoi(defaultInfo));
  ArLog::log(ourSuccessLogLevel, 
	     "%sSet tcp connection to open %s (and port %d)", 
	     logPrefix, port, atoi(defaultInfo));
  return tcpConn;

  /*
      This code is commented out because it created problems with demo
      (or any other program that used ArLaserConnector::connectLasers
      with addAllLasersToRobot as true)
  
  if ((ret = tcpConn->open(port, atoi(defaultInfo))) == 0)
  {
    ArLog::log(ourSuccessLogLevel, 
	       "%sOpened tcp connection from %s (and port %d)", 
	       logPrefix, port, atoi(defaultInfo));
    return tcpConn;
  }
  else
  {
    ArLog::log(ArLog::Normal, "%sCould not open a tcp connection to host '%s' with default port %d (from '%s'), because %s", 
	       logPrefix, port, atoi(defaultInfo), defaultInfo,
	       tcpConn->getOpenMessage(ret));
    delete tcpConn;
    return NULL;
  }
  */
}

ArRetFunctor3<ArDeviceConnection *, const char *, const char *, const char *> *
ArDeviceConnectionCreatorHelper::getCreateTcpCB(void)
{
  return &ourTcpCB;
}

void ArDeviceConnectionCreatorHelper::setSuccessLogLevel(
	ArLog::LogLevel successLogLevel)
{
  ourSuccessLogLevel = successLogLevel;
}

ArLog::LogLevel ArDeviceConnectionCreatorHelper::setSuccessLogLevel(void)
{
  return ourSuccessLogLevel;
}