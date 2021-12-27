#include <string.h>

#include "config.h"

#include "cpmfs.h"
#undef CPMFS_DEBUG

int autoReadSuper(struct cpmSuperBlock *d, char const *format);

/*
 * autoReadSuper -- infer super block from disk image
 */
int autoReadSuper(struct cpmSuperBlock *d, char const *format) {
	memset(((char *)d) + sizeof(d->dev), 0, sizeof(*d) - sizeof(d->dev));
	d->skew = 1;
	d->blksiz = d->boottrk = d->secLength = d->sectrk = d->tracks = d->maxdir = -1;
	/*
	 * implement auto-detect here.
	 */
	if (0) { /* success path */
		d->size = (d->secLength * d->sectrk * (d->tracks - d->boottrk)) / d->blksiz;
		d->extents = ((d->size > 256 ? 8 : 16) * d->blksiz) / 16384;
		return 0;
	}
	return -1; /* error: not implemented, in case anyone checks */
}
