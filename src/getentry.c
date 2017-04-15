#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <openssl/evp.h>

#include "index.h"
#include "fmap.h"
#include "log.h"
#include "args.h"

#define	MD_NAME	"md5"

static	ARGS_T	*args;
static	FLAG_T	flags[] = {
	{"-help",   1, AVK_NONE, AVT_BOOL, "0",  "Use -help to print this message"},
	{"-fmap",   0, AVK_REQ,  AVT_STR,  NULL, "Use -fmap FM to access the data described by file map FM"},
        {"-md5",    1, AVK_NONE, AVT_BOOL, "0",  "Use -md5 convert the key entries int md5 fixed length strings"},
	{"-addq",   1, AVK_NONE, AVT_BOOL, "0",  "Use -addq to prefix each record with search key followed by \\t"},
	{"-rnum",   1, AVK_NONE, AVT_BOOL, "0",  "Use -rnum to print only the rec number of the input key.  Can not be used with -single"},
	{"-single", 1, AVK_NONE, AVT_BOOL, "0",  "Use -single to put each entry into its own file, named key.sfx, sfx = f(FM.format)"}
};
static	int	n_flags = sizeof(flags)/sizeof(flags[0]);

static	KEY2RNUM_T	*findkey(char *, int, KEY2RNUM_T []);

int
main(int argc, char *argv[])
{
	int	a_stat = AS_OK;
	const ARG_VAL_T	*a_val;
	int	u_md5 = 0;
	int	aq_opt = 0;
	int	rnum_opt = 0;
	const EVP_MD	*md = NULL;
	EVP_MD_CTX	mdctx;
	int	mdctx_init = 0;
	unsigned char	md_value[EVP_MAX_MD_SIZE];
	unsigned int	md_len;
	char	md_value_str[(2*EVP_MAX_MD_SIZE + 1)];
	char	*mvp;
	int	i;
	int	single = 0;
	char	*fmfname = NULL;
	FMAP_T	*fmap = NULL;
	char	*hdr = NULL;
	char	*trlr = NULL;
	char	*ifname = NULL;
	FILE	*ifp = NULL;
	FILE	*k2rfp = NULL;
	int	k2rfd;
	struct	stat	k2rsbuf;
	void	*map = NULL;
	HDR_T	*k2rhdr;
	KEY2RNUM_T	*atab, *k2r;
	int	n_atab;
	char	acc[20];
	char	*line = NULL;
	size_t	s_line = 0;
	ssize_t	l_line;
	FM_ENTRY_T	*l_fme, *fme;
	char	ofname[256];
	char	*o_suffix;
	HDR_T	rixhdr;
	FILE	*rixfp = NULL, *dfp = NULL, *ofp = NULL;
	int	offset;
	int	ibuf[4];
	RIDX_T	ridx;
	char	*dbuf = NULL;
	int	s_dbuf = 0;
	int	rval = 0;

	a_stat = TJM_get_args(argc, argv, n_flags, flags, 0, 1, &args);
	if(a_stat != AS_OK){
		rval = a_stat != AS_HELP_ONLY;
		goto CLEAN_UP;
	}

	a_val = TJM_get_flag_value(args, "-fmap", AVT_STR);
	fmfname = a_val->av_value.v_str;

	a_val = TJM_get_flag_value(args, "-md5", AVT_BOOL);
	u_md5 = a_val->av_value.v_int;

	a_val = TJM_get_flag_value(args, "-addq", AVT_BOOL);
	aq_opt = a_val->av_value.v_int;

	a_val = TJM_get_flag_value(args, "-single", AVT_BOOL);
	single = a_val->av_value.v_int;

	a_val = TJM_get_flag_value(args, "-rnum", AVT_BOOL);
	rnum_opt = a_val->av_value.v_int;
	if(rnum_opt && single){
		LOG_ERROR("-rnum can not be used with -single");
		TJM_print_help_msg(stderr, args);
		rval = 1;
		goto CLEAN_UP;
	}

	if(u_md5){
		OpenSSL_add_all_digests();
		md = EVP_get_digestbyname(MD_NAME);
		if(md == NULL){
			LOG_ERROR("no such digest %s", MD_NAME);
			rval = 1;
			goto CLEAN_UP;
		}
		EVP_MD_CTX_init(&mdctx);
		mdctx_init = 1;
	}

	if(args->an_files == 0)
		ifp = stdin;
	else if((ifp = fopen(args->a_files[0], "r")) == NULL){
		LOG_ERROR("can't read acc-list-file %s", args->a_files[0]);
		rval = 1;
		goto CLEAN_UP;
	}

	if((fmap = FMread_fmap(fmfname)) == NULL){
		rval = 1;
		goto CLEAN_UP;
	}

	if(fmap->f_hdr){
		if((hdr = FMread_file_to_str(fmap, fmap->f_hdr)) == NULL){
			rval = 1;
			goto CLEAN_UP;
		}
	}
	if(fmap->f_trlr){
		if((trlr = FMread_file_to_str(fmap, fmap->f_trlr)) == NULL){
			rval = 1;
			goto CLEAN_UP;
		}
	}

	if(!strcmp(fmap->f_format, "sd"))
		o_suffix = "sdf";
	else if(!strcmp(fmap->f_format, "mae"))
		o_suffix = "mae";
	else
		o_suffix = "entry";

	if((k2rfp = FMfopen(fmap->f_root, "", fmap->f_index, "r")) == NULL){
		rval = 1;
		goto CLEAN_UP;
	} 
	k2rfd = fileno(k2rfp);
	if(fstat(k2rfd, &k2rsbuf)){
		LOG_ERROR("can't stat rec index file %s", fmap->f_index);
		rval = 1;
		goto CLEAN_UP;
	}
	map = mmap(NULL, k2rsbuf.st_size, PROT_READ, MAP_SHARED, k2rfd, 0L);
	if(map == NULL){
		LOG_ERROR("can't mmap rec index file %s", fmap->f_index);
		rval = 1;
		goto CLEAN_UP;
	}
	k2rhdr = (HDR_T *)map;
	n_atab = k2rhdr->h_count;
	atab = (KEY2RNUM_T *)&((char *)map)[sizeof(HDR_T)];

	if(!single){
		ofp = stdout;
		if(hdr != NULL)
			fputs(hdr, ofp);
	}

	for(l_fme = NULL; (l_line = getline(&line, &s_line, ifp)) > 0;){
		if(*line == '#')
			continue;
		if(u_md5){
			if(line[l_line - 1] == '\n'){
				line[l_line - 1] = '\0';
				l_line--;
				if(l_line == 0)
					continue;
			}
			EVP_DigestInit_ex(&mdctx, md, NULL);
			EVP_DigestUpdate(&mdctx, line, l_line);
			EVP_DigestFinal(&mdctx, md_value, &md_len);
			for(mvp = md_value_str, i = 0; i < md_len; i++, mvp += 2)
				sprintf(mvp, "%02x", md_value[i] & 0xff);
			*mvp = '\0';
			k2r = findkey(md_value_str, n_atab, atab);
			if(k2r == NULL){
				LOG_ERROR("%s (%s) not in index %s", line, md_value_str, fmap->f_index);
				rval = 1;
				continue;
			}
		}else{
			// TODO: this was %s, so it stopped at space, add option
			if(sscanf(line, "%[^\n]", acc) != 1){
				LOG_ERROR("input format is 1 acc/line");
				rval = 1;
				goto CLEAN_UP;
			}
			k2r = findkey(acc, n_atab, atab);
			if(k2r == NULL){
				LOG_WARN("%s not in index %s", acc, fmap->f_index);
				rval = 1;
				continue;
			}
		}
		if((fme = FMrnum2fmentry(fmap, k2r->k_rnum)) == NULL){
			LOG_ERROR("NO fme for rnum %010u", k2r->k_rnum);
			rval = 1;
			goto CLEAN_UP;
		}
		if(rnum_opt){
			printf("%d", k2r->k_rnum);
			if(aq_opt)
				printf("\t%s", line);
			printf("\n");
			continue;
		}
		if(fme != l_fme){
			if(rixfp != NULL){
				fclose(rixfp);
				rixfp = NULL;
			}
			if((rixfp = FMfopen(fmap->f_root, fme->f_fname, fmap->f_ridx, "r")) == NULL){
				rval = 1;
				goto CLEAN_UP;
			}
			fread(&rixhdr, sizeof(HDR_T), 1L, rixfp);
			if(dfp != NULL){
				fclose(dfp);
				dfp = NULL;
			}
			if((dfp = FMfopen(fmap->f_root, fme->f_fname, "", "r")) == NULL){
				rval = 1;
				goto CLEAN_UP;
			}
		}

		if(single){
			sprintf(ofname, "%s.%s", k2r->k_key, o_suffix);
			if((ofp = fopen(ofname, "w")) == NULL){
				LOG_ERROR("can't write file '%s'", ofname);
				rval = 1;
				goto CLEAN_UP;
			}
			if(aq_opt)
				fprintf(ofp, "QUERY %s\n", line);
			if(hdr != NULL)
				fputs(hdr, ofp);
		}

		offset = (k2r->k_rnum - fme->f_first) * rixhdr.h_size + sizeof(HDR_T);
		fseek(rixfp, (long)offset, SEEK_SET);
		fread(&ridx, sizeof(ridx), 1L, rixfp);
		if(ridx.r_length + 1 > s_dbuf){
			if(dbuf != NULL)
				free(dbuf);
			s_dbuf = ridx.r_length + 1;
			dbuf = (char *)malloc(s_dbuf * sizeof(char));
			if(dbuf == NULL){
				LOG_ERROR("can't allocate dbuf"); 
				rval = 1;
				goto CLEAN_UP;
			}
		}
		fseek(dfp, ridx.r_offset, SEEK_SET);
		fread(dbuf, ridx.r_length, sizeof(char), dfp);
		dbuf[ridx.r_length] = '\0';
		if(aq_opt)
			fprintf(ofp, "QUERY %s\n", line);
		fwrite(dbuf, ridx.r_length, sizeof(char), ofp);
		if(single){
			fclose(ofp);
			ofp = NULL;
		}

		if(single){
			if(trlr != NULL)
				fputs(trlr, ofp);
		}

		l_fme = fme;
	}

	if(!single){
		if(trlr != NULL)
			fputs(trlr, ofp);
	}

CLEAN_UP : ;

	if(line != NULL)
		free(line);

	if(map != NULL){
		munmap(map, k2rsbuf.st_size);
		map = NULL;
	}

	if(trlr != NULL)
		free(trlr);

	if(hdr != NULL)
		free(hdr);

	if(ifp != NULL && ifp != stdin){
		fclose(ifp);
		ifp = NULL;
	}

	if(k2rfp != NULL){
		fclose(k2rfp);
		k2rfp = NULL;
	}

	if(rixfp != NULL){
		fclose(rixfp);
		rixfp = NULL;
	}

	if(dfp != NULL){
		fclose(dfp);
		dfp = NULL;
	}

	if(ofp != NULL && ofp != stdout){
		fclose(ofp);
		ofp = NULL;
	}

	if(u_md5){
		if(mdctx_init)
			EVP_MD_CTX_cleanup(&mdctx);
	}

	TJM_free_args(args);

	exit(rval);
}

static	KEY2RNUM_T	*
findkey(char *key, int n_idx, KEY2RNUM_T idx[])
{
	int	i, j, k, cv;
	KEY2RNUM_T	*k2r;

	for(i = 0, j = n_idx - 1; i <= j; ){
		k = (i + j) / 2;
		k2r = &idx[k];
		if((cv = strcmp(key, k2r->k_key)) == 0)
			return k2r;
		else if(cv < 0)
			j = k - 1;
		else
			i = k + 1;
	}
	return NULL;
}
