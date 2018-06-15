#include <unistd.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <libgen.h>
#include <string.h>
#include <sys/time.h>

#include "snapshot.h"

/*
    this fuction is desingned open the file
    return: int to represent if good or bad
*/
static int pid_in_core (void)
{
    FILE* fin = fopen ("/proc/sys/kernel/core_uses_pid", "r");
    if (fin == NULL)
        return 0;
    char val = fgetc (fin);
    fclose(fin);
    return val == '1' ? 1 : 0;
}

static int fileExists (char *path)
{
    struct stat sb;
    return stat (path, &sb) == 0 && !S_ISDIR (sb.st_mode);
}

static int dirExists (char *path)
{
    struct stat sb;
    return stat (path, &sb) == 0 && S_ISDIR (sb.st_mode);
}

static int tar (char *fname)
{
    char cmd[4096];
    sprintf (cmd, "tar -czvf %s.tgz %s", fname, fname);
    return system (cmd);
}

int remove_directory (const char *path)
{
    DIR *d = opendir (path);
    if (d == NULL)
        return -1;

    int res = 0;
    char fname [PATH_MAX];
    struct dirent *p;
    struct stat statbuf;

    while (res == 0 && (p = readdir(d)))
    {
        /* Skip the names "." and ".." as we don't want to recurse on them. */
        if (strcmp(p->d_name, ".") == 0 || strcmp(p->d_name, "..") == 0)
            continue;

        snprintf(fname, sizeof(fname), "%s/%s", path, p->d_name);
        if (!stat(fname, &statbuf))
        {
            if (S_ISDIR (statbuf.st_mode))
                res = remove_directory (fname);
            else
                res = unlink (fname);
        }
    }
    closedir (d);
    res = rmdir (path);
    return res;
}

static int copyFile (char *source, char *destination)
{
    FILE* fileInput = fopen (source, "r");
    FILE* fileOutput = fopen (destination, "w");
    int character;
    while ((character = fgetc (fileInput)) != EOF)
    {
        fputc (character, fileOutput);
    }
    fclose (fileInput);
    fclose (fileOutput);
    return 0;
}

static void changeToExeDir(void)
{
    // Get path to exe from symbolic link
    char name[PATH_MAX];
    snprintf(name, sizeof (name), "/proc/%d/exe", getpid ());
    char buf[PATH_MAX];
    buf [0] = '\0';
    int len = readlink (name, buf, sizeof (buf));
    if (len != -1)
    {
        buf [len] = '\0';
        chdir (dirname (buf));
    }
}

int snapshot(char *ssname, char *progpath, char *readme)
{
    char tmpName [PATH_MAX];
    char cwdbuf [PATH_MAX];
    getcwd (cwdbuf, sizeof (cwdbuf));
    changeToExeDir ();

    // check if dir exists, fail
    if (dirExists(ssname))
    {
        printf ("%s already exists, cannot overwrite\n", ssname);
        chdir (cwdbuf);
        return EXIT_FAILURE;
    }

    struct rlimit cl;
    cl.rlim_cur = cl.rlim_max = RLIM_INFINITY;
    CHECK_SYSCALL_TRACE (setrlimit (RLIMIT_CORE, &cl), snapshot_debug);

    // create dir
    CHECK_SYSCALL_TRACE(mkdir(ssname, 0777), snapshot_debug);

    char *progDirD = strdup(progpath);
    char *progDir = dirname(progDirD);
    char *progNameD = strdup(progpath);
    char *progName = basename(progNameD);
    
    // Copy binary file into temp directory
    snprintf (tmpName, sizeof (tmpName), "%s/%s", ssname, progName);
    copyFile (progpath, tmpName);

    // Copy core dump file into temp directory
    char srcCoreName [PATH_MAX];
    int coreHasPid = pid_in_core ();
    if (coreHasPid)
    {
        pid_t pid = getpid ();
        snprintf (srcCoreName, sizeof (srcCoreName), "%s/core_%d", progDir, pid);
    }
    else
    {
        snprintf (srcCoreName, sizeof (srcCoreName), "%s/core", progDir);
    }
    
    snprintf (tmpName, sizeof (tmpName), "%s/core", ssname);
    if (fileExists (srcCoreName))
        copyFile (srcCoreName, tmpName);

    // make readme
    snprintf (tmpName, sizeof (tmpName), "%s/README", ssname);
    FILE* fout = fopen (tmpName, "w");
    if (fout == NULL)
    {
        printf ("error creating %s\n", tmpName);
        chdir (cwdbuf);
        return EXIT_FAILURE;
    }
    fprintf (fout, "%s\n", readme);
    fclose (fout);

    // tar    
    CHECK_SYSCALL_TRACE (tar (ssname), snapshot_debug);

    // delete temp dir
    CHECK_SYSCALL_TRACE (remove_directory (ssname), snapshot_debug);

    chdir(cwdbuf);
    free(progNameD);
    free(progDirD);
    return EXIT_SUCCESS;
}