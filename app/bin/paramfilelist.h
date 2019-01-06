#ifndef HAVE_PARAMFILELIST_H
	#define HAVE_PARAMFILELIST_H
	#include <stdbool.h>
	#include "paramfile.h"

	typedef struct {
		char * name;				/** < name of parameter file */
		char * contents;
		int deleted;
		int deletedShadow;
		int valid;					/** < FALSE for dropped file */
		enum paramFileState trackState;
		enum paramFileState structureState;
	} paramFileInfo_t;
	typedef paramFileInfo_t * paramFileInfo_p;

	#define paramFileInfo(N) DYNARR_N( paramFileInfo_t, paramFileInfo_da, N )
	
	char *GetParamFileListDir(void);
	void SetParamFileListDir(char *fullPath);
	void LoadParamFileList(void);
	void SaveParamFileList(void);
	int GetParamFileCount();

	int LoadParamFile(int files, char ** fileName, void * data);
	void InitializeParamDir(void);
	void ParamFileListConfirmChange(void);
	void ParamFileListCancelChange(void);
	BOOL_T ParamFileListInit(void);

#endif // !HAVE_PARAMFILELIST_H