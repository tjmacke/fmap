#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/evp.h>

#include "log.h"
#include "index.h"
#include "args.h"

#define	MD_NAME	"md5"

static	ARGS_T	*args;
static	FLAG_T	flags[] = {
	{"-help", 1, AVK_NONE, AVT_BOOL, "0",  "Use -help to print this message."},
	{"-v",    1, AVK_OPT,  AVT_UINT, "0",  "Use -v to set the verbosity to 1; use -v=N to set it to N."},
	{"-kf",   0, AVK_REQ,  AVT_STR , NULL, "Use -kf N, N>0 to set the key to field N; use -kf @F to read keys from file F; -kf @F requires -md5."},
	{"-md5",  1, AVK_NONE, AVT_BOOL, "0",  "Use -md5 to use md5(kf) as the key."}
};
static	int	n_flags = sizeof(flags)/sizeof(flags[0]);

static	int
indexlines(const char *, int, const char *);

int
main(int argc, char *argv[])
{
	int	a_stat = AS_OK;
	const ARG_VAL_T	*a_val;
	int	verbose = 0;
	const char	*kf_str = NULL;
	int	md5 = 0;
	char	*fname = NULL;
	int	err = 0;

	a_stat = TJM_get_args(argc, argv, n_flags, flags, 1, 1, &args);
	if(a_stat != AS_OK){
		err = a_stat != AS_HELP_ONLY;
		goto CLEAN_UP;
	}

	a_val = TJM_get_flag_value(args, "-v", AVT_UINT);
	verbose = a_val->av_value.v_int;

	a_val = TJM_get_flag_value(args, "-kf", AVT_STR);
	kf_str = a_val->av_value.v_str;

	a_val = TJM_get_flag_value(args, "-md5", AVT_BOOL);
	md5 = a_val->av_value.v_int;

	if(*kf_str == '@' && !md5){
		LOG_ERROR("reading keys from a file (-kf %s), requires -md5", kf_str);
		err = 1;
		goto CLEAN_UP;
	}

	if(verbose > 1)
		TJM_dump_args(stderr, args);

	if(indexlines(args->a_files[0], md5, kf_str)){
		err = 1;
		goto CLEAN_UP;
	}

CLEAN_UP : ;

	TJM_free_args(args);

	exit(err);
}

static	int
indexlines(const char *fname, int md5, const char *kf_str)
{
	const char	*fnp, *dot, *slash;
	const char	*rnp, *e_rnp;
	char	*bname = NULL;
	size_t	l_bname, l_fname;
	char	*rixfname = NULL;
	int	rd_kfile = 0;
	int	kf;
	char	*key = NULL;
	size_t	s_key = 0;
	ssize_t	l_key;
	char	*kfname = NULL;
	char	*md5fname = NULL;
	FILE	*fp = NULL;
	FILE	*rixfp = NULL;
	FILE	*kfp = NULL;
	FILE	*md5fp = NULL;
	HDR_T	rixhdr;
	RIDX_T	ridx;
	size_t	roff;
	size_t	rlen;
	char	*line = NULL;
	size_t	s_line = 0;
	ssize_t	l_line;
	int	lcnt;

	EVP_MD_CTX	mdctx;
	int	mdctx_init = 0;
	const	EVP_MD	*md;
	unsigned char	md_value[EVP_MAX_MD_SIZE];
	int	md_len;
	char	md_value_str[(2*EVP_MAX_MD_SIZE + 1)];

	int	err = 0;

	for(dot = slash = NULL, fnp = fname; *fnp; fnp++){
		if(*fnp == '.')
			dot = fnp;
		else if(*fnp == '/')
			slash = fnp;
	}
	if(slash != NULL){
		if(dot == NULL || dot < slash){
			bname = strdup(&slash[1]);
		}else
			bname = strndup(&slash[1], dot - (slash + 1));
	}else if(dot != NULL)
		bname = strndup(fname, dot - fname);
	else
		bname = strdup(fname);
	if(bname == NULL){
		LOG_ERROR("can't strdup/strndup bname");
		err = 1;
		goto CLEAN_UP;
	}
	l_bname = strlen(bname);
	if(l_bname == 0){
		LOG_ERROR("fname %s has zero length base name", fname);
		err = 1;
		goto CLEAN_UP;
	}

	l_fname = l_bname + 1 + strlen("lidx") + 1;
	rixfname = (char *)malloc(l_fname * sizeof(char));
	if(rixfname == NULL){
		LOG_ERROR("can't allocate rixfname");
		err = 1;
		goto CLEAN_UP;
	}
	sprintf(rixfname, "%s.%s", bname, "lidx");
	if(*kf_str == '@')
		rd_kfile = 1;
	else{
		l_fname = l_bname + 1 + strlen("key") + 1;
		kfname = (char *)malloc(l_fname * sizeof(char));
		if(kfname == NULL){
			LOG_ERROR("can't allocate kfname");
			err = 1;
			goto CLEAN_UP;
		}
		sprintf(kfname, "%s.%s", bname, "key");
		kf = atoi(kf_str);
	}
	if(md5){
		l_fname = l_bname + 1 + strlen("md5") + 1;
		md5fname = (char *)malloc(l_fname * sizeof(char));
		if(md5fname == NULL){
			LOG_ERROR("can't allocate md5fname");
			err = 1;
			goto CLEAN_UP;
		}
		sprintf(md5fname, "%s.%s", bname, "md5");
	}

	if((fp = fopen(fname, "r")) == NULL){
		LOG_ERROR("can't read line-file %s", fname); 
		err = 1;
		goto CLEAN_UP;
	}
	if((rixfp = fopen(rixfname, "w")) == NULL){
		LOG_ERROR("can't write ridx-file %s", rixfname); 
		err = 1;
		goto CLEAN_UP;
	}
	if(rd_kfile){
		if((kfp = fopen(&kf_str[1], "r")) == NULL){
			LOG_ERROR("can't read kfile %s", &kf_str[1]);
			err = 1;
			goto CLEAN_UP;
		}
	}else if((kfp = fopen(kfname, "w")) == NULL){
		LOG_ERROR("can't write key-file %s", kfname); 
		err = 1;
		goto CLEAN_UP;
	}
	if(md5){
		if((md5fp = fopen(md5fname, "w")) == NULL){
			LOG_ERROR("can't write key-file %s", kfname); 
			err = 1;
			goto CLEAN_UP;
		}
	}

	if(md5){
		OpenSSL_add_all_digests();
		md = EVP_get_digestbyname(MD_NAME);
		if(md == NULL){
			LOG_ERROR("no such digest %s", MD_NAME);
			err = 1;
			goto CLEAN_UP;
		}
		EVP_MD_CTX_init(&mdctx);
		mdctx_init = 1;
	}
	
	rixhdr.h_count = 0;
	rixhdr.h_size = sizeof(RIDX_T);
	fwrite(&rixhdr, sizeof(HDR_T), (size_t)1, rixfp);

	for(roff = 0, lcnt = 0; (l_line = getline(&line, &s_line, fp)) > 0; ){
		char	*e_lp;
		int	fcnt;
		char	*fldp, *e_fldp;

		lcnt++;
		if(s_line > s_key){
			if(key != NULL)
				free(key);
			s_key = s_line;
			key = (char *)malloc(s_key * sizeof(char));
			if(key == NULL){
				LOG_ERROR("can't allocate key");
				err = 1;
				goto CLEAN_UP;
			}
		}

		// keys from file or in line?
		if(rd_kfile){
			if((l_key = getline(&key, &s_key, kfp)) <= 0){
				LOG_ERROR("line %7d: short key file", lcnt);
				err = 1;
				goto CLEAN_UP;
			}
			if(key[l_key - 1] == '\n'){
				key[l_key - 1] = '\0';
				l_key--;
			}
			if(l_key == 0){
				LOG_ERROR("line %7d: empty key", lcnt);
				err = 1;
				goto CLEAN_UP;
			}
		}else{
			e_lp = &line[l_line];
			if(e_lp[-1] == '\n')
				e_lp--;
			for(fcnt = 1, fldp = e_fldp = line; e_fldp < e_lp; e_fldp++){
				if(*e_fldp == '\t'){
					if(fcnt == kf)
						break;
					fldp = e_fldp + 1;
					fcnt++;
				}
			}
			if(fcnt != kf){
				LOG_ERROR("lcnt %7d: short record %d fields, need %d", lcnt, fcnt, kf);
				err = 1;
				goto CLEAN_UP;
			}
			l_key = e_fldp - fldp;
			strncpy(key, fldp, l_key);
			key[l_key] = '\0';
		}
		ridx.r_offset = roff;
		ridx.r_length = l_line;
		fwrite(&ridx, sizeof(RIDX_T), (size_t)1, rixfp);
		roff += l_line;
		if(md5){
			char	*mvp;
			int	i;

			EVP_DigestInit_ex(&mdctx, md, NULL);
			EVP_DigestUpdate(&mdctx, key, l_key);
			EVP_DigestFinal_ex(&mdctx, md_value, &md_len);

			for(mvp = md_value_str, i = 0; i < md_len; i++, mvp += 2)
				sprintf(mvp, "%02x", md_value[i] & 0xff);
			*mvp = '\0';
			fprintf(md5fp, "%s\n", md_value_str);
		}else if((l_key + 1) > KEYSIZE){
			LOG_ERROR("lcnt %7d: key is too large: %ld, limit is %d", lcnt, (l_key + 1), KEYSIZE);
			err = 1;
			goto CLEAN_UP;
		}else
			fprintf(kfp, "%s\n", key);
	}
	rixhdr.h_count = lcnt;
	rewind(rixfp);
	fwrite(&rixhdr, sizeof(HDR_T), (size_t)1, rixfp);

	printf("%s nrecs=%d\n", fname, lcnt);

CLEAN_UP : ;

	if(key != NULL)
		free(key);

	if(line != NULL)
		free(line);

	if(mdctx_init)
		EVP_MD_CTX_cleanup(&mdctx);

	if(md5fp != NULL)
		fclose(md5fp);
	if(kfp != NULL)
		fclose(kfp);
	if(rixfp != NULL)
		fclose(rixfp);
	if(fp != NULL)
		fclose(fp);

	if(md5fname != NULL)
		free(md5fname);
	if(kfname != NULL)
		free(kfname);
	if(rixfname != NULL)
		free(rixfname);
	if(bname != NULL)
		free(bname);

	return err;
}
