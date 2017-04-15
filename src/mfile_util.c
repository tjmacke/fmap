#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "log.h"
#include "index.h"
#include "mfile_util.h"

MFILE_T	*
mfile_new(const char *pfx, const char *ext)
{
	MFILE_T	*mfp = NULL;
	size_t	s_pfx;
	size_t	size;
	int	err = 0;

	mfp = (MFILE_T *)calloc((size_t)1, sizeof(MFILE_T));
	if(mfp == NULL){
		LOG_ERROR("can't allocate mfp");
		err = 1;
		goto CLEAN_UP;
	}

	s_pfx = strlen(pfx);
	size = s_pfx + 1 + strlen(ext) + 1;
	mfp->m_fname = (char *)malloc(size * sizeof(char));
	if(mfp == NULL){
		LOG_ERROR("can't allocate m_fname");
		err = 1;
		goto CLEAN_UP;
	}
	sprintf(mfp->m_fname, "%s.%s", pfx, ext);

CLEAN_UP : ;

	if(err){
		mfile_delete(mfp);
		mfp = NULL;
	}

	return mfp;
}

int
mfile_open(MFILE_T *mfp, const char *mf_mode, int h_hdr)
{
	int	fd;
	struct stat	sbuf;
	const char	*fmode;
	int	prot;
	HDR_T	*hdr;
	int	err = 0;

	if(mfp == NULL){
		LOG_ERROR("mfp is NULL");
		err = 1;
		goto CLEAN_UP;
	}

	if(mf_mode == NULL){
		LOG_ERROR("mf_mode is NULL");
		err = 1;
		goto CLEAN_UP;
	}else if(!strcmp(mf_mode, "r")){
		fmode = "r";
		prot = PROT_READ;
	}else if(!strcmp(mf_mode, "u")){
		fmode = "r+";
		prot = (PROT_READ|PROT_WRITE);
	}else if(!strcmp(mf_mode, "w")){
		fmode = "w";
		prot = 0;
	}else{
		LOG_ERROR("unknown mf_mode %s", mf_mode);
		err = 1;
		goto CLEAN_UP;
	}

	if((mfp->m_fp = fopen(mfp->m_fname, fmode)) == NULL){
		LOG_ERROR("can't open file %s with mode %s", mfp->m_fname, fmode);
		err = 1;
		goto CLEAN_UP;
	}

	if(*fmode != 'w'){	// only a file opened for read/update will be mapped
		fd = fileno(mfp->m_fp);
		if(fstat(fd, &sbuf)){
			LOG_ERROR("can't stat file %s", mfp->m_fname);
			err = 1;
			goto CLEAN_UP;
		}
		mfp->ms_file = sbuf.st_size;
		if((mfp->m_map = mmap(NULL, mfp->ms_file, prot, MAP_SHARED, fd, (off_t)0)) == NULL){
			LOG_ERROR("can't mmap file %s with prot %d", mfp->m_fname, prot);
			err = 1;
			goto CLEAN_UP;
		}
		if(h_hdr){
			hdr = (HDR_T *)mfp->m_map;
			mfp->mn_tab = hdr->h_count;
			mfp->m_tab = &((char *)mfp->m_map)[sizeof(HDR_T)];
		}else{
			mfp->mn_tab = 0;
			mfp->m_tab = mfp->m_map;
		}
	}

CLEAN_UP : ;

	return err;
}

void
mfile_delete(MFILE_T *mfp)
{

	if(mfp == NULL)
		return;

	if(mfp->m_fname != NULL){
		if(mfp->m_fp != NULL){
			if(mfp->m_map != NULL)
				munmap(mfp->m_map, mfp->ms_file);
			fclose(mfp->m_fp);
		}
		free(mfp->m_fname);
	}
	free(mfp);
}
