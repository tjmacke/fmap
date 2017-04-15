#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/evp.h>

#include "log.h"
#include "args.h"

#define	MD_NAME	"md5"

static	ARGS_T	*args;
static	FLAG_T	flags[] = {
	{"-help", 1, AVK_NONE, AVT_BOOL, "0", "Use -help to print this message."},
	{"-v",    1, AVK_OPT,  AVT_UINT, "0", "Use -v to set the verbosity to 1, use -v=N to set it to N."}
};
static	int	n_flags = sizeof(flags)/sizeof(flags[0]);

int
main(int argc, char *argv[])
{
	int	a_stat = AS_OK;
	const ARG_VAL_T	*a_val;
	int	verbose = 0;
	FILE	*fp = NULL;
	char	*line = NULL;
	size_t	s_line = 0;
	ssize_t	l_line;
	int	lcnt;
	EVP_MD_CTX	mdctx;
	int	mdctx_init = 0;
	const EVP_MD	*md;
	unsigned char	md_value[EVP_MAX_MD_SIZE];
	unsigned int	md_len;
	char	md_value_str[(2*EVP_MAX_MD_SIZE + 1)];
	char	*mvp;
	int	i;
	int	err = 0;

	a_stat = TJM_get_args(argc, argv, n_flags, flags, 0, 1, &args);
	if(a_stat != AS_OK){
		err = a_stat != AS_HELP_ONLY;
		goto CLEAN_UP;
	}

	a_val = TJM_get_flag_value(args, "-v", AVT_UINT);
	verbose = a_val->av_value.v_int;

	if(verbose > 1)
		TJM_dump_args(stderr, args);

	// Call once before making any digests
	OpenSSL_add_all_digests();
	md = EVP_get_digestbyname(MD_NAME);
	if(md == NULL){
		LOG_ERROR("no such digest %s", MD_NAME);
		err = 1;
		goto CLEAN_UP;
	}
	EVP_MD_CTX_init(&mdctx);
	mdctx_init = 1;

	if(args->an_files == 0)
		fp = stdin;
	else if((fp = fopen(args->a_files[0], "r")) == NULL){
		LOG_ERROR("can't read line file %s", args->a_files[0]);
		err = 1;
		goto CLEAN_UP;
	}

	for(lcnt = 0; (l_line = getline(&line, &s_line, fp)) > 0; ){
		lcnt++;
		if(line[l_line - 1] == '\n'){
			line[l_line - 1] = '\0';
			l_line--;
			if(line == 0){
				LOG_ERROR("line %7d: blank line not allowed", lcnt);
				err = 1;
				goto CLEAN_UP;
			}
		}

		// Call these three for each digest
		EVP_DigestInit_ex(&mdctx, md, NULL);
		EVP_DigestUpdate(&mdctx, line, l_line); 
		EVP_DigestFinal_ex(&mdctx, md_value, &md_len);
	
		for(mvp = md_value_str, i = 0; i < md_len; i++, mvp += 2)
			sprintf(mvp, "%02x", md_value[i] & 0xff);
		*mvp = '\0';
		printf("%s\n", md_value_str);
	}

CLEAN_UP : ; 

	if(fp != NULL && fp != stdin)
		fclose(fp);

	// Call once after all digests have been make
	if(mdctx_init)
		EVP_MD_CTX_cleanup(&mdctx);

	TJM_free_args(args);

	exit(0);
}
