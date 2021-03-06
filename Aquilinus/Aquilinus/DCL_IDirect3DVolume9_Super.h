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
#ifndef DCL_IDIRECT3DVOLUME9_SUPER
#define DCL_IDIRECT3DVOLUME9_SUPER

/*** IUnknown method prototypes ***/
typedef HRESULT (WINAPI* D3D9_QueryInterface)  (IDirect3DVolume9* pcThis,REFIID riid, void** ppvObj);
typedef ULONG   (WINAPI* D3D9_AddRef)          (IDirect3DVolume9* pcThis);
typedef ULONG   (WINAPI* D3D9_Release)         (IDirect3DVolume9* pcThis);

/*** IDirect3DVolume9 method prototypes ***/
typedef HRESULT (WINAPI* D3D9_GetDevice)       (IDirect3DVolume9* pcThis,  IDirect3DDevice9** ppDevice);
typedef HRESULT (WINAPI* D3D9_SetPrivateData)  (IDirect3DVolume9* pcThis,  REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags);
typedef HRESULT (WINAPI* D3D9_GetPrivateData)  (IDirect3DVolume9* pcThis,  REFGUID refguid,void* pData,DWORD* pSizeOfData);
typedef HRESULT (WINAPI* D3D9_FreePrivateData) (IDirect3DVolume9* pcThis,  REFGUID refguid);
typedef HRESULT (WINAPI* D3D9_GetContainer)    (IDirect3DVolume9* pcThis,  REFIID riid, void** ppContainer);
typedef HRESULT (WINAPI* D3D9_GetDesc)         (IDirect3DVolume9* pcThis,  D3DVOLUME_DESC *pDesc);
typedef HRESULT (WINAPI* D3D9_LockBox)         (IDirect3DVolume9* pcThis,  D3DLOCKED_BOX * pLockedVolume,CONST D3DBOX* pBox,DWORD Flags);
typedef HRESULT (WINAPI* D3D9_UnlockBox)       (IDirect3DVolume9* pcThis);

/*** IUnknown super methods ***/
D3D9_QueryInterface               D3D9_IDirect3DVolume9_QueryInterface_Super;
D3D9_AddRef                       D3D9_IDirect3DVolume9_AddRef_Super;
D3D9_Release                      D3D9_IDirect3DVolume9_Release_Super;

/*** IDirect3DVolume9 super methods ***/
D3D9_GetDevice       D3D9_IDirect3DVolume9_GetDevice_Super;
D3D9_SetPrivateData  D3D9_IDirect3DVolume9_SetPrivateData_Super;
D3D9_GetPrivateData  D3D9_IDirect3DVolume9_GetPrivateData_Super;
D3D9_FreePrivateData D3D9_IDirect3DVolume9_FreePrivateData_Super;
D3D9_GetContainer    D3D9_IDirect3DVolume9_GetContainer_Super;
D3D9_GetDesc         D3D9_IDirect3DVolume9_GetDesc_Super;
D3D9_LockBox         D3D9_IDirect3DVolume9_LockBox_Super;
D3D9_UnlockBox       D3D9_IDirect3DVolume9_UnlockBox_Super;

#endif