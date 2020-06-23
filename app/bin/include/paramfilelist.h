#ifndef HAVE_PARAMFILELIST_H
	#define HAVE_PARAMFILELIST_H
	#include <stdbool.h>
	#include "include/paramfile.h"

	typedef struct {
		char * name;				/** < name of parameter file */
		char * contents;
		int deleted;
		int deletedShadow;
		int valid;					/** < FALSE for dropped file */
		bool favorite;
		enum paramFileState trackState;
		enum paramFileState structureState;
	} paramFileInfo_t;
	typedef paramFileInfo_t * paramFileInfo_p;

	#define paramFileInfo(N) DYNARR_N( paramFileInfo_t, paramFileInfo_da, N )
	
	char *GetParamFileDir(void);
	void SetParamFileDir(char *fullPath);
	void LoadParamFileList(void);
	BOOL_T ReadDefaultParams(const char * dirName);
	void SaveParamFileList(void);
	int GetParamFileCount();
	void UpdateParamFileList(void);
	void ParamFilesChange(long changes);
	int LoadParamFile(int files, char ** fileName, void * data);
	BOOL_T ParamFileListInit(void);

	void SearchUiOk(void * junk);
	bool UnloadParamFile(wIndex_t fileIndex);
#endif // !HAVE_PARAMFILELIST_H
