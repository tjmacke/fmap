#ifndef	__FMAP__
#define	__FMAP__

#ifndef	UNDEF
#define	UNDEF	(-1)
#endif

typedef	struct	fm_entry_t	{
	char	*f_dname;
	int	f_part;
	char	*f_fname;
	int	f_first;
	int	f_last;
	char	*f_hosts;
} FM_ENTRY_T;

typedef	struct	fmap_t	{
	char	*f_root;
	char	*f_format;
	char	*f_hdr;
	char	*f_trlr;
	char	*f_ridx;
	char	*f_fidx;
	char	*f_key;
	char	*f_index;
	int	f_nentries;
	FM_ENTRY_T	*f_entries;
} FMAP_T;

typedef	struct	afm_entry_t	{
	FMAP_T	*a_fmap;
	FM_ENTRY_T	*a_fme;
	FILE	*a_dfp;
	FILE	*a_rfp;
	FILE	*a_ffp;	/* this is not always present	*/
} AFM_ENTRY_T;

FMAP_T	*FMread_fmap(const char *);
char	*FMread_file_to_str(FMAP_T *, const char *);
int	FMupd_fmap(FILE *, FMAP_T *);
FM_ENTRY_T	*FMget_fmentry(FMAP_T *, char *, int);
FM_ENTRY_T	*FMget_fmentry1(FMAP_T *, char *, int);
FM_ENTRY_T	*FMrnum2fmentry(FMAP_T *, int);
int     FMmark_active(FMAP_T *, char *, int []);
void	FMwrite_fmap(FILE *, FMAP_T *);
int	FMev_sub(char *, char *);
FILE	*FMfopen(char *, char *, char *, char *);
AFM_ENTRY_T	*FMnew_afmentry(FMAP_T * );
int	FMuse_afmentry(AFM_ENTRY_T *, int);
void	*FMfree_fmap(FMAP_T *);

#endif
