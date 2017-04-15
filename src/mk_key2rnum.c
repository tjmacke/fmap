#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "args.h"
#include "index.h"
#include "fmap.h"

#define	U_MSG_S	"usage: %s [ -help ] file-map\n"

static	ARGS_T	*args;
static	FLAG_T	flags[] = {
	{"-help", 1, AVK_NONE, AVT_BOOL, "0", "Use -help to print this message."},
	{"-v",    1, AVK_OPT,  AVT_UINT, "0", "Use -v to set the verbosity to 1, use -v=N to set it to N."}
};
static	int	n_flags = sizeof(flags)/sizeof(flags[0]);

static	int
mk_key2eref(FILE *, const char *);

static	int
idxcmp(const void *, const void *);

int
main(int argc, char *argv[])
{
	int	a_stat = AS_OK;
	const ARG_VAL_T	*a_val;
	int	verbose = 0;
	const char	*fmfname = NULL;
	int	err = 0;

	a_stat = TJM_get_args(argc, argv, n_flags, flags, 1, 1, &args);
	if(a_stat != AS_OK){
		err = a_stat != AS_HELP_ONLY;
		goto CLEAN_UP;
	}

	a_val = TJM_get_flag_value(args, "-v", AVT_UINT);
	verbose = a_val->av_value.v_int;

	fmfname = args->a_files[0];

	if(verbose > 1)
		TJM_dump_args(stderr, args);

	if(mk_key2eref(stdout, fmfname)){
		err = 1;
		goto CLEAN_UP;
	}

CLEAN_UP : ;

	TJM_free_args(args);

	exit(err);
}

static	int
mk_key2eref(FILE *fp, const char *fmfname)
{
	FMAP_T	*fmap;
	int	ok;
	int	f, c, nrecs;
	FM_ENTRY_T	*fme;
	HDR_T	hdr;
	KEY2RNUM_T	*ip, *idx = NULL; 
	FILE	*ixfp = NULL;
	int	n_idx;
	FILE	*afp = NULL;
	char	*line = NULL;
	size_t	s_line = 0;
	int	err = 0;

	if((fmap = FMread_fmap(fmfname)) == NULL){
		err = 1;
		goto CLEAN_UP;
	}

	fme = fmap->f_entries;
	for(ok = 1, f = 0; f < fmap->f_nentries; f++, fme++){
		if(fme->f_first == UNDEF){
			LOG_ERROR("fmap[%d] = '%s %d %s': nrecs = UNDEF", f+1, fme->f_dname, fme->f_part, fme->f_fname);
			ok = 0;
		}
	}
	if(!ok){
		LOG_ERROR("some '%s' entries have no nrecs= info", fmfname);
		err = 1;
		goto CLEAN_UP;
	}else
		nrecs = fmap->f_entries[fmap->f_nentries - 1].f_last + 1;
	LOG_INFO("fmap: %s: %d entries", fmfname, nrecs);

	n_idx = nrecs;
	idx = (KEY2RNUM_T *)malloc(n_idx * sizeof(KEY2RNUM_T));
	if(idx == NULL){
		LOG_ERROR("can't allocate idx");
		err = 1;
		goto CLEAN_UP;
	}

	ip = idx;
	for(fme=fmap->f_entries, f = 0; f < fmap->f_nentries; f++, fme++){
		if((afp = FMfopen(
			fmap->f_root,fme->f_fname,fmap->f_key,"r")) == NULL)
		{
			err = 1;
			goto CLEAN_UP;
		}
		nrecs = fme->f_last - fme->f_first + 1;
		for(c = 0; getline(&line, &s_line, afp) > 0; ){
			if(c < nrecs){
				// TODO: This was %s, so it stopped at space, add option/
				sscanf(line, "%[^\n]", ip->k_key);
				ip->k_rnum = fme->f_first + c;
				ip++;
			}
			c++;
		}
		fclose(afp);
		afp = NULL;
		if(c != nrecs){
			LOG_ERROR("fme: '%s %d %s', has %d recs, expecting %d", fme->f_dname, fme->f_part, fme->f_fname, c, nrecs);
			err = 1;
			goto CLEAN_UP;
		}
	}

	qsort(idx, n_idx, sizeof(KEY2RNUM_T), idxcmp);

	hdr.h_count = n_idx;
	hdr.h_size = sizeof(KEY2RNUM_T);

	if((ixfp = FMfopen(fmap->f_root, "", fmap->f_index, "w")) == NULL){
		err = 1;
		goto CLEAN_UP;
	}

	fwrite(&hdr, sizeof(hdr), 1L, ixfp);
	fwrite(idx, sizeof(KEY2RNUM_T), (long)n_idx, ixfp);

CLEAN_UP : ;

	if(line != NULL)
		free(line);

	if(idx != NULL)
		free(idx);

	if(ixfp != NULL)
		fclose(ixfp);

	return  err;
}

static	int
idxcmp(const void *k1, const void *k2)
{

	return(strcmp(((KEY2RNUM_T *)k1)->k_key, ((KEY2RNUM_T *)k2)->k_key));
}
