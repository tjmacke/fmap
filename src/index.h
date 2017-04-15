#ifndef	__INDEX__
#define	__INDEX__

#define	H_RIDX		0
#define	H_RFIDX		1
#define	H_FIDX		2
#define	H_KEY2RNUM	3

typedef	struct	hdr_t	{
	size_t	h_count;
	size_t	h_size;
} HDR_T;

typedef	struct	ridx_t	{
	size_t	r_offset;
	size_t	r_length;
} RIDX_T;

typedef	struct	rfidx_t	{
	int	r_offset;
	int	r_length;
	int	r_foffset;
	int	r_flength;
} RFIDX_T;

#define	KEYSIZE	36
typedef	struct	key2rnum_t	{
	char	k_key[ KEYSIZE ];
	int	k_rnum;
} KEY2RNUM_T;

#endif
