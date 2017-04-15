#ifndef	_MFILE_UTIL_H_
#define	_MFILE_UTIL_H_

typedef	struct	mfile_t	{
	char	*m_fname;
	FILE	*m_fp;
	size_t	ms_file;
	void	*m_map;
	void	*m_tab;
	int	mn_tab;
} MFILE_T;

#define	MFILE_GET_PELT(t,f,i)	(&((t)((f)->m_tab))[i])
#define	MFILE_GET_ELT(t,f,i)	(((t)((f)->m_tab))[i])
#define	MFILE_GET_NELTS(f)	((f)->mn_tab)
#define	MFILE_GET_TAB(t,f)	((t)((f)->m_tab))

#ifdef	__cplusplus
extern "C" {
#endif

MFILE_T	*
mfile_new(const char *, const char *);

int
mfile_open(MFILE_T *, const char *, int);

void
mfile_delete(MFILE_T *);

#ifdef	__cplusplus
}
#endif

#endif
