/********************************************************************
Vireio Perception : Open-Source Stereoscopic 3D Driver
Copyright (C) 2012 Andres Hernandez

Aquilinus : Vireio Perception 3D Modification Studio
Copyright � 2014 Denis Reischl

Vireio Perception Version History:
v1.0.0 2012 by Andres Hernandez
v1.0.X 2013 by John Hicks, Neil Schneider
v1.1.x 2013 by Primary Coding Author: Chris Drain
Team Support: John Hicks, Phil Larkson, Neil Schneider
v2.0.x 2013 by Denis Reischl, Neil Schneider, Joshua Brown
v2.0.4 to v3.0.x 2014-2015 by Grant Bagwell, Simon Brown and Neil Schneider
v4.0.x 2015 by Denis Reischl, Grant Bagwell, Simon Brown and Neil Schneider

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
********************************************************************/
#define DEBUG_UINT(a) { wchar_t buf[128]; wsprintf(buf, L"%u", a); OutputDebugString(buf); }
#define DEBUG_HEX(a) { wchar_t buf[128]; wsprintf(buf, L"%x", a); OutputDebugString(buf); }

#define WORKSPACE_HEADER_SIZE 30
#define PROFILE_HEADER_SIZE 30
#define NODE_HASH_SIZE (sizeof(size_t))
const char* szWHeader_v1_0_0 = "AQUILINUS_WORKING_AREA_V01.0.0";
const char* szPHeader_v1_0_0 = "AQUILINUS_GAME_PROFILE_V01.0.0";

#include"AQU_FileManager.h"
#include"AQU_GlobalTypes.h"

/**
* Constructor.
* Creates user directories.
* @param bCreate True if directories are to be created.
***/
AQU_FileManager::AQU_FileManager(bool bCreate) :
	m_dwProcessListSize(0),
	m_hSaveMapFile(nullptr),
	m_pcProcesses(nullptr)
{

	// load process file
	this->LoadProcessListCSV();
}

/**
* Destructor. Empty.
***/
AQU_FileManager::~AQU_FileManager()
{
	free(m_pcProcesses);

	// release map file handle
	if (m_hSaveMapFile)
	{
		UnmapViewOfFile((LPCVOID)m_hSaveMapFile);
		CloseHandle(m_hSaveMapFile);
	}
}

/**
* Loads the basic information from a working area file for the Inicio app.
***/
HRESULT AQU_FileManager::LoadWorkingAreaBasics(LPWSTR szWorkspacePath, DWORD& dwProcessIndex, DWORD& dwSupportedInterfacesNumber, int* pnInterfaceInjectionTechnique, LPWSTR szPicturePath, BOOL& bPicture, DWORD& dwDetourTimeDelay, bool bKeepProcessName)
{
	OPENFILENAME ofn;
	wchar_t szFileName[MAX_PATH] = L"";

	// get filename using the OPENFILENAME structure and GetOpenFileName()
	ZeroMemory(&ofn, sizeof(ofn));

	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = NULL;
	ofn.lpstrFilter = (LPCWSTR)L"Aquilinus Workspace (*.aquw)\0*.aquw\0";
	ofn.lpstrFile = (LPWSTR)szFileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = (LPCWSTR)L"aquw";
	ofn.lpstrTitle = L"Load Aquilinus Workspace";
	ofn.Flags = OFN_FILEMUSTEXIST;

	// the pure data stream
	std::stringstream sstrDataStream;

	if (GetOpenFileName(&ofn))
	{
		// open file
		std::ifstream inFile;
		inFile.open(ofn.lpstrFile, std::ios::in | std::ios::binary);
		if (inFile.is_open())
		{
			// write exact file path to the configuration
			CopyMemory((PVOID)szWorkspacePath, ofn.lpstrFile, wcslen(ofn.lpstrFile) * sizeof(wchar_t));

			// read header
			int nVersion = -1;
			char szHeaderCompare[WORKSPACE_HEADER_SIZE + 1];
			inFile.read(szHeaderCompare, WORKSPACE_HEADER_SIZE);
			szHeaderCompare[WORKSPACE_HEADER_SIZE] = 0;

			// get file version
			if (strcmp(szWHeader_v1_0_0, szHeaderCompare) == 0)
				nVersion = 0;

			// fail if version not supported
			if (nVersion == -1)
			{
				OutputDebugString(L"Aquilinus : Wrong workspace file version !");
				OutputDebugStringA(szHeaderCompare);
				OutputDebugStringA(szWHeader_v1_0_0);
				inFile.close();
				return E_FAIL;
			}

			DWORD dwInputHash;
			char pData[256];
			while (inFile.good())
			{
				// read hash and data
				inFile.read((char*)&dwInputHash, sizeof(DWORD));
				inFile.read(pData, 256);

				// get actual data size
				int nActualSize = (int)inFile.gcount();

				// is there any data ?
				if (nActualSize > 0)
				{
					// get the hash for this data block
					DWORD dwHash = this->GetHash((BYTE*)pData, (DWORD)nActualSize);

					// compare hash, write data
					if (dwHash == dwInputHash)
						sstrDataStream.write(pData, nActualSize);
					else
					{
						OutputDebugString(L"Aquilinus : Corrupt workspace file !");
						inFile.close();
						return E_FAIL;
					}
				}
			}
		}

		inFile.close();
	}
	else
		return E_FAIL;

	// set back binary stream
	sstrDataStream.seekg(0);
	sstrDataStream.seekp(0);

	// first, get the process index by the game name in the stream
	wchar_t szEntryName[MAX_JOLIET_FILENAME];
	sstrDataStream.read((char*)szEntryName, ENTRY_SIZE);
	int nIndex = -1;
	for (DWORD i = 0; i < GetProcessNumber(); i++)
	{
		LPCWSTR szNameCompare = GetName(i);
		if (wcscmp(szNameCompare, szEntryName) == 0)
			nIndex = (int)i;
	}

	// empty profile ?
	BOOL bEmptyProcess;
	if (szEntryName[0] == 0)
		bEmptyProcess = TRUE;
	else
		bEmptyProcess = FALSE;

	// do we keep the process name ?
	if ((!bKeepProcessName) && (!bEmptyProcess))
	{
		// game name in list ?
		if (nIndex < 0)
		{
			OutputDebugString(L"Aquilinus : Game not supported !");
			return E_FAIL;
		}
		else
			dwProcessIndex = (DWORD)nIndex;
	}

	// read the length of the additional option block (currently zero.. for future use)
	DWORD dwAdditionalDataBlockLength = 0;
	sstrDataStream.read((char*)&dwAdditionalDataBlockLength, sizeof(DWORD));

	// read profile picture boolean and the path if true
	sstrDataStream.read((char*)&bPicture, sizeof(BOOL));
	if (bPicture)
		sstrDataStream.read((char*)&szPicturePath[0], MAX_PATH * sizeof(wchar_t));

	// TODO !! PICTURES FOR WORKING AREAS ARE SET OFF HERE....
	bPicture = false;

	// read the detour time delay
	sstrDataStream.read((char*)&dwDetourTimeDelay, sizeof(DWORD));

	// get the injected classes booleans, first the number of supported interfaces
	sstrDataStream.read((char*)&dwSupportedInterfacesNumber, sizeof(DWORD));
	sstrDataStream.read((char*)&pnInterfaceInjectionTechnique[0], dwSupportedInterfacesNumber * sizeof(int));

	return S_OK;
}

/**
* Loads the basic information from a profile file for the Inicio app.
***/
HRESULT AQU_FileManager::LoadProfileBasics(LPCWSTR szProfilePath, AquilinusCfg* psConfig, DWORD& dwSupportedInterfacesNumber, BYTE*& paPictureData, DWORD& dwPictureSize)
{
	// set fields
	std::stringstream sstrDataStream;
	OPENFILENAME ofn;
	wchar_t szFileName[MAX_PATH] = L"";
	std::wstring szPath = std::wstring();
	if (szProfilePath) szPath = std::wstring(szProfilePath);

	// file path provided ? otherwise create open file window
	if (!szPath.length())
	{
		// get filename using the OPENFILENAME structure and GetOpenFileName()
		ZeroMemory(&ofn, sizeof(ofn));

		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = NULL;
		ofn.lpstrFilter = (LPCWSTR)L"Aquilinus Profile (*.aqup)\0*.aqup\0";
		ofn.lpstrFile = (LPWSTR)szFileName;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrDefExt = (LPCWSTR)L"aqup";
		ofn.lpstrTitle = L"Load Aquilinus Profile";
		ofn.Flags = OFN_FILEMUSTEXIST;

		if (GetOpenFileName(&ofn))
		{
			szPath = std::wstring(ofn.lpstrFile);
		}
	}

	// path present now ? proceed
	if (szPath.length())
	{
		// open file
		std::ifstream inFile;
		inFile.open(szPath.c_str(), std::ios::in | std::ios::binary);
		if (inFile.is_open())
		{
			// write exact file path to the configuration
			CopyMemory((PVOID)psConfig->szProfileFilePath, szPath.c_str(), szPath.length() * sizeof(wchar_t));

			// read header
			int nVersion = -1;
			char szHeaderCompare[PROFILE_HEADER_SIZE + 1];
			inFile.read(szHeaderCompare, PROFILE_HEADER_SIZE);
			szHeaderCompare[PROFILE_HEADER_SIZE] = 0;

			// get file version
			if (strcmp(szPHeader_v1_0_0, szHeaderCompare) == 0)
				nVersion = 0;

			// fail if version not supported
			if (nVersion == -1)
			{
				OutputDebugString(L"Aquilinus : Wrong profile file version !");
				OutputDebugStringA(szHeaderCompare);
				OutputDebugStringA(szPHeader_v1_0_0);
				inFile.close();
				return E_FAIL;
			}

			DWORD dwInputHash;
			char pData[256];
			while (inFile.good())
			{
				// read hash and data
				inFile.read((char*)&dwInputHash, sizeof(DWORD));
				inFile.read(pData, 256);

				// get actual data size
				int nActualSize = (int)inFile.gcount();

				// is there any data ?
				if (nActualSize > 0)
				{
					// get the hash for this data block
					DWORD dwHash = this->GetHash((BYTE*)pData, (DWORD)nActualSize);

					// compare hash, write data
					if (dwHash == dwInputHash)
						sstrDataStream.write(pData, nActualSize);
					else
					{
						OutputDebugString(L"Aquilinus : Corrupt profile file !");
						inFile.close();
						return E_FAIL;
					}
				}
			}
		}

		inFile.close();
	}
	else
		return E_FAIL;

	// set back binary stream
	sstrDataStream.seekg(0);
	sstrDataStream.seekp(0);

	// get new buffer
	SIZE_T nStreamSize = sstrDataStream.str().size();
	char* pDecryptedData = new char[nStreamSize];
	sstrDataStream.read(pDecryptedData, nStreamSize);

	// first, get the entry
	sstrDataStream.read((char*)psConfig->szEntryName, ENTRY_SIZE);

	// read the window name..
	sstrDataStream.read((char*)&psConfig->szWindowName[0], ENTRY_SIZE);

	// read the game process..
	sstrDataStream.read((char*)&psConfig->szProcessName[0], ENTRY_SIZE);

	// empty profile ?
	if ((psConfig->szEntryName[0] == 0) &&
		(psConfig->szWindowName[0] == 0) &&
		(psConfig->szProcessName[0] == 0))
		psConfig->bEmptyProcess = TRUE;
	else
		psConfig->bEmptyProcess = FALSE;

	// read the length of the additional option block (currently zero.. for future use)
	DWORD dwAdditionalDataBlockLength = 0;
	sstrDataStream.read((char*)&dwAdditionalDataBlockLength, sizeof(DWORD));

	// read profile picture boolean and the path if true
	sstrDataStream.read((char*)&psConfig->bProfileWindow, sizeof(BOOL));
	if (psConfig->bProfileWindow)
	{
		LONG nImageSize;
		sstrDataStream.read((char*)&nImageSize, sizeof(LONG));
		paPictureData = new BYTE[nImageSize];
		sstrDataStream.read((char*)&paPictureData[0], nImageSize);
		dwPictureSize = (DWORD)nImageSize;
	}

	// read the detour time delay
	sstrDataStream.read((char*)&psConfig->dwDetourTimeDelay, sizeof(DWORD));

	// get the injected classes booleans, first the number of supported interfaces
	sstrDataStream.read((char*)&dwSupportedInterfacesNumber, sizeof(DWORD));
	sstrDataStream.read((char*)&psConfig->eInjectionTechnique[0], dwSupportedInterfacesNumber * sizeof(int));

	return S_OK;
}

/**
* Load working area to pure data stream.
* @param szWorkspacePath The file path.
* @param sstrDataStream The pure data stream.
***/
HRESULT AQU_FileManager::LoadWorkingArea(LPWSTR szWorkspacePath, std::stringstream& sstrDataStream)
{
	// first, clear the data stream
	sstrDataStream.clear();

	// open file
	std::ifstream inFile;
	inFile.open(szWorkspacePath, std::ios::in | std::ios::binary);
	if (inFile.is_open())
	{
		// read header
		int nVersion = -1;
		char szHeaderCompare[WORKSPACE_HEADER_SIZE + 1];
		inFile.read(szHeaderCompare, WORKSPACE_HEADER_SIZE);
		szHeaderCompare[WORKSPACE_HEADER_SIZE] = 0;

		// get file version
		if (strcmp(szWHeader_v1_0_0, szHeaderCompare) == 0)
			nVersion = 0;

		// fail if version not supported
		if (nVersion == -1)
		{
			OutputDebugString(L"Aquilinus : Wrong workspace file version !");
			OutputDebugStringA(szHeaderCompare);
			OutputDebugStringA(szWHeader_v1_0_0);
			inFile.close();
			return E_FAIL;
		}

		DWORD dwInputHash;
		char pData[256];
		while (inFile.good())
		{
			// read hash and data
			inFile.read((char*)&dwInputHash, sizeof(DWORD));
			inFile.read(pData, 256);

			// get actual data size
			int nActualSize = (int)inFile.gcount();

			// is there any data ?
			if (nActualSize > 0)
			{
				// get the hash for this data block
				DWORD dwHash = this->GetHash((BYTE*)pData, (DWORD)nActualSize);

				// compare hash, write data
				if (dwHash == dwInputHash)
					sstrDataStream.write(pData, nActualSize);
				else
				{
					OutputDebugString(L"Aquilinus : Corrupt workspace file !");
					inFile.close();
					return E_FAIL;
				}
			}
		}

		inFile.close();
	}
	else
		return E_FAIL;

	// set back binary stream
	sstrDataStream.seekg(0);
	sstrDataStream.seekp(0);

	return S_OK;
}

/**
* Load profile to pure data stream.
* @param szWorkspacePath The file path.
* @param sstrDataStream The pure data stream.
***/
HRESULT AQU_FileManager::LoadProfile(LPWSTR szProfilePath, std::stringstream& sstrDataStream)
{
	sstrDataStream.clear();

	// open file
	std::ifstream inFile;
	inFile.open(szProfilePath, std::ios::in | std::ios::binary);
	if (inFile.is_open())
	{
		// read header
		int nVersion = -1;
		char szHeaderCompare[PROFILE_HEADER_SIZE + 1];
		inFile.read(szHeaderCompare, PROFILE_HEADER_SIZE);
		szHeaderCompare[PROFILE_HEADER_SIZE] = 0;

		// get file version
		if (strcmp(szPHeader_v1_0_0, szHeaderCompare) == 0)
			nVersion = 0;

		// fail if version not supported
		if (nVersion == -1)
		{
			OutputDebugString(L"Aquilinus : Wrong profile file version !");
			OutputDebugStringA(szHeaderCompare);
			OutputDebugStringA(szPHeader_v1_0_0);
			inFile.close();
			return E_FAIL;
		}

		DWORD dwInputHash;
		char pData[256];
		while (inFile.good())
		{
			// read hash and data
			inFile.read((char*)&dwInputHash, sizeof(DWORD));
			inFile.read(pData, 256);

			// get actual data size
			int nActualSize = (int)inFile.gcount();

			// is there any data ?
			if (nActualSize > 0)
			{
				// get the hash for this data block
				DWORD dwHash = this->GetHash((BYTE*)pData, (DWORD)nActualSize);

				// compare hash, write data
				if (dwHash == dwInputHash)
					sstrDataStream.write(pData, nActualSize);
				else
				{
					OutputDebugString(L"Aquilinus : Corrupt profile file !");
					inFile.close();
					return E_FAIL;
				}
			}
		}
	}

	inFile.close();

	// set back binary stream
	sstrDataStream.seekg(0);
	sstrDataStream.seekp(0);

	return S_OK;
}

/**
* Save working area.
* Saves all nodes to a .aquw file.
* @param psConfig Pointer to the Aquilinus configuration field.
* @param (*ppaNodes) Vector of all workspace nodes.
* @param dwSupportedInterfacesNumber The number of supported interfaces to create the interface injection table.
***/
HRESULT AQU_FileManager::SaveWorkingArea(AquilinusCfg* psConfig, std::vector<NOD_Basic*>* ppaNodes, DWORD dwSupportedInterfacesNumber)
{
	// get all necessary fields from the config
	DWORD dwProcessIndex = psConfig->dwProcessIndex;
	int* pnInterfaceInjectionTechnique = (int*)psConfig->eInjectionTechnique;
	LPWSTR szPath = psConfig->szAquilinusPath;
	LPWSTR szPicturePath = psConfig->szPictureFilePath;
	BOOL bPicture = psConfig->bProfileWindow;
	DWORD dwDetourTimeDelay = psConfig->dwDetourTimeDelay;
	BOOL bEmptyProfile = psConfig->bEmptyProcess;

	// create filename fields
	OPENFILENAME ofn;
	wchar_t szFileName[MAX_PATH] = L"";

	// get filename using the OPENFILENAME structure and GetSaveFileName()
	ZeroMemory(&ofn, sizeof(ofn));

	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = NULL;
	ofn.lpstrFilter = (LPCWSTR)L"Aquilinus Workspace (*.aquw)\0*.aquw\0";
	ofn.lpstrFile = (LPWSTR)szFileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = (LPCWSTR)L"aquw";
	ofn.lpstrTitle = L"Save Aquilinus Workspace";
	ofn.Flags = OFN_OVERWRITEPROMPT;

	if (GetSaveFileName(&ofn))
	{
		// get a binary stream
		std::stringstream binaryStream;

		if (bEmptyProfile)
		{
			// write down the game name entry 
			wchar_t szEntryName[MAX_JOLIET_FILENAME];
			ZeroMemory(szEntryName, ENTRY_SIZE);
			binaryStream.write((const char*)&szEntryName[0], ENTRY_SIZE);
		}
		else
		{
			// write down the game name entry 
			wchar_t* szEntryName = GetName(dwProcessIndex);
			binaryStream.write((const char*)&szEntryName[0], ENTRY_SIZE);
		}

		// write down the length of the additional option block (currently zero.. for future use)
		DWORD dwAdditionalDataBlockLength = 0;
		binaryStream.write((const char*)&dwAdditionalDataBlockLength, sizeof(DWORD));

		// write the profile picture boolean and the path if true
		binaryStream.write((const char*)&bPicture, sizeof(BOOL));
		if (bPicture)
			binaryStream.write((const char*)&szPicturePath[0], MAX_PATH * sizeof(wchar_t));

		// write down the detour time delay
		binaryStream.write((const char*)&dwDetourTimeDelay, sizeof(DWORD));

		// and the injected classes booleans, first the number of supported interfaces
		binaryStream.write((const char*)&dwSupportedInterfacesNumber, sizeof(DWORD));
		binaryStream.write((const char*)&pnInterfaceInjectionTechnique[0], dwSupportedInterfacesNumber * sizeof(int));

		// and the node number ...
		UINT dwNodeNumber = (UINT)(*ppaNodes).size();
		binaryStream.write((const char*)&dwNodeNumber, sizeof(UINT));

		// loop through the nodes to add node data 
		for (std::vector<NOD_Basic*>::size_type i = 0; i != (*ppaNodes).size(); i++)
		{
			// add node type
			UINT id = (*ppaNodes)[i]->GetNodeTypeId();
			binaryStream.write((const char*)&id, sizeof(UINT));

			// add node position
			POINT pos = (*ppaNodes)[i]->GetNodePosition();
			binaryStream.write((const char*)&pos.x, sizeof(LONG));
			binaryStream.write((const char*)&pos.y, sizeof(LONG));

			// test for plugin node
			if (id == ELEMENTARY_NODE_PLUGIN)
			{
				// get data (id + filename)
				NOD_Plugin* pPlugin = (NOD_Plugin*)(*ppaNodes)[i];
				UINT idPlugin = pPlugin->GetPluginNodeTypeId();
				LPCWSTR szFileName = pPlugin->GetPluginNodeFileName();

				// and write to stream
				binaryStream.write((const char*)&idPlugin, sizeof(UINT));
				binaryStream.write((const char*)&szFileName[0], sizeof(wchar_t) * 64);
			}

			// add node data
			UINT dwDataSize = 0;
			char* pcData = (*ppaNodes)[i]->GetSaveData(&dwDataSize);
			binaryStream.write((const char*)&dwDataSize, sizeof(UINT));
			if (dwDataSize)
				binaryStream.write((const char*)pcData, dwDataSize);
		}

		// loop through the nodes to add node connections
		for (std::vector<NOD_Basic*>::size_type i = 0; i != (*ppaNodes).size(); i++)
		{
			// write down the number of commanders
			DWORD dwCommandersNumber = (DWORD)(*ppaNodes)[i]->m_paCommanders.size();
			binaryStream.write((const char*)&dwCommandersNumber, sizeof(DWORD));

			// loop through commanders, write down the number of connections and the connection indices
			for (DWORD j = 0; j < dwCommandersNumber; j++)
			{
				// write the commander connections number
				DWORD dwConnectionsNumber = (*ppaNodes)[i]->GetCommanderConnectionsNumber(j);
				binaryStream.write((const char*)&dwConnectionsNumber, sizeof(DWORD));

				// write down the commander connection node indices
				LONG* pnNodeIndices = (*ppaNodes)[i]->GetCommanderConnectionIndices(ppaNodes, j);
				binaryStream.write((const char*)pnNodeIndices, dwConnectionsNumber * 2 * sizeof(LONG));

				// delete the array... we comment that out for now since that crashes for Unreal games
				// delete [] pnNodeIndices;
			}

			// write the provoker connections number
			DWORD dwConnectionsNumber = (*ppaNodes)[i]->GetProvokerConnectionsNumber();
			binaryStream.write((const char*)&dwConnectionsNumber, sizeof(DWORD));

			// write down the provoker connection node indices
			LONG* pnNodeIndices = (*ppaNodes)[i]->GetProvokerConnectionIndices();
			binaryStream.write((const char*)pnNodeIndices, dwConnectionsNumber * sizeof(LONG));
		}

		// set back binary stream
		binaryStream.seekg(0);
		binaryStream.seekp(0);

		// saved to memory page ? else save directly to file
		if (psConfig->bExternalSave)
		{
			// save working area file
			std::stringstream outFile;

			// first, write the header (version 1.0.0 right now)
			outFile.write(szWHeader_v1_0_0, WORKSPACE_HEADER_SIZE);

			// go through stream, write down hash code for every 256 bytes
			while (binaryStream.good())
			{
				// read data (size 256)
				char pData[256];
				binaryStream.read(pData, 256);

				// get actual data size
				int nActualSize = (int)binaryStream.gcount();

				if (nActualSize > 0)
				{
					// get the hash for this data block
					DWORD dwHash = this->GetHash((BYTE*)pData, (DWORD)nActualSize);

					// write hash, then the data block
					outFile.write((char*)&dwHash, sizeof(DWORD));
					outFile.write((char*)pData, nActualSize);
				}
			}

			// get the length of the stream
			long nLength = (long)outFile.tellp();
			if (nLength < 0) nLength = 0;

			// release old map file handle
			if (m_hSaveMapFile)
			{
				UnmapViewOfFile((LPCVOID)m_hSaveMapFile);
				CloseHandle(m_hSaveMapFile);
			}

			// create config memory page
			m_hSaveMapFile = CreateFileMapping(
				INVALID_HANDLE_VALUE,    // use paging file
				NULL,                    // default security
				PAGE_READWRITE,          // read/write access
				0,                       // maximum object size (high-order DWORD)
				nLength,                 // maximum object size (low-order DWORD)
				L"AquilinusSaveMap");     // name of the Aquilinus config

			// return if failed
			if (m_hSaveMapFile == NULL)
			{
				OutputDebugString(TEXT("Aquilinus : Could not create file mapping object.\n"));
				psConfig->bExternalSave = FALSE;
			}
			else
			{
				// create map view
				LPVOID pData =
					MapViewOfFile(m_hSaveMapFile,   // handle to map object
						FILE_MAP_ALL_ACCESS,            // read/write permission
						0,
						0,
						nLength);

				// return if failed
				if (pData == NULL)
				{
					OutputDebugString(TEXT("Aquilinus : Could not map view of file.\n"));
					CloseHandle(m_hSaveMapFile);
					psConfig->bExternalSave = FALSE;
				}
				else
				{
					// copy data
					CopyMemory(pData, outFile.str().c_str(), nLength);

					// copy save file path
					CopyMemory(psConfig->szExternalSaveFilePath, ofn.lpstrFile, sizeof(wchar_t) * MAX_PATH);

					// set file data size, inicio will save whenever this one is set
					psConfig->dwSizeOfExternalSaveFile = (DWORD)nLength;
				}
			}
		}
		else
		{
			// save working area file
			std::ofstream outFile;
			outFile.open(ofn.lpstrFile, std::ios::out | std::ios::binary);
			if (outFile.is_open())
			{
				// first, write the header (version 1.0.0 right now)
				outFile.write(szWHeader_v1_0_0, WORKSPACE_HEADER_SIZE);

				// go through stream, write down hash code for every 256 bytes
				while (binaryStream.good())
				{
					// read data (size 256)
					char pData[256];
					binaryStream.read(pData, 256);

					// get actual data size
					int nActualSize = (int)binaryStream.gcount();

					if (nActualSize > 0)
					{
						// get the hash for this data block
						DWORD dwHash = this->GetHash((BYTE*)pData, (DWORD)nActualSize);

						// write hash, then the data block
						outFile.write((char*)&dwHash, sizeof(DWORD));
						outFile.write((char*)pData, nActualSize);
					}
				}
				outFile.close();
			}
			else
				return E_FAIL;
		}

		return S_OK;
	}
	else
		return E_FAIL;
}

/**
* Compile a profile.
* Saves all data to a .aqup file containing the PNG data for the profile window image.
* File is encrypted.
***/
HRESULT AQU_FileManager::CompileProfile(AquilinusCfg* psConfig, std::vector<NOD_Basic*>* ppaNodes, DWORD dwSupportedInterfacesNumber)
{
	// get all necessary fields from the config
	DWORD dwProcessIndex = psConfig->dwProcessIndex;
	int* pnInterfaceInjectionTechnique = (int*)psConfig->eInjectionTechnique;
	LPWSTR szPath = psConfig->szAquilinusPath;
	LPWSTR szPicturePath = psConfig->szPictureFilePath;
	BOOL bPicture = psConfig->bProfileWindow;
	DWORD dwDetourTimeDelay = psConfig->dwDetourTimeDelay;
	BOOL bEmptyProfile = psConfig->bEmptyProcess;

	// get output fields
	OPENFILENAME ofn;
	wchar_t szFileName[MAX_PATH] = L"";

	// get filename using the OPENFILENAME structure and GetSaveFileName()
	ZeroMemory(&ofn, sizeof(ofn));

	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = NULL;
	ofn.lpstrFilter = (LPCWSTR)L"Aquilinus Profile (*.aqup)\0*.aqup\0";
	ofn.lpstrFile = (LPWSTR)szFileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = (LPCWSTR)L"aqup";
	ofn.lpstrTitle = L"Compile Aquilinus Profile";
	ofn.Flags = OFN_OVERWRITEPROMPT;

	if (GetSaveFileName(&ofn))
	{
		// first, get a buffer with the picture data
		std::vector<unsigned char> paImageBuffer;
		std::streamsize nImageSize = 0;

		if (bPicture)
		{
			std::ifstream file(szPicturePath, std::ios::in | std::ios::binary | std::ios::ate);

			// get filesize
			if (file.seekg(0, std::ios::end).good()) nImageSize = file.tellg();
			if (file.seekg(0, std::ios::beg).good()) nImageSize -= file.tellg();

			// read contents of the file into the vector
			if (nImageSize > 0)
			{
				paImageBuffer.resize((size_t)nImageSize);
				file.read((char*)(&paImageBuffer[0]), nImageSize);
			}
			else paImageBuffer.clear();

			file.close();
		}

		// get a binary stream
		std::stringstream binaryStream;

		if (bEmptyProfile)
		{
			// write down the game name entry 
			wchar_t szEntryName[MAX_JOLIET_FILENAME];
			ZeroMemory(szEntryName, ENTRY_SIZE);
			binaryStream.write((const char*)&szEntryName[0], ENTRY_SIZE);
		}
		else
		{
			// write down the game name entry 
			wchar_t* szEntryName = GetName(dwProcessIndex);
			binaryStream.write((const char*)&szEntryName[0], ENTRY_SIZE);
		}
		if (bEmptyProfile)
		{
			// write down the window name
			wchar_t szWindowName[MAX_JOLIET_FILENAME];
			ZeroMemory(szWindowName, ENTRY_SIZE);
			binaryStream.write((const char*)&szWindowName[0], ENTRY_SIZE);
		}
		else
		{
			// write down the window name
			wchar_t* szWindowName = GetWindowName(dwProcessIndex);
			binaryStream.write((const char*)&szWindowName[0], ENTRY_SIZE);
		}
		if (bEmptyProfile)
		{
			// write down the game process
			wchar_t szProcessName[MAX_JOLIET_FILENAME];
			ZeroMemory(szProcessName, ENTRY_SIZE);
			binaryStream.write((const char*)&szProcessName[0], ENTRY_SIZE);
		}
		else
		{
			// write down the game process
			wchar_t* szProcessName = GetProcessName(dwProcessIndex);
			binaryStream.write((const char*)&szProcessName[0], ENTRY_SIZE);
		}

		// write down the length of the additional option block (currently zero.. for future use)
		DWORD dwAdditionalDataBlockLength = 0;
		binaryStream.write((const char*)&dwAdditionalDataBlockLength, sizeof(DWORD));

		// write the profile picture boolean and the picture data (first, the size) if true
		binaryStream.write((const char*)&bPicture, sizeof(BOOL));
		if (bPicture)
		{
			LONG nISize = (LONG)nImageSize;
			binaryStream.write((const char*)&nISize, sizeof(LONG));
			binaryStream.write((const char*)(&paImageBuffer[0]), nISize);
		}

		// write down the detour time delay
		binaryStream.write((const char*)&dwDetourTimeDelay, sizeof(DWORD));

		// and the injected classes booleans, first the number of supported interfaces
		binaryStream.write((const char*)&dwSupportedInterfacesNumber, sizeof(DWORD));
		binaryStream.write((const char*)&pnInterfaceInjectionTechnique[0], dwSupportedInterfacesNumber * sizeof(int));

		// and the node number ...
		UINT dwNodeNumber = (UINT)(*ppaNodes).size();
		binaryStream.write((const char*)&dwNodeNumber, sizeof(UINT));

		// loop through the nodes to add node data 
		for (std::vector<NOD_Basic*>::size_type i = 0; i != (*ppaNodes).size(); i++)
		{
			// add node type
			UINT id = (*ppaNodes)[i]->GetNodeTypeId();
			binaryStream.write((const char*)&id, sizeof(UINT));

			// add node position
			POINT pos = (*ppaNodes)[i]->GetNodePosition();
			binaryStream.write((const char*)&pos.x, sizeof(LONG));
			binaryStream.write((const char*)&pos.y, sizeof(LONG));

			// test for plugin node
			if (id == ELEMENTARY_NODE_PLUGIN)
			{
				// get data (id + filename)
				NOD_Plugin* pPlugin = (NOD_Plugin*)(*ppaNodes)[i];
				UINT idPlugin = pPlugin->GetPluginNodeTypeId();
				LPCWSTR szFileName = pPlugin->GetPluginNodeFileName();

				// and write to stream
				binaryStream.write((const char*)&idPlugin, sizeof(UINT));
				binaryStream.write((const char*)&szFileName[0], sizeof(wchar_t) * 64);
			}

			// add node data
			UINT dwDataSize = 0;
			char* pcData = (*ppaNodes)[i]->GetSaveData(&dwDataSize);
			binaryStream.write((const char*)&dwDataSize, sizeof(UINT));
			if (dwDataSize)
				binaryStream.write((const char*)pcData, dwDataSize);
		}

		// loop through the nodes to add node connections
		for (std::vector<NOD_Basic*>::size_type i = 0; i != (*ppaNodes).size(); i++)
		{
			// write down the number of commanders
			DWORD dwCommandersNumber = (DWORD)(*ppaNodes)[i]->m_paCommanders.size();
			binaryStream.write((const char*)&dwCommandersNumber, sizeof(DWORD));

			// loop through commanders, write down the number of connections and the connection indices
			for (DWORD j = 0; j < dwCommandersNumber; j++)
			{
				// write the commander connections number
				DWORD dwConnectionsNumber = (*ppaNodes)[i]->GetCommanderConnectionsNumber(j);
				binaryStream.write((const char*)&dwConnectionsNumber, sizeof(DWORD));

				// write down the commander connection node indices
				LONG* pnNodeIndices = (*ppaNodes)[i]->GetCommanderConnectionIndices(ppaNodes, j);
				binaryStream.write((const char*)pnNodeIndices, dwConnectionsNumber * 2 * sizeof(LONG));

				// delete the array... we comment that out for now since that crashes for Unreal games
				// delete [] pnNodeIndices;
			}

			// write the provoker connections number
			DWORD dwConnectionsNumber = (*ppaNodes)[i]->GetProvokerConnectionsNumber();
			binaryStream.write((const char*)&dwConnectionsNumber, sizeof(DWORD));

			// write down the provoker connection node indices
			LONG* pnNodeIndices = (*ppaNodes)[i]->GetProvokerConnectionIndices();
			binaryStream.write((const char*)pnNodeIndices, dwConnectionsNumber * sizeof(LONG));
		}

		// set back binary stream
		binaryStream.seekg(0);
		binaryStream.seekp(0);

		// saved to memory page ? else save directly to file
		if (psConfig->bExternalSave)
		{
			// save profile file
			std::stringstream outFile;

			// first, write the header (version 1.0.0 right now)
			outFile.write(szPHeader_v1_0_0, PROFILE_HEADER_SIZE);

			// go through stream, write down hash code for every 256 bytes
			while (binaryStream.good())
			{
				// read data (size 256)
				char pData[256];
				binaryStream.read(pData, 256);

				// get actual data size
				int nActualSize = (int)binaryStream.gcount();

				if (nActualSize > 0)
				{
					// get the hash for this data block
					DWORD dwHash = this->GetHash((BYTE*)pData, (DWORD)nActualSize);

					// write hash, then the data block
					outFile.write((char*)&dwHash, sizeof(DWORD));
					outFile.write((char*)pData, nActualSize);
				}
			}

			// get the length of the stream
			long nLength = (long)outFile.tellp();
			if (nLength < 0) nLength = 0;

			// release old map file handle
			if (m_hSaveMapFile)
			{
				UnmapViewOfFile((LPCVOID)m_hSaveMapFile);
				CloseHandle(m_hSaveMapFile);
			}

			// create config memory page
			m_hSaveMapFile = CreateFileMapping(
				INVALID_HANDLE_VALUE,    // use paging file
				NULL,                    // default security
				PAGE_READWRITE,          // read/write access
				0,                       // maximum object size (high-order DWORD)
				nLength,                 // maximum object size (low-order DWORD)
				L"AquilinusSaveMap");     // name of the Aquilinus config

			// return if failed
			if (m_hSaveMapFile == NULL)
			{
				OutputDebugString(TEXT("Aquilinus : Could not create file mapping object.\n"));
				psConfig->bExternalSave = FALSE;
			}
			else
			{
				// create map view
				LPVOID pData =
					MapViewOfFile(m_hSaveMapFile,   // handle to map object
						FILE_MAP_ALL_ACCESS,            // read/write permission
						0,
						0,
						nLength);

				// return if failed
				if (pData == NULL)
				{
					OutputDebugString(TEXT("Aquilinus : Could not map view of file.\n"));
					CloseHandle(m_hSaveMapFile);
					psConfig->bExternalSave = FALSE;
				}
				else
				{
					// copy data
					CopyMemory(pData, outFile.str().c_str(), nLength);

					// copy save file path
					CopyMemory(psConfig->szExternalSaveFilePath, ofn.lpstrFile, sizeof(wchar_t) * MAX_PATH);

					// set file data size, inicio will save whenever this one is set
					psConfig->dwSizeOfExternalSaveFile = (DWORD)nLength;
				}
			}
		}
		else
		{
			// save profile file
			std::ofstream outFile;
			outFile.open(ofn.lpstrFile, std::ios::out | std::ios::binary);
			if (outFile.is_open())
			{
				// first, write the header (version 1.0.0 right now)
				outFile.write(szPHeader_v1_0_0, PROFILE_HEADER_SIZE);

				// go through stream, write down hash code for every 256 bytes
				while (binaryStream.good())
				{
					// read data (size 256)
					char pData[256];
					binaryStream.read(pData, 256);

					// get actual data size
					int nActualSize = (int)binaryStream.gcount();

					if (nActualSize > 0)
					{
						// get the hash for this data block
						DWORD dwHash = this->GetHash((BYTE*)pData, (DWORD)nActualSize);

						// write hash, then the data block
						outFile.write((char*)&dwHash, sizeof(DWORD));
						outFile.write((char*)pData, nActualSize);
					}
				}
				outFile.close();
			}
			else
			{
				return E_FAIL;
			}
		}

		return S_OK;
	}
	else
		return E_FAIL;
}

/**
* Set a custom Aquilinus path.
***/
HRESULT AQU_FileManager::SetCustomDirectoryPath(LPCWSTR szPath)
{
	// get a stringstream
	std::wstringstream wsstm = std::wstringstream(szPath);

	// set aquilinus path
	CopyMemory(m_szAquilinusPathW, wsstm.str().c_str(), sizeof(wchar_t) * MAX_PATH);
	OutputDebugString(L"Set custom Aquilinus path : ");
	OutputDebugString(m_szAquilinusPathW);

	// set profile path
	wsstm = std::wstringstream();
	wsstm << m_szAquilinusPathW << "My Profiles\\";
	CopyMemory(m_szProfilePathW, wsstm.str().c_str(), sizeof(wchar_t) * MAX_PATH);

	// set nodes path
	wsstm = std::wstringstream();
#ifdef _WIN64
	wsstm << m_szAquilinusPathW << "My Nodes x64\\";
#else
	wsstm << m_szAquilinusPathW << "My Nodes Win32\\";
#endif
	CopyMemory(m_szPluginPathW, wsstm.str().c_str(), sizeof(wchar_t) * MAX_PATH);

	// set projects path
	wsstm = std::wstringstream();
	wsstm << m_szAquilinusPathW << "My Projects\\";
	CopyMemory(m_szProjectPathW, wsstm.str().c_str(), sizeof(wchar_t) * MAX_PATH);

	return S_OK;
}

/**
* Returns Aquilinus path.
***/
LPCWSTR AQU_FileManager::GetAquilinusPath()
{
	return m_szAquilinusPathW;
}

/**
* Returns Aquilinus path.
***/
LPCWSTR AQU_FileManager::GetPluginPath()
{
	return m_szPluginPathW;
}

/**
* Returns Aquilinus path.
***/
LPCWSTR AQU_FileManager::GetProfilePath()
{
	return m_szProfilePathW;
}

/**
* Returns Aquilinus path.
***/
LPCWSTR AQU_FileManager::GetProjectPath()
{
	return m_szProjectPathW;
}

/**
* Returns the current number of processes.
***/
DWORD AQU_FileManager::GetProcessNumber()
{
	return m_dwProcessListSize / (PROCESS_ENTRY_SIZE);
}

/**
*
***/
LPWSTR AQU_FileManager::GetName(DWORD dwIndex)
{
	if (!m_pcProcesses) return nullptr;

	static WCHAR ret[ENTRY_SIZE];
	DWORD dwPos = dwIndex * 3 * MAX_JOLIET_FILENAME * sizeof(wchar_t);

	// copy process name
	CopyMemory((PVOID)ret, (PVOID)&m_pcProcesses[dwPos], ENTRY_SIZE);

	return ret;
}

/**
*
***/
LPWSTR AQU_FileManager::GetWindowName(DWORD dwIndex)
{
	if (!m_pcProcesses) return nullptr;

	static WCHAR ret[ENTRY_SIZE];
	DWORD dwPos = dwIndex * 3 * MAX_JOLIET_FILENAME * sizeof(wchar_t);

	// copy process name
	CopyMemory((PVOID)ret, (PVOID)&m_pcProcesses[dwPos + (ENTRY_SIZE)], ENTRY_SIZE);

	return ret;
}

/**
*
***/
LPWSTR AQU_FileManager::GetProcessName(DWORD dwIndex)
{
	if (!m_pcProcesses) return nullptr;

	static WCHAR ret[ENTRY_SIZE];
	DWORD dwPos = dwIndex * 3 * MAX_JOLIET_FILENAME * sizeof(wchar_t);

	// copy process name
	CopyMemory((PVOID)ret, (PVOID)&m_pcProcesses[dwPos + (2 * ENTRY_SIZE)], ENTRY_SIZE);

	return ret;
}

/**
* Get hash code helper.
***/
DWORD AQU_FileManager::GetHash(BYTE* pcData, DWORD dwSize)
{
	DWORD h = 0;

	// create hash
	for (DWORD i = 0; i < dwSize; i++)
	{
		h = 31 * h + pcData[i];
	}

	return h;
}

/**
*
***/
HRESULT AQU_FileManager::AddProcess(LPCWSTR szName, LPCWSTR szWindow, LPCWSTR szProcess)
{
	if ((wcslen(szName) == 0) || (wcslen(szWindow) == 0) || (wcslen(szProcess) == 0)) return E_FAIL;

	// allocate memory
	if (m_dwProcessListSize > 0)
	{
		// allocate new space
		BYTE* pByte = m_pcProcesses;
		m_pcProcesses = (BYTE*)malloc(m_dwProcessListSize + (PROCESS_ENTRY_SIZE));

		// fill with random characters to avoid cypher detection
		for (int i = 0; i < PROCESS_ENTRY_SIZE; i++)
			m_pcProcesses[m_dwProcessListSize + i] = (BYTE)(rand() % 256);

		// copy old list, delete old list
		CopyMemory((PVOID)m_pcProcesses, (PVOID)pByte, m_dwProcessListSize);
		free(pByte);
	}
	else
	{
		// allocate first space
		m_pcProcesses = (BYTE*)malloc(PROCESS_ENTRY_SIZE);

		// fill with zero
		for (int i = 0; i < PROCESS_ENTRY_SIZE; i++)
			m_pcProcesses[i] = (BYTE)0;

	}

	// copy names
	DWORD dwLen = (DWORD)wcslen(szName) + 1;
	if (dwLen > MAX_JOLIET_FILENAME) dwLen = MAX_JOLIET_FILENAME;
	CopyMemory((PVOID)&m_pcProcesses[m_dwProcessListSize], (PVOID)szName, dwLen * sizeof(wchar_t));
	dwLen = (DWORD)wcslen(szWindow) + 1;
	if (dwLen > MAX_JOLIET_FILENAME) dwLen = MAX_JOLIET_FILENAME;
	CopyMemory((PVOID)&m_pcProcesses[m_dwProcessListSize + (ENTRY_SIZE)], (PVOID)szWindow, dwLen * sizeof(wchar_t));
	dwLen = (DWORD)wcslen(szProcess) + 1;
	if (dwLen > MAX_JOLIET_FILENAME) dwLen = MAX_JOLIET_FILENAME;
	CopyMemory((PVOID)&m_pcProcesses[m_dwProcessListSize + (2 * ENTRY_SIZE)], (PVOID)szProcess, dwLen * sizeof(wchar_t));

	// set new size
	m_dwProcessListSize += (PROCESS_ENTRY_SIZE);

	return S_OK;
}

/**
* Loads the process list from the readable .csv file.
* Fully removes old process list !
***/
HRESULT AQU_FileManager::LoadProcessListCSV()
{
	UINT uI = 0;
	DEBUG_UINT(uI++);
	// clear old list
	if (m_pcProcesses) free(m_pcProcesses);
	m_pcProcesses = nullptr;
	m_dwProcessListSize = 0;
	DEBUG_UINT(uI++);

	// open proc file
	std::wifstream procFile;
#ifdef _WIN64
	procFile.open("proc_x64.csv", std::ios::in | std::ios::binary);
#else
	procFile.open("proc_x86.csv", std::ios::in | std::ios::binary);
#endif
	if (procFile.is_open())
	{
		DEBUG_UINT(uI++);
		// get the number of processes
		DWORD dwNumber = 0;
		std::wstring szLine;
		while (getline(procFile, szLine))
			++dwNumber;
		procFile.clear();
		procFile.seekg(0);
		DEBUG_UINT(uI++);

		// read line by line
		for (DWORD i = 0; i < dwNumber; i++)
		{
			// read the line
			std::getline(procFile, szLine);
			OutputDebugString(L"Read line...");
			OutputDebugString(szLine.c_str());

			// parse
			std::wstring delimiter = L",";
			size_t pos = 0;
			std::wstring szName;
			std::wstring szWinName;
			if ((pos = szLine.find(delimiter)) != std::wstring::npos)
			{
				szName = szLine.substr(0, pos);
				szLine.erase(0, pos + delimiter.length());
				OutputDebugString(szName.c_str());
			}
			if ((pos = szLine.find(delimiter)) != std::wstring::npos)
			{
				szWinName = szLine.substr(0, pos);
				szLine.erase(0, pos + delimiter.length());
				OutputDebugString(szWinName.c_str());
			}
			OutputDebugString(szLine.c_str());

			// and add the process
			AddProcess(szName.c_str(), szWinName.c_str(), szLine.c_str());
		}
		DEBUG_UINT(uI++);
	}
	else
	{
		OutputDebugString(L"File not found : proc.csv");
		return ERROR_FILE_NOT_FOUND;
	}
	DEBUG_UINT(uI++);

	procFile.close();
	DEBUG_UINT(uI++);

	// output all process names for debug reasons
	for (DWORD i = 0; i < this->GetProcessNumber(); i++)
	{
		OutputDebugString(this->GetName(i));
		OutputDebugString(this->GetWindowName(i));
		OutputDebugString(this->GetProcessName(i));
	}
	DEBUG_UINT(uI++);

	return S_OK;
}

/**
* Saves the process list as readable .csv file.
***/
HRESULT AQU_FileManager::SaveProcessListCSV()
{
	std::wofstream outfile;

	// open file
#ifdef _WIN64
	outfile.open("proc_x64.csv", std::ios::out);
#else
	outfile.open("proc_x86.csv", std::ios::out);
#endif

	// loop through processes, write line
	for (DWORD i = 0; i < this->GetProcessNumber(); i++)
	{
		std::wstring szName(this->GetName(i));
		std::wstring szWinName(this->GetWindowName(i));
		std::wstring szProcName(this->GetProcessName(i));

		// remove trademarks : 8482
		szName.erase(std::remove(szName.begin(), szName.end(), 8482), szName.end());
		szWinName.erase(std::remove(szWinName.begin(), szWinName.end(), 8482), szWinName.end());
		szProcName.erase(std::remove(szProcName.begin(), szProcName.end(), 8482), szProcName.end());

		// and write
		outfile << szName << L"," << szWinName << L"," << szProcName << std::endl;
	}

	outfile.close();

	return S_OK;
}

/**
* Saves the whole game list alphabetically as readable .txt file.
***/
HRESULT AQU_FileManager::SaveGameListTXT()
{
	std::vector<std::wstring> aszGameNames;

	// clear vector
	aszGameNames.clear();

	// add names
	for (DWORD i = 0; i < this->GetProcessNumber(); i++)
		aszGameNames.push_back(this->GetName(i));

	// and sort alphabetical
	std::sort(aszGameNames.begin(), aszGameNames.end());

	std::wofstream outfile;

	// open file
	outfile.open("proc.txt", std::ios::out);

	// loop through games, write line
	for (std::vector<NOD_Basic*>::size_type i = 0; i != aszGameNames.size(); i++)
	{
		// remove trademarks : 8482
		aszGameNames[i].erase(std::remove(aszGameNames[i].begin(), aszGameNames[i].end(), 8482), aszGameNames[i].end());

		// and write
		outfile << aszGameNames[i] << std::endl;
	}

	outfile.close();

	return S_OK;
}

