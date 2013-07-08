// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2013, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#include "MergeDataFiles.hpp"
#include "ByteUtilities.hpp"
#include "Constants.hpp"
#include "FileUtils.hpp"
#include "DebugUtils.hpp"

#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>

using namespace std;
typedef int64_t Long;
namespace TraceviewerServer
{
	MergeDataAttribute MergeDataFiles::merge(string directory, string globInputFile,
			string outputFile)
	{
		 int lastDot = globInputFile.find_last_of('.');
		 string suffix = globInputFile.substr(lastDot);

		DEBUGCOUT(2) << "Checking to see if " << outputFile << " exists" << endl;


		if (FileUtils::exists(outputFile))
		{

			DEBUGCOUT(2) << "Exists" << endl;

			if (isMergedFileCorrect(&outputFile))
				return SUCCESS_ALREADY_CREATED;
			// the file exists but corrupted. In this case, we have to remove and create a new one
			cout << "Database file may be corrupted. Continuing" << endl;
			return STATUS_UNKNOWN;
			//remove(OutputFile.string().c_str());
		}

		DEBUGCOUT(2) << "Doesn't exist" << endl;
		// check if the files in glob patterns is correct

		if (!atLeastOneValidFile(directory))
		{
			return FAIL_NO_DATA;
		}

		DataOutputFileStream dos(outputFile.c_str());

		//-----------------------------------------------------
		// 1. write the header:
		//  int type (0: unknown, 1: mpi, 2: openmp, 3: hybrid, ...
		//	int num_files
		//-----------------------------------------------------

		int type = 0;
		dos.writeInt(type);

		vector<string> allPaths = FileUtils::getAllFilesInDir(directory);
		vector<string> filtered;
		vector<string>::iterator it;
		for (it = allPaths.begin(); it != allPaths.end(); it++)
		{
			string val = *it;
			if (val.find(".hpctrace") < string::npos)//This is hardcoded, which isn't great but will have to do because GlobInputFile is regex-style ("*.hpctrace")
				filtered.push_back(val);
		}
		// on linux, we have to sort the files
		//To sort them, we need a random access iterator, which means we need to load all of them into a vector
		sort(filtered.begin(), filtered.end());

		dos.writeInt(filtered.size());
		const Long num_metric_header = 2 * SIZEOF_INT; // type of app (4 bytes) + num procs (4 bytes)
		 Long num_metric_index = filtered.size()
				* (SIZEOF_LONG + 2 * SIZEOF_INT);
		Long offset = num_metric_header + num_metric_index;

		int name_format = 0; // FIXME hack:some hpcprof revisions have different format name !!
		//-----------------------------------------------------
		// 2. Record the process ID, thread ID and the offset
		//   It will also detect if the application is mp, mt, or hybrid
		//	 no accelator is supported
		//  for all files:
		//		int proc-id, int thread-id, long offset
		//-----------------------------------------------------
		vector<string>::iterator it2;
		for (it2 = filtered.begin(); it2 < filtered.end(); it2++)
		{

			 string Filename = *it2;
			 int last_pos_basic_name = Filename.length() - suffix.length();
			 string Basic_name = Filename.substr(FileUtils::combinePaths(directory, "").length(),//This ensures we count the "/" at the end of the path
					last_pos_basic_name);

			vector<string> tokens = splitString(Basic_name, '-');


			 int num_tokens = tokens.size();
			if (num_tokens < PROC_POS)
				// if it is wrong file with the right extension, we skip
				continue;
			int proc;
			string Token_To_Parse = tokens[name_format + num_tokens - PROC_POS];
			proc = atoi(Token_To_Parse.c_str());
			if ((proc == 0) && (!FileUtils::stringActuallyZero(Token_To_Parse)))
			{
				// old version of name format
				name_format = 1;
				string Token_To_Parse = tokens[name_format + num_tokens - PROC_POS];
				proc = atoi(Token_To_Parse.c_str());
			}
			dos.writeInt(proc);
			if (proc != 0)
				type |= MULTI_PROCESSES;
			 int Thread = atoi(tokens[name_format + num_tokens - THREAD_POS].c_str());
			dos.writeInt(Thread);
			if (Thread != 0)
				type |= MULTI_THREADING;
			dos.writeLong(offset);
			offset += FileUtils::getFileSize(Filename);
		}
		//-----------------------------------------------------
		// 3. Copy all data from the multiple files into one file
		//-----------------------------------------------------
		for (it2 = filtered.begin(); it2 < filtered.end(); it2++)
		{
			string i = *it2;

			ifstream dis(i.c_str(), ios_base::binary | ios_base::in);
			char data[PAGE_SIZE_GUESS];
			dis.read(data, PAGE_SIZE_GUESS);
			int NumRead = dis.gcount();
			while (NumRead > 0)
			{
				dos.write(data, NumRead);
				dis.read(data, PAGE_SIZE_GUESS);
				NumRead = dis.gcount();
			}
			dis.close();
		}
		insertMarker(&dos);
		dos.close();
		//-----------------------------------------------------
		// 4. FIXME: write the type of the application
		//  	the type of the application is computed in step 2
		//		Ideally, this step has to be in the beginning !
		//-----------------------------------------------------
		//While we don't actually want to do any input operations, adding the input flag prevents the file from being truncated to 0 bytes
		DataOutputFileStream f(outputFile.c_str(), ios_base::in | ios_base::out | ios_base::binary);
		f.writeInt(type);
		f.close();

		//-----------------------------------------------------
		// 5. remove old files
		//-----------------------------------------------------
		removeFiles(filtered);
		return SUCCESS_MERGED;
	}



	void MergeDataFiles::insertMarker(DataOutputFileStream* dos)
	{
		dos->writeLong(MARKER_END_MERGED_FILE);
	}
	bool MergeDataFiles::isMergedFileCorrect(string* filename)
	{
		ifstream f(filename->c_str(), ios_base::binary | ios_base::in);
		bool isCorrect = false;
		Long pos = FileUtils::getFileSize(*filename) - SIZEOF_LONG;
		int diff;
		if (pos > 0)
		{
			f.seekg(pos, ios_base::beg);
			char buffer[8];
			f.read(buffer, 8);
			uint64_t marker = ByteUtilities::readLong(buffer);

			diff = marker - MARKER_END_MERGED_FILE;
			isCorrect = (diff) == 0;
		}
		f.close();
		return isCorrect;
	}
	bool MergeDataFiles::removeFiles(vector<string> vect)
	{
		bool success = true;
		vector<string>::iterator it;
		for (it = vect.begin(); it != vect.end(); ++it)
		{
			bool thisSuccess = (remove(it->c_str()) == 0);
			success &= thisSuccess;
		}
		return success;
	}
	bool MergeDataFiles::atLeastOneValidFile(string dir)
	{
		vector<string> FileList = FileUtils::getAllFilesInDir(dir);
		vector<string>::iterator it;
		for (it = FileList.begin(); it != FileList.end(); ++it)
		{
			string filename = *it;

			unsigned int l = filename.length();
			//if it ends with ".hpctrace", we are good.
			string ending = ".hpctrace";
			if (l < ending.length())
				continue;
			string supposedext = filename.substr(l - ending.length());

			if (ending == supposedext)
			{
				return true;
			}
		}
		return false;
	}
	//From http://stackoverflow.com/questions/236129/splitting-a-string-in-c
	vector<string> MergeDataFiles::splitString(string toSplit, char delimiter)
	{
		vector<string> toReturn;
		stringstream ss(toSplit);
		string item;
		while (getline(ss, item, delimiter)) {
		       toReturn.push_back(item);
		}
		return toReturn;
	}

} /* namespace TraceviewerServer */