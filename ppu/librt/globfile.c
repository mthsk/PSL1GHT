#include <_ansi.h>
#include <_syslist.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reent.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/syslimits.h>
#include <sys/lv2errno.h>

#include <sys/file.h>

char *__cwd = NULL;

static void __glob_file_init(void)  __attribute__((constructor(105)));
static void __glob_file_init(void)
{
    __cwd = strdup("/");
}

static char* deletePathFromPath(char *dirname, int path_count)
{
    int i;

    if (dirname == NULL)
        return NULL;

    i = strlen(dirname);
    while (i >= 0) {
        if (dirname[i] == '/') {
            if (--path_count <= 0)
                break;
        }
        i--;
    }

    if (i > 0)
        dirname[i + 1] = 0;
    else
        strcpy(dirname, "");

    return dirname;
}

static char* flattenDirectory(const char *dirname, const char *root)
{
    char *dir = NULL;
    int len, last_pos;
    int was_real_dir;
    char fpath[PATH_MAX + 1];
    fpath[0] = 0;

    if (dirname == NULL)
        return NULL;

    strncat(fpath, dirname, PATH_MAX);

    len = strlen(dirname);
    if (dirname[len - 1] != '/') {
        fpath[len++] = '/';
        fpath[len] = 0;
    }

    last_pos = 0;
    was_real_dir = 0;
    while (1) {
        char *subdir;
        int pos = last_pos;
        while (pos < len && fpath[pos] != '/')
            pos++;
        if (pos >= len)
            break;

        subdir = &fpath[last_pos];

        if (subdir[0] == '.' && subdir[1] == '.' && subdir[2] == '/') {
            if (was_real_dir) {
                dir = deletePathFromPath(dir, 2);
                was_real_dir = (dir != NULL && strlen(dir) > 0);
            } else {
                int copy_len = (pos - last_pos + 1);
                dir = realloc(dir, (dir != NULL ? strlen(dir) : 0) + copy_len);
                strncat(dir, subdir, copy_len);
                was_real_dir = 0;
            }
        } else if (subdir[0] == '/') {
            dir = strdup(root);
        } else if (!(subdir[0] == '.' && subdir[1] == '/')) {
            int copy_len = (pos - last_pos + 1);
            dir = realloc(dir, (dir != NULL ? strlen(dir) : 0) + copy_len);
            strncat(dir, subdir, copy_len);
            was_real_dir = 1;
        }

        fpath[pos] = 0;
        last_pos = pos + 1;
    }

    return dir;
}

static int resolve_path(char *path, char *result, char *pos)
{
    if (*path == '/') {
        *result = '/';
        pos = result + 1;
        ++path;
    }

    *pos = 0;
    if (!*path)
        return 0;

    while (1) {
        char *slash = *path ? strchr(path, '/') : NULL;
        if (slash)
            *slash = 0;
        
        if (!path[0] || (path[0] == '.' && (!path[1] || (path[1] == '.' && !path[2])))) {
            --pos;
            if (pos != result && path[0] && path[1])
                while (*--pos != '/');
        } else {
            strcpy(pos, path);
            pos = strchr(result, 0);
        }

        if (slash) {
            *pos++ = '/';
            path = slash + 1;
        }

        *pos = 0;
        if (!slash)
            break;
    }

    return 0;
}

// Resolve an LV2 path against PSL1GHT's emulated current directory.
// LV2 sys_fs calls themselves do not understand process-relative paths, so
// every libc wrapper that accepts a pathname must hand LV2 an absolute path.
char* __full_path(const char *path, char *resolved_path)
{
    char combined[PATH_MAX + 1];
    size_t cwd_len, path_len, n, out;
    const char *p;

    if (path == NULL || path[0] == 0 || resolved_path == NULL || __cwd == NULL)
        return NULL;

    path_len = strlen(path);
    if (path[0] == '/') {
        if (path_len > PATH_MAX)
            return NULL;
        memcpy(combined, path, path_len + 1);
    } else {
        cwd_len = strlen(__cwd);
        if (cwd_len + (cwd_len > 1 && __cwd[cwd_len - 1] != '/' ? 1 : 0) + path_len > PATH_MAX)
            return NULL;

        memcpy(combined, __cwd, cwd_len);
        n = cwd_len;
        if (n == 0 || combined[n - 1] != '/')
            combined[n++] = '/';
        memcpy(combined + n, path, path_len + 1);
    }

    // Canonicalize repeated '/', '.', and '..' without touching the filesystem.
    out = 0;
    resolved_path[out++] = '/';
    p = combined;
    while (*p == '/')
        ++p;

    while (*p) {
        const char *component = p;
        size_t len;

        while (*p && *p != '/')
            ++p;
        len = (size_t)(p - component);
        while (*p == '/')
            ++p;

        if (len == 0 || (len == 1 && component[0] == '.'))
            continue;

        if (len == 2 && component[0] == '.' && component[1] == '.') {
            if (out > 1) {
                if (resolved_path[out - 1] == '/')
                    --out;
                while (out > 1 && resolved_path[out - 1] != '/')
                    --out;
            }
            resolved_path[out] = 0;
            continue;
        }

        if (out > 1 && resolved_path[out - 1] != '/') {
            if (out >= PATH_MAX)
                return NULL;
            resolved_path[out++] = '/';
        }
        if (out + len > PATH_MAX)
            return NULL;
        memcpy(resolved_path + out, component, len);
        out += len;
        resolved_path[out] = 0;
    }

    if (out == 0) {
        resolved_path[0] = '/';
        out = 1;
    }
    resolved_path[out] = 0;
    return resolved_path;
}

int
_DEFUN(__librt_chdir_r, (r,dirname),
       struct _reent *r _AND
       const char *dirname)
{
    char resolved[PATH_MAX + 1];
    char *dir;
    s32 fd, res;

    if (!__full_path(dirname, resolved)) {
        r->_errno = ENOENT;
        return -1;
    }

    res = sysLv2FsOpenDir(resolved, &fd);
    if (res != 0)
        return lv2errno_r(r, res);

    sysLv2FsCloseDir(fd);
    dir = strdup(resolved);
    if (!dir) {
        r->_errno = ENOMEM;
        return -1;
    }

    free(__cwd);
    __cwd = dir;
    return 0;
}

char*
_DEFUN(__librt_getcwd_r, (r,buf,size),
       struct _reent *r _AND
       char *buf _AND
       size_t size)
{
    size_t cwd_len = strlen(__cwd);
    if (buf == NULL || size <= cwd_len) {
        r->_errno = EINVAL;
        return NULL;
    }

    buf[0] = 0;
    strncat(buf, __cwd, size);

    return buf;
}
