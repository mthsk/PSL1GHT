#include <stdio.h>
#include <fcntl.h>
#include <_ansi.h>
#include <_syslist.h>
#include <sys/reent.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/syslimits.h>
#include <sys/lv2errno.h>

#include <sys/file.h>

extern char* __full_path(const char *path, char *resolved_path);

int
_DEFUN(__librt_link_r,(r,old,new),
	   struct _reent *r _AND
	   const char *old _AND
	   const char *new)
{
	char resolved_old[PATH_MAX + 1];
	char resolved_new[PATH_MAX + 1];
	const char *lv2old = __full_path(old, resolved_old);
	const char *lv2new = __full_path(new, resolved_new);
	if (!lv2old || !lv2new) {
		r->_errno = EINVAL;
		return -1;
	}
	return lv2errno_r(r,sysLv2FsLink(lv2old,lv2new));
}
