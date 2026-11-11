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
_DEFUN(__librt_truncate_r,(r,path,len),
	   struct _reent *r _AND
	   const char *path _AND
	   off_t len)
{
	char resolved[PATH_MAX + 1];
	const char *lv2path = __full_path(path, resolved);
	if (!lv2path) {
		r->_errno = EINVAL;
		return -1;
	}
	return lv2errno_r(r,sysLv2FsTruncate(lv2path,len));
}

int
_DEFUN(__librt_ftruncate_r,(r,fd,len),
	   struct _reent *r _AND
	   int fd _AND
	   off_t len)
{
	return lv2errno_r(r,sysLv2FsFtruncate(fd,len));
}
