// genlib - a component of the Depthmap - spatial network analysis platform
// Copyright (C) 2000-2010 University College London, Alasdair Turner

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef __COMM_H__
#define __COMM_H__

//#include <io.h>
#include <sys/types.h>
#include <sys/timeb.h>

// distinguish between UTF-16 used by Windows and UTF-8 used by Linus / MacOS:
// converted to Unicode path for Win32 (AT 31.01.11)
// typedef solution AT 29.04.11
#ifdef _MSC_VER // MSVC compiler
    typedef wstring comm_string;
    typedef wchar_t comm_char;
    typedef __time64_t comm_time_t;
    const comm_char *const g_default_file_set = L"File set";
#else
    typedef string comm_string;
    typedef char comm_char;
    typedef time_t comm_time_t;
    const comm_char *const g_default_file_set = "File set";
#endif

struct FilePath {
   comm_string m_path;
   comm_string m_name;
   comm_string m_ext;
   FilePath(const comm_string& pathname)
   {
      size_t dot   = pathname.find_last_of('.');
#ifdef _WIN32
      size_t slash = pathname.find_last_of('\\'); // WIN32
#else
      size_t slash = pathname.find_last_of('/'); // Other
#endif
      if (slash != paftl::npos) {
         m_path = pathname.substr(0,slash+1);
      }
      if (dot != paftl::npos) {
         m_name = pathname.substr(slash+1,dot-slash-1);
         m_ext = pathname.substr(dot+1);
      }
      else {
         m_name = pathname.substr(slash+1);
      }
   }
};

class Communicator
{
public:
   class CancelledException  // throw from your class
   {
   public:
      CancelledException() {;}
   };
   enum { NUM_STEPS, CURRENT_STEP, NUM_RECORDS, CURRENT_RECORD };
protected:
   bool m_cancelled;
   bool m_delete_flag;
   // nb. converted to Win32 UTF-16 Unicode path (AT 31.01.11) Linux, MacOS use UTF-8 (AT 29.04.11)
   comm_string m_infilename;
   ifstream *m_infile;
   ifstream *m_infile2; // <- MapInfo MIF files come in two parts
   ofstream *m_outfile;
   // nb. converted to Win32 UTF-16 Unicode path (AT 31.01.11) Linux, MacOS use UTF-8 (AT 29.04.11)
   pqvector<comm_string> m_fileset;   // <- sometimes you want to load a whole set of files
public:
   Communicator()
   { m_infile = NULL; m_infile2 = NULL; m_outfile = NULL; m_cancelled = false; m_delete_flag = false; }
   //
   bool GetDeleteFlag() // used by ICommunicator and IComm together
   { return m_delete_flag; }
   //
   virtual ~Communicator()
   { if (m_infile) delete m_infile; m_infile = NULL;
     if (m_infile2) delete m_infile2; m_infile2 = NULL;
     if (m_outfile) delete m_outfile; m_outfile = NULL; }
   //
   void SetInfile( const comm_char* filename )
   {
      m_infile = new ifstream( filename );
      FilePath fp(filename);
      m_infilename = fp.m_name;
   }
   void SetInfile2( const comm_char* filename )
   {
      m_infile2 = new ifstream( filename );
   }
   comm_string GetInfileName()
   {
      return m_fileset.size() ? comm_string(g_default_file_set) : m_infilename;
   }
   pstring GetMBInfileName()
   {
      pstring ret;
      if (m_fileset.size()) {
         ret = "File set";
      }
      else {
         ret = pstring(m_infilename.c_str());
      }
      return ret;
   }
   size_t GetInfileSize()
   {
      if (m_infile) {
         m_infile->seekg(0, ios::beg);
         size_t begin_pos = m_infile->tellg();
         m_infile->seekg(0, ios::end);
         size_t end_pos = m_infile->tellg();
         m_infile->seekg(0, ios::beg);
         return size_t(end_pos - begin_pos);
      }
      return 0;
   }
   void SetOutfile( const char *filename )
   { m_outfile = new ofstream( filename ); }
   //
   bool IsCancelled() const
   { return m_cancelled; }
   void Cancel()
   { m_cancelled = true; }
   //
   operator ofstream& ()
   { return *m_outfile; }
   operator ifstream& ()
   { return *m_infile; }
   ifstream& GetInfile2()
   { return *m_infile2; }
   //
   const pqvector<comm_string>& GetFileSet() const
   { return m_fileset; }
   //
   virtual void CommPostMessage(int m, int x, int y = 0) const = 0; // Override for specific operating system
};

// this is a simple version of the Communicator which can be used for
// an interface

class ICommunicator : public Communicator
{
   friend class IComm; // IComm is found in idepthmap.h
   //
protected:
   mutable int num_steps;
   mutable int num_records;
   mutable int step;
   mutable int record;
   //
public:
	ICommunicator() { m_delete_flag = true; } // note: an ICommunicator lets IComm know that it should delete it
	virtual ~ICommunicator() {;}
   virtual void CommPostMessage(int m, int x, int y = 0) const;
};

inline void ICommunicator::CommPostMessage(int m, int x, int y) const
{
	switch (m) {
		case Communicator::NUM_STEPS:
			num_steps = x;
			break;
		case Communicator::CURRENT_STEP:
			step = x;
			break;
		case Communicator::NUM_RECORDS:
			num_records = x;
			break;
		case Communicator::CURRENT_RECORD:
			record = x;
			break;
   }
}


// a helpful little function...
#ifdef _MSC_VER // MSVC compiler
inline bool qtimer( __time64_t& t1, __time64_t timeout )
{
   /* static */ _timeb time2; // static removed for multithreaded usage
   _ftime( &time2 );
   __time64_t t2 = (time2.time % 100) * 1000 + time2.millitm;
   if ((t2 - t1) > timeout || (t2 - t1) < 0) { // also catch a loop
      t1 = t2;
      return true;
   }
   return false;
}
#else
inline bool qtimer( time_t& t1, time_t timeout )
{
   /* static */ timeb time2; // static removed for multithreaded usage
   ftime( &time2 );
   time_t t2 = (time2.time % 100) * 1000 + time2.millitm;
   if ((t2 - t1) > timeout || (t2 - t1) < 0) { // also catch a loop
      t1 = t2;
      return true;
   }
   return false;
}
#endif

#endif
