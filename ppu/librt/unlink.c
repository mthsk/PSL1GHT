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
_DEFUN(__librt_unlink_r,(r,path),
	   struct _reent *r _AND
	   const char *path)
{
	char resolved[PATH_MAX + 1];
	const char *lv2path = __full_path(path, resolved);
	if (!lv2path) {
		r->_errno = EINVAL;
		return -1;
	}
	return lv2errno_r(r,sysLv2FsUnlink(lv2path));
}
