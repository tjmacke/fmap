#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "log.h"
#include "args.h"
#include "fmap.h"

//#define	U_MSG_S	"usage: %s [ -help ] [ -f ] -fmap file-map [ data-file ]\n"

static	ARGS_T	*args;
static	FLAG_T	flags[] = {
	{"-help", 1, AVK_NONE, AVT_BOOL, "0",  "Use -help to print this message."},
	{"-v",    1, AVK_OPT,  AVT_UINT, "0",  "Use -v to set the verbosity to 1, use -v=N to set it to N."},
	{"-f",    1, AVK_NONE, AVT_BOOL, "0",  "Use -f to continue even if a .BAK file can not be created."},
	{"-fmap", 0, AVK_REQ,  AVT_STR,  NULL, "Use -fmap FM to update file map FM."}
};
static	int	n_flags = sizeof(flags)/sizeof(flags[0]);

int
main(int argc, char *argv[])
{
	int	a_stat = AS_OK;
	const ARG_VAL_T	*a_val;
	int	verbose = 0;
	int	fopt = 0;
	const char	*fmfname = NULL;
	FMAP_T	*fmap = NULL;
	char	*dfname = NULL;
	FILE	*dfp = NULL;
	char	b_fmfname[256];
	FILE	*bfp = NULL;
	FILE	*ofp = NULL;
	int	c;
	int	err = 0;

	a_stat = TJM_get_args(argc, argv, n_flags, flags, 0, 1, &args);
	if(a_stat != AS_OK){
		err = a_stat != AS_HELP_ONLY;
		goto CLEAN_UP;
	}

	a_val = TJM_get_flag_value(args, "-v", AVT_UINT);
	verbose = a_val->av_value.v_int;

	a_val = TJM_get_flag_value(args, "-f", AVT_BOOL);
	fopt = a_val->av_value.v_int;

	a_val = TJM_get_flag_value(args, "-fmap", AVT_STR);
	fmfname = a_val->av_value.v_str;

	if(verbose > 1)
		TJM_dump_args(stderr, args);

	if((fmap = FMread_fmap(fmfname)) == NULL){
		err = 1;
		goto CLEAN_UP;
	}

	if(args->an_files == 0)
		dfp = stdin;
	else if((dfp = fopen(args->a_files[0], "r")) == NULL){
		LOG_ERROR("can't read data-file %s", args->a_files[0]);
		err = 1;
		goto CLEAN_UP;
	}

	if(FMupd_fmap(dfp, fmap)){
		sprintf(b_fmfname, "%s.bak", fmfname);
		if((bfp = fopen(b_fmfname, "w")) == NULL){
			LOG_ERROR("can't write backup file %s", b_fmfname);
			if(!fopt){
				err = 1;
				goto CLEAN_UP;
			}
		}
		if((ofp = fopen(fmfname, "r+")) == NULL){
			LOG_ERROR("can't update file map %s", fmfname);
			err = 1;
			goto CLEAN_UP;
		}
		while((c = getc(ofp)) != EOF)
			putc(c, bfp);
		fclose(bfp);
		bfp = NULL;

		rewind(ofp);
		if(ftruncate(fileno(ofp), (off_t)0)){
			LOG_ERROR("can't ftruncate file map %s", fmfname);
			err = 1;
			goto CLEAN_UP;
		}

		FMwrite_fmap(ofp, fmap);
		fclose(ofp);
		ofp = NULL;
	}

CLEAN_UP : ;

	if(dfp != NULL && dfp != stdin)
		fclose(dfp);

	if(bfp != NULL)
		fclose(bfp);

	if(ofp != NULL)
		fclose(ofp);

	TJM_free_args(args);

	exit(err);
}
