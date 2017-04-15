#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "log.h"
#include "fmap.h"

static	const char	*fm_fmname;
static	int	fm_lcnt;	

static	char	
*trim(char *);
static	char	
*getkword(char *, char []);
static	FM_ENTRY_T	
*readfme(FILE *, int);
static	int	
updfme( FM_ENTRY_T *, int, int, char **);
static	int	
fmecmp_dp(const void *, const void *);
static	int	
fmecmp_fn(const void *, const void *);
static	FM_ENTRY_T	
*fi_fme(char *, int, FM_ENTRY_T **);
static	int	
setrange(char *, int *, int *, int);

FMAP_T	
*FMread_fmap(const char *fmname)
{
	FILE	*fp = NULL;
	int	n_fme;
	char	line[256], kword[256], value[256];
	char	*lp, *kwp, *vp;
	char	*root = NULL;
	char	*format = NULL;
	int	g_count = 0;
	int	count = UNDEF;
	int	g_files = 0;
	FMAP_T	*fmap = NULL;
	FM_ENTRY_T	*fme = NULL;
	char	*sp;
	int	err = 0;

	if(fmname == NULL){
		LOG_ERROR("fmname is NULL");
		err = 1;
		goto CLEAN_UP;
	}

	if((fp = fopen(fmname, "r")) == NULL){
		LOG_ERROR("can't read file-map %s", fmname);
		err = 1;
		goto CLEAN_UP;
	}
	fm_fmname = fmname;

	fmap = (FMAP_T *)malloc(sizeof(FMAP_T));
	if(fmap == NULL){
		LOG_ERROR("can't allocate fmap");
		err = 1;
		goto CLEAN_UP;
	}
	fmap->f_root = NULL;
	fmap->f_format = NULL;
	fmap->f_hdr = NULL;
	fmap->f_trlr = NULL;
	fmap->f_ridx = NULL;
	fmap->f_fidx = NULL;
	fmap->f_key = NULL;
	fmap->f_index = NULL;
	fmap->f_nentries = UNDEF;
	fmap->f_entries = NULL;

	for(fm_lcnt = 0, n_fme = 0; fgets(line, sizeof(line), fp); ){
		fm_lcnt++;
		lp = trim(line);
		if(*lp == '#' || *lp == '\0')
			continue;

		if((lp = getkword(lp, kword)) == NULL){
			err = 1;
			goto CLEAN_UP;
		}

		lp = trim(lp);
		if(*lp != '='){
			LOG_ERROR("%s:%d: '=' expected", fm_fmname, fm_lcnt);
			err = 1;
			goto CLEAN_UP;
		}else
			lp++;

		lp = trim(lp);
		if(*lp == '\0'){
			LOG_ERROR("%s:%d: SYNTAX: keyword = value", fm_fmname, fm_lcnt);
			err = 1;
			goto CLEAN_UP;
		}
		for(vp = value; *lp != '\n' && *lp; lp++)
			*vp++ = *lp;
		*vp = '\0';

		if(!strcmp(kword, "root")){
			if(fmap->f_root != NULL){
				LOG_ERROR("%s:%d: only one root stmt allowed", fm_fmname, fm_lcnt);
				err = 1;
				goto CLEAN_UP;
			}
			fmap->f_root = strdup(value);
			if(fmap->f_root == NULL){
				LOG_ERROR("%s:%d: can't strdup root", fm_fmname, fm_lcnt);
				err = 1;
				goto CLEAN_UP;
			}
		}else if(!strcmp(kword, "format")){
			if(fmap->f_format != NULL){
				LOG_ERROR("%s:%d: only one format stmt allowed", fm_fmname, fm_lcnt);
				err = 1;
				goto CLEAN_UP;
			}
			fmap->f_format = strdup(value);
			if(fmap->f_format == NULL){
				LOG_ERROR("%s:%d: can't strdup format", fm_fmname, fm_lcnt);
				err = 1;
				goto CLEAN_UP;
			}
		}else if(!strcmp(kword, "hdr")){
			if(fmap->f_hdr != NULL){
				LOG_ERROR("%s:%d: only one hdr stmt allowed", fm_fmname, fm_lcnt);
				err = 1;
				goto CLEAN_UP;
			}
			fmap->f_hdr = strdup(value);
			if(fmap->f_hdr == NULL){
				LOG_ERROR("%s:%d: can't strdup hdr", fm_fmname, fm_lcnt);
				err = 1;
				goto CLEAN_UP;
			}
		}else if(!strcmp(kword, "trlr")){
			if(fmap->f_trlr != NULL){
				LOG_ERROR("%s:%d: only one trlr stmt allowed", fm_fmname, fm_lcnt);
				err = 1;
				goto CLEAN_UP;
			}
			fmap->f_trlr = strdup(value);
			if(fmap->f_trlr == NULL){
				LOG_ERROR("%s:%d: can't strdup trlr", fm_fmname, fm_lcnt);
				err = 1;
				goto CLEAN_UP;
			}
		}else if(!strcmp(kword, "ridx")){
			if(fmap->f_ridx != NULL){
				LOG_ERROR("%s:%d: only one ridx stmt allowed", fm_fmname, fm_lcnt);
				err = 1;
				goto CLEAN_UP;
			}
			fmap->f_ridx = strdup(value);
			if(fmap->f_ridx == NULL){
				LOG_ERROR("%s:%d: can't strdup ridx", fm_fmname, fm_lcnt);
				err = 1;
				goto CLEAN_UP;
			}
		}else if(!strcmp(kword, "fidx")){
			if(fmap->f_fidx != NULL){
				LOG_ERROR("%s:%d: only one ridx stmt allowed", fm_fmname, fm_lcnt);
				err = 1;
				goto CLEAN_UP;
			}
			fmap->f_fidx = strdup(value);
			if(fmap->f_fidx == NULL){
				LOG_ERROR("%s:%d: can't strdup fidx", fm_fmname, fm_lcnt);
				err = 1;
				goto CLEAN_UP;
			}
		}else if(!strcmp(kword, "key")){
			if(fmap->f_key != NULL){
				LOG_ERROR("%s:%d: only one key stmt allowed", fm_fmname, fm_lcnt);
				err = 1;
				goto CLEAN_UP;
			}
			fmap->f_key = strdup(value);
			if(fmap->f_key == NULL){
				LOG_ERROR("%s:%d: can't strdup key", fm_fmname, fm_lcnt);
				err = 1;
				goto CLEAN_UP;
			}
		}else if(!strcmp(kword, "index")){
			if(fmap->f_index != NULL){
				LOG_ERROR("%s:%d: only one index stmt allowed", fm_fmname, fm_lcnt);
				err = 1;
				goto CLEAN_UP;
			}
			fmap->f_index = strdup(value);
			if(fmap->f_index == NULL){
				LOG_ERROR("%s:%d: can't strdup index", fm_fmname, fm_lcnt);
				err = 1;
				goto CLEAN_UP;
			}
		}else if(!strcmp(kword, "count")){
			if(g_count){
				LOG_ERROR("%s:%d: only one count stmt allowed", fm_fmname, fm_lcnt);
				err = 1;
				goto CLEAN_UP;
			}
			g_count = 1;
			count = atoi(value);
			if(count < 1){
				LOG_ERROR("%s:%d: count must be > 0", fm_fmname, fm_lcnt);
				err = 1;
				goto CLEAN_UP;
			}
			fmap->f_nentries = count;
		}else if(!strcmp(kword, "files")){
			if(strcmp(value, "{")){
				LOG_ERROR("%s:%d: SYNTAX: files = {", fm_fmname, fm_lcnt);
				err = 1;
				goto CLEAN_UP;
			}
			if(fmap->f_entries != NULL){
				LOG_ERROR("%s:%d: only one files stmt allowed", fm_fmname, fm_lcnt);
				err = 1;
				goto CLEAN_UP;
			}
			if(count == UNDEF){
				LOG_ERROR("%s:%d: count stmt must preced files stmt", fm_fmname, fm_lcnt);
				err = 1;
				goto CLEAN_UP;
			}
			fmap->f_entries = readfme(fp, count);
			if(fmap->f_entries == NULL){
				err = 1;
				goto CLEAN_UP;
			}
		}else{
			LOG_ERROR("%s:%d: unknown keyword '%s'", fm_fmname, fm_lcnt, kword);
			err = 1;
			goto CLEAN_UP;
		}
	}

CLEAN_UP : ;

	if(fp != NULL){
		fclose(fp);
		fp = NULL;
	}

	if(err && fmap != NULL){
		fmap = FMfree_fmap(fmap);
		fmap = NULL;
	}

	return fmap;
}

char	*
FMread_file_to_str(FMAP_T *fmap, const char *fname)
{
	FILE	*fp = NULL;
	struct stat	sbuf;
	char	*str = NULL;
	int	err = 0;

	if((fp = FMfopen(fmap->f_root, fname, "", "r")) == NULL){
		err = 1;
		goto CLEAN_UP;
	}
	if(fstat(fileno(fp), &sbuf)){
		LOG_ERROR("can't stat %s", fname);
		err = 1;
		goto CLEAN_UP;
	}
	str = (char *)malloc((sbuf.st_size + 1) * sizeof(char));
	if(str == NULL){
		LOG_ERROR("can't allocate str for %s", fname);
		err = 1;
		goto CLEAN_UP;
	}
	fread(str, sbuf.st_size, 1L, fp);
	str[sbuf.st_size] = '\0';

CLEAN_UP : ;

	if(fp != NULL)
		fclose(fp);

	if(err){
		if(str != NULL)
			free(str);
		str = NULL;
	}

	return str;
}

FM_ENTRY_T	*
FMget_fmentry(FMAP_T *fmap, char *dname, int part)
{
	int	i, j, k, cv;
	FM_ENTRY_T	*fme;

	for(i = 0, j = fmap->f_nentries - 1; i <= j; ){
		k = (i + j) / 2;
		fme = &fmap->f_entries[k];
		if((cv = strcmp(fme->f_dname, dname)) == 0){
			if(part != UNDEF){
				cv = fme->f_part - part;
				return(&fmap->f_entries[k - cv]);
			}else{
				for( ; k > 0; k--){
					fme = &fmap->f_entries[k - 1];
					if(strcmp(fme->f_dname, dname))
						return &fmap->f_entries[k];
				}
				return  &fmap->f_entries[0];
			}
		}else if(cv < 0)
			i = k + 1;
		else
			j = k - 1;
	}
	return  NULL;
}

FM_ENTRY_T	*
FMget_fmentry1(FMAP_T *fmap, char *dname, int part)
{
	int	i, j, k, cv;
	FM_ENTRY_T	*fme1, fme2;

	fme2.f_dname = dname;
	fme2.f_part = part;
	for(i = 0, j = fmap->f_nentries - 1; i <= j; ){
		k = (i + j) / 2;
		fme1 = &fmap->f_entries[k];
		if((cv = fmecmp_dp(fme1, &fme2)) == 0)
			return  fme1;
		else if(cv < 0)
			i = k + 1;
		else
			j = k - 1;
	}
	return  NULL;
}

FM_ENTRY_T	*
FMrnum2fmentry(FMAP_T *fmap, int rnum)
{
	int	i, j, k;
	FM_ENTRY_T	*fme;

	for(i = 0, j = fmap->f_nentries - 1; i <= j; ){
		k = (i + j) / 2;
		fme = &fmap->f_entries[k];
		if(rnum < fme->f_first)
			j = k - 1;
		else if(rnum > fme->f_last)
			i = k + 1;
		else
			return  fme;
	}
	return  NULL;
}

int	
FMmark_active(FMAP_T *fmap, char *dbname, int active[])
{
	char	*dp, *pp, *pc;
	char	dname[32];
	char	range[32];
	int	f, pcnt, p, p1;
	int	pl, ph;
	FM_ENTRY_T	*fme;

	dp = dbname;
	if(pp = strchr(dp, ':')){
		strncpy(dname, dp, pp - dp);
		dname[pp - dp] = '\0';
	}else
		strcpy(dname, dp);

	if(!strcmp(dname, "all")){
		for(f = 0; f < fmap->f_nentries; f++)
			active[f] = 1;
		return  0;
	}

	if((fme = FMget_fmentry(fmap, dname, 1)) == NULL){
		LOG_ERROR("unknown db '%s'", dp);
		return 1;
	}

	for(pcnt = 0, p1 = p = fme - fmap->f_entries; ; ){
		pcnt++;
		if(p == fmap->f_nentries - 1)
			break;
		p++;
		fme++;
		if(strcmp(fme->f_dname, dname))
			break;
	}

	if(pp == NULL || pp[1] == '\0'){
		for(p = 0; p < pcnt; p++)
			active[p1 + p] = 1;
		return  0;
	}

	for(++pp; pp && *pp; ){
		if(pc = strchr(pp, ',')){
			strncpy(range, pp, pc - pp);
			range[pc - pp] = '\0';
			pp = pc + 1;
		}else{
			strcpy(range, pp);
			pp = pc;
		}
		if(setrange(range, &pl, &ph, pcnt))
			return  1;
		for(p = pl - 1; p < ph; p++)
			active[p1 + p] = 1;
	}

	return  0;
}

int	
FMupd_fmap(FILE *dfp, FMAP_T *fmap)
{
	FM_ENTRY_T	**fmidx = NULL, *fme;
	int	n_fmidx;
	int	nf, cf, f;
	int	lcnt;
	char	line[256];
	char	*fields[10];
	int	ok = 1;

	n_fmidx = fmap->f_nentries;
	fmidx = (FM_ENTRY_T **)calloc((long)n_fmidx, sizeof(FM_ENTRY_T *));
	if(fmidx == NULL){
		LOG_ERROR("can't allocate fmidx");
		ok = 0;
		goto CLEAN_UP;
	}
	for(f = 0; f < n_fmidx; f++)
		fmidx[f] = &fmap->f_entries[f];
	qsort(fmidx, n_fmidx, sizeof(FM_ENTRY_T *), fmecmp_fn);

	for(lcnt = 0; fgets(line, sizeof(line), dfp); ){
		lcnt++;
		nf = split(line, fields, " \t\n");
		if(nf == 0)
			continue;
		for(cf = nf, f = 0; f < nf; f++){
			if(*fields[f] == '#'){
				cf = f;
				break;
			}
		}
		if(cf == 1){
			LOG_ERROR("line %5d: SYNTAX: file-name data1 ...", lcnt);
			ok = 0;
		}else if(cf > 1){
			fme = fi_fme(fields[0], n_fmidx, fmidx);
			if(fme == NULL){
				LOG_ERROR("file %s in not in filemap", fields[0]);
				ok = 0;
			}else if(!updfme(fme, 1, cf, fields))
				ok = 0;
		}
		for(f = 0; f < nf; f++)
			free(fields[f]);
	}

CLEAN_UP : ;

	if(fmidx != NULL){
		free(fmidx);
	}

	return  ok;
}

void	
FMwrite_fmap(FILE *fp, FMAP_T *fmap)
{
	int	f;
	FM_ENTRY_T	*fme;

	fprintf(fp, "root = %s\n", fmap->f_root);
	fprintf(fp, "format = %s\n", fmap->f_format);
	if(fmap->f_hdr != NULL)
		fprintf(fp, "hdr = %s\n", fmap->f_hdr);
	if(fmap->f_trlr != NULL)
		fprintf(fp, "trlr = %s\n", fmap->f_trlr);
	if(fmap->f_ridx != NULL)
		fprintf(fp, "ridx = %s\n", fmap->f_ridx);
	if(fmap->f_fidx != NULL)
		fprintf(fp, "fidx = %s\n", fmap->f_fidx);
	if(fmap->f_key != NULL)
		fprintf(fp, "key = %s\n", fmap->f_key);
	if(fmap->f_index != NULL)
		fprintf(fp, "index = %s\n", fmap->f_index);
	fprintf(fp, "count = %d\n", fmap->f_nentries);
	fprintf(fp, "files = {\n");
	for(fme = fmap->f_entries, f = 0; f < fmap->f_nentries; f++, fme++){
		fprintf(fp, "\t%-4s %3d %s",
			fme->f_dname, fme->f_part, fme->f_fname);
		if(fme->f_last != UNDEF){
			fprintf(fp, " nrecs=");
			if(fme->f_first != UNDEF)
				fprintf(fp, "%d", fme->f_last-fme->f_first+1);
			else
				fprintf(fp, "%d", fme->f_last);
		}
		if(fme->f_hosts)
			fprintf(fp, " hosts=%s", fme->f_hosts);
		fprintf(fp, "\n");
	}
	fprintf(fp, "}\n");
}

int	
FMev_sub(char *fname, char *xfname)
{
	char	*sp, *e_sp;
	char	symbol[256];
	int	rval = 0;

	if(*fname != '$')
		strcpy(xfname, fname);
	else{
		sp = &fname[1];
		if(e_sp = strchr(sp, '/')){
			strncpy(symbol, sp, e_sp - sp);
			symbol[e_sp - sp] = '\0';
		}else
			strcpy(symbol, sp);
		if(sp = getenv(symbol))
			sprintf(xfname, "%s%s", sp, e_sp);
		else{
			LOG_ERROR("symbol '%s' no defined", symbol);
			rval = 1;
		}
	}

CLEAN_UP : ;

	return  rval;
}

FILE	*
FMfopen(char *root, char *fname, char *ext, char *mode)
{
	FILE	*fp = NULL;
	char	*fnp, *dp, *rp, *wp1, *wp2;
	char	wfname1[256], wfname2[256], xfname[256];

	if(fname == NULL)
		strcpy(wfname1, "");
	else
		strcpy(wfname1, fname);

	if(ext != NULL && *ext != '\0'){
		if(*wfname1 == '\0')
			strcpy(wfname1, ext);
		else{
			for(dp = NULL, fnp = fname; *fnp; fnp++){
				if(*fnp == '.')
					dp = fnp;
			}
			if(dp == NULL){
				strcpy(wfname1, fname);
				strcat(wfname1, ".");
				strcat(wfname1, ext );
			}else{
				dp++;
				strncpy(wfname1, fname, dp - fname);
				strcpy(&wfname1[dp - fname], ext);
			}
		}
	}

	if(*wfname1 == '$')
		FMev_sub(wfname1, xfname);
	else if(!strcmp(wfname1, "./") || !strcmp(wfname1, "../") ||
		 *wfname1 == '/')
	{
		strcpy(xfname, wfname1);
	}else if(root == NULL || *root == '\0')
		strcpy(xfname, wfname1);
	else{
		for(wp2 = wfname2, rp = root; *rp; rp++)
			*wp2++ = *rp;
		*wp2++ = '/';
		for(wp1 = wfname1; *wp1; wp1++)
			*wp2++ = *wp1;
		*wp2 = '\0';
		if(*wfname2 == '$')
			FMev_sub(wfname2, xfname);
		else
			strcpy(xfname, wfname2);
	}

	if((fp = fopen(xfname, mode)) == NULL){
		LOG_ERROR("can't open file '%s' w/mode '%s'", xfname, mode);
	}

	return  fp;
}

AFM_ENTRY_T	*
FMnew_afmentry(FMAP_T *fmap)
{
	AFM_ENTRY_T	*afme = NULL;

	afme = (AFM_ENTRY_T *)calloc(1L, sizeof(AFM_ENTRY_T));
	if(afme == NULL){
		LOG_ERROR("can't allocate afme");
	}else
		afme->a_fmap = fmap;
	return  afme;
}

int	
FMuse_afmentry(AFM_ENTRY_T *afme, int rnum)
{
	int	err = 0;
	FMAP_T	*fmap;
	FM_ENTRY_T	*fme;

	if(afme->a_fme == NULL ||
		rnum < afme->a_fme->f_first ||
		rnum > afme->a_fme->f_last)
	{
		if(afme->a_dfp != NULL)
			fclose(afme->a_dfp); 
		afme->a_dfp = NULL;
		if(afme->a_rfp != NULL)
			fclose(afme->a_rfp); 
		afme->a_rfp = NULL;
		if(afme->a_ffp != NULL)
			fclose(afme->a_ffp); 
		afme->a_ffp = NULL;
		afme->a_fme = fme = FMrnum2fmentry(afme->a_fmap, rnum);
		if(afme->a_fme == NULL){
			LOG_ERROR("rnum %d not in fmap", rnum);
			err = 1;
			goto CLEAN_UP;
		}
		fmap = afme->a_fmap;
		afme->a_dfp = FMfopen(fmap->f_root, fme->f_fname, "", "r");
		if(afme->a_dfp == NULL){
			err = 1;
			goto CLEAN_UP;
		}
		afme->a_rfp = FMfopen(fmap->f_root, fme->f_fname,
			fmap->f_ridx, "r");
		if(afme->a_rfp == NULL){
			err = 1;
			goto CLEAN_UP;
		}
		if(fmap->f_fidx != NULL){
			afme->a_ffp = FMfopen(fmap->f_root, fme->f_fname,
				fmap->f_fidx, "r");
			if(afme->a_ffp == NULL){
				err = 1;
				goto CLEAN_UP;
			}
		}
	}

CLEAN_UP : ;

	if(err){
		afme->a_fme = NULL;
		if(afme->a_dfp != NULL)
			fclose(afme->a_dfp);
		afme->a_dfp = NULL;
		if(afme->a_rfp != NULL)
			fclose(afme->a_rfp);
		afme->a_rfp = NULL;
		if(afme->a_ffp != NULL)
			fclose(afme->a_ffp);
		afme->a_ffp = NULL;
	}

	return  err;
}

void	*
FMfree_fmap(FMAP_T *fmap)	
{
	FM_ENTRY_T	*fme;
	int	f;

	if(fmap != NULL){
		if(fmap->f_root != NULL)
			free(fmap->f_root);
		if(fmap->f_format != NULL)
			free(fmap->f_format);
		if(fmap->f_hdr != NULL)
			free(fmap->f_hdr);
		if(fmap->f_trlr != NULL)
			free(fmap->f_trlr);
		if(fmap->f_ridx != NULL)
			free(fmap->f_ridx);
		if(fmap->f_fidx != NULL)
			free(fmap->f_fidx);
		if(fmap->f_fidx != NULL)
			free(fmap->f_fidx);
		if(fmap->f_key != NULL)
			free(fmap->f_key);
		if(fmap->f_index != NULL)
			free(fmap->f_index);
		if((fme = fmap->f_entries) != NULL){
			for(f = 0; f < fmap->f_nentries; f++, fme++){
				if(fme->f_dname != NULL)
					free(fme->f_dname);
				if(fme->f_fname != NULL)
					free(fme->f_fname);
				if(fme->f_hosts != NULL)
					free(fme->f_hosts);
			}
			free(fmap->f_entries);
		}
		free(fmap);
		fmap = NULL;
	}
	return  NULL;
}

static	char	*
trim(char *str)
{
	char	*sp;

	if(str == NULL)
		return  NULL;
	for(sp = str; isspace(*sp); sp++)
		;
	return  sp;
}

static	char	*
getkword(char *str, char kword[])
{
	char	*sp, *kwp;

	sp = str;
	if(isalpha(*sp)){
		kwp = kword;
		for(*kwp++ = *sp++; isalnum(*sp); sp++)
			*kwp++ = *sp;
		*kwp = '\0';
	}else{
		LOG_ERROR("bad keyword: %s: must begin with a letter", sp);
		sp = NULL;
	}
	return  sp;
}

static	FM_ENTRY_T	*
readfme(FILE *fp, int count)
{
	FM_ENTRY_T	*fme = NULL, *fme1, *fme2;
	char	line[256];
	int	c, cf, nf, f;
	char	*fields[20];
	int	nrecs, done, err = 0;

	fme = (FM_ENTRY_T *)calloc((long)count, sizeof(FM_ENTRY_T));
	if(fme == NULL){
		LOG_ERROR("can't allocate fme");
		goto CLEAN_UP;
	}

	for(done = 0, fme1 = fme, c = 0; fgets(line, sizeof(line), fp); ){
		fm_lcnt++;
		nf = split(line, fields, " \t\n");
		if(nf == 0)
			continue;
		for(cf = nf, f = 0; f < nf; f++){
			if(*fields[f] == '#'){
				cf = f;
				break;
			}
		}
		if(cf == 1 || cf == 2){
			if(cf == 1 && !strcmp(fields[0], "}"))
				done = 1;
			else{
				LOG_ERROR("bad fme %s: SYNTAX: name part filename [ options ]", line);
				err = 1;
				goto FREE;
			}
		}else if (cf >= 3){
			if(c < count){
				fme1->f_dname = fields[0];
				fields[0] = NULL;
				fme1->f_part = atoi(fields[1]);
				fme1->f_fname = fields[2];
				fields[2] = NULL;
				fme1->f_first = UNDEF;
				fme1->f_last = UNDEF;
				fme1->f_hosts = NULL;
				if(cf >= 4){
					updfme(fme1, 3, cf, fields); 
				}
				fme1++;
			}
			c++;
		}

	FREE : ;

		for(f = 0; f < nf; f++){
			if(fields[f] != NULL)
				free(fields[f]);
		}
		if(done || err)
			break;
	}
	if(!done){
		LOG_ERROR("%s: file block missing closing } line", fm_fmname);
		err = 1;
		goto CLEAN_UP;
	}

	if(err)
		goto CLEAN_UP;

	if(c != count){
		LOG_ERROR("%s: fmap has %d entries, expecting %d", fm_fmname, c, count);
		err = 1;
		goto CLEAN_UP;
	}

	qsort(fme, count, sizeof(FM_ENTRY_T), fmecmp_dp);
	fme1 = fme;
	for(fme1 = fme, c = 1; c < count; c++, fme1++){
		fme2 = fme1 + 1;
		if(!fmecmp_dp(fme1, fme2)){
			LOG_ERROR("%s: fmap has duplicate entries: %s:%d, %s:%d",
				fm_fmname, fme1->f_dname, fme1->f_part, fme2->f_dname, fme2->f_part);
			err = 1;
			goto CLEAN_UP;
		}
	}

	for(nrecs = 1, fme1 = fme, c = 0; c < count; c++, fme1++){
		if(fme1->f_last == UNDEF){
			nrecs = 0;
			break;
		}
	}
	if(nrecs){
		for(nrecs = 0, fme1 = fme, c = 0; c < count; c++, fme1++){
			fme1->f_first = nrecs;
			nrecs += fme1->f_last;
			fme1->f_last = nrecs - 1;
		}
	}

/*
{
	for( fme1 = fme, c = 0; c < count; c++, fme1++ ){
		fprintf( stderr, "fme[%2d] = %-3s %2d %-15s %7d %7d %s\n",
			c, fme1->f_dname, fme1->f_part, fme1->f_fname,
			fme1->f_first, fme1->f_last,
			fme1->f_hosts ? fme1->f_hosts : "ALL" );
	}
}
*/

CLEAN_UP : ;

	if(err && fme != NULL){
		for(fme1 = fme, f = 0; f < count; f++, fme1++){
			if(fme->f_dname != NULL)
				free(fme->f_dname);
			if(fme->f_fname != NULL)
				free(fme->f_fname);
			if(fme->f_hosts != NULL)
				free(fme->f_hosts);
		}
		free(fme);
		fme = NULL;
	}

	return  fme;
}

static	int	
updfme(FM_ENTRY_T *fme, int f1, int nf, char **fields)
{
	int	f;
	char	*eq, name[256], value[256];
	int	ival;
	int	ok = 1;

	for(f = f1; f < nf; f++){
		if(eq = strchr(fields[f], '=')){
			strncpy(name, fields[f], eq - fields[f]);
			name[eq - fields[f]] = '\0';
			strcpy(value, &eq[1]);
		}else{
			strcpy(name, "hosts");
			strcpy(value, fields[f]);
		}

		if(!strcmp(name, "nrecs")){
			ival = atoi(value);
			if(ival < 1){
				LOG_ERROR("nrecs most be > 0, file: %s", fme->f_fname);
				ok = 0;
			}else
				fme->f_last = ival;
		}else if(!strcmp(name, "hosts")){
			if(fme->f_hosts != NULL)
				free(fme->f_hosts);
			fme->f_hosts = strdup(value);
			if(fme->f_hosts == NULL){
				LOG_ERROR("can't strdup hosts, file: %s", fme->f_fname);
				ok = 0;
			}
		}else{
			LOG_ERROR("uknown field %s, file: %s", name, fme->f_fname);
			ok = 0;
		}
	}
	return  ok;
}

static	int	
fmecmp_dp(const void *f1, const void *f2)
{
	int	cv;

	if((cv = strcmp(((FM_ENTRY_T *)f1)->f_dname, ((FM_ENTRY_T *)f2)->f_dname)) != 0)
		return  cv;
	else
		return ((FM_ENTRY_T *)f1)->f_part - ((FM_ENTRY_T *)f2)->f_part;
}

static	int	
fmecmp_fn(const void *f1, const void *f2)
{

	return strcmp((*((FM_ENTRY_T **)f1))->f_fname, (*((FM_ENTRY_T **)f2))->f_fname);
}

static	FM_ENTRY_T	*
fi_fme(char *fname, int n_fmidx, FM_ENTRY_T **fmidx)
{
	int	i, j, k, cv;
	FM_ENTRY_T	*fme;

	for(i = 0, j = n_fmidx - 1; i <= j; ){
		k = (i + j) / 2;
		fme = fmidx[k];
		if((cv = strcmp(fme->f_fname, fname)) == 0)
			return  fme;
		else if(cv < 0)
			i = k + 1;
		else
			j = k - 1;
	}
	return  NULL;
}

static	int	
setrange(char *range, int *pl, int *ph, int pcnt)
{
	char	*rp;
	int	rng;

	rng = 0;
	rp = range;
	if(!isdigit(*rp) && *rp != '-'){
		LOG_ERROR("SYNXTAX: N, N-M, N-, -M, -");
		return  1;
	}
	for(*pl = 0; isdigit(*rp); rp++)
		*pl = 10 * *pl + *rp - '0';
	if(*pl == 0)
		*pl = 1;
	if(*rp == '-'){
		rp++;
		rng = 1;
	}
	for(*ph = 0; isdigit(*rp); rp++)
		*ph = 10 * *ph + *rp - '0';
	if(*rp != '\0'){
		LOG_ERROR("SYNTAX: extra chars: %s", rp);
		return  1;
	}
	if(*ph == 0)
		*ph = rng ? pcnt : *pl;
	if(*ph > pcnt)
		*ph = pcnt;
	if(*ph < *pl){
		LOG_ERROR("low (%d) > hi (%d)", *pl, *ph);
		return  1;
	}

	return  0;
}
