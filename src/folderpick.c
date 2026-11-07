#define COBJMACROS
#include "folderpick.h"

#include <windows.h>
#include <shobjidl.h>

int folder_pick(char *out_utf8, size_t out_size) {
	int result = -1;
	HRESULT init = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	int must_uninit = SUCCEEDED(init); /* RPC_E_CHANGED_MODE: COM já iniciado de outro jeito, segue sem desfazer */

	IFileOpenDialog *dialog = NULL;
	if (FAILED(CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_ALL, &IID_IFileOpenDialog, (void **) &dialog))) {
		goto done;
	}

	DWORD options = 0;
	IFileOpenDialog_GetOptions(dialog, &options);
	IFileOpenDialog_SetOptions(dialog, options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);

	HRESULT shown = IFileOpenDialog_Show(dialog, GetActiveWindow());
	if (shown == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
		result = 0;
		goto done;
	}
	if (FAILED(shown)) goto done;

	IShellItem *item = NULL;
	if (FAILED(IFileOpenDialog_GetResult(dialog, &item))) goto done;

	PWSTR wpath = NULL;
	if (SUCCEEDED(IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &wpath)) && wpath) {
		if (WideCharToMultiByte(CP_UTF8, 0, wpath, -1, out_utf8, (int) out_size, NULL, NULL) != 0) {
			for (char *c = out_utf8; *c; c++) {
				if (*c == '\\') *c = '/'; /* o mGBA só reconhece '/' */
			}
			result = 1;
		}
		CoTaskMemFree(wpath);
	}
	IShellItem_Release(item);

done:
	if (dialog) IFileOpenDialog_Release(dialog);
	if (must_uninit) CoUninitialize();
	return result;
}
