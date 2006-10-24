/*
    envvar.c:

    Copyright (C) 2005 Istvan Varga

    This file is part of Csound.

    The Csound Library is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Csound is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with Csound; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
    02111-1307 USA
*/

#include "csoundCore.h"
#include <ctype.h>
#include <math.h>
#include "envvar.h"

#if defined(mac_classic) && defined(__MWERKS__)
#include <unix.h>
#endif
#if defined(SYMANTEC)
extern off_t lseek(int, off_t, int);
#endif

#include "namedins.h"

/* list of environment variables used by Csound */

static const char *envVar_list[] = {
    "CSNOSTOP",
    "CSOUNDRC",
    "CSSTRNGS",
    "CS_LANG",
    "HOME",
    "INCDIR",
    "OPCODEDIR",
    "OPCODEDIR64",
    "RAWWAVE_PATH",
    "SADIR",
    "SFDIR",
    "SFOUTYP",
    "SNAPDIR",
    "SSDIR",
    "MFDIR",
    NULL
};

#define ENV_DB          (((CSOUND*) csound)->envVarDB)

#if defined MSVC
#define RD_OPTS  O_RDONLY | O_BINARY
#define WR_OPTS  O_TRUNC | O_CREAT | O_WRONLY | O_BINARY,_S_IWRITE
#elif defined(mac_classic) || defined(SYMANTEC) || defined(WIN32)
#define RD_OPTS  O_RDONLY | O_BINARY
#define WR_OPTS  O_TRUNC | O_CREAT | O_WRONLY | O_BINARY, 0644
#elif defined DOSGCC
#define RD_OPTS  O_RDONLY | O_BINARY, 0
#define WR_OPTS  O_TRUNC | O_CREAT | O_WRONLY | O_BINARY, 0644
#else
#ifndef O_BINARY
# define O_BINARY (0)
#endif
#define RD_OPTS  O_RDONLY | O_BINARY, 0
#define WR_OPTS  O_TRUNC | O_CREAT | O_WRONLY | O_BINARY, 0644
#endif

typedef struct envVarEntry_s {
    struct envVarEntry_s
            *nxt;               /* pointer to next link in chain        */
    char    *name;              /* name of environment variable         */
    char    *value;             /* value of environment variable        */
} envVarEntry_t;

typedef struct searchPathCacheEntry_s {
    char    *name;
    struct searchPathCacheEntry_s   *nxt;
    char    *lst[1];
} searchPathCacheEntry_t;

typedef struct nameChain_s {
    struct nameChain_s  *nxt;
    char    s[1];
} nameChain_t;

typedef struct CSFILE_ {
    struct CSFILE_  *nxt;
    struct CSFILE_  *prv;
    int             type;
    int             fd;
    FILE            *f;
    SNDFILE         *sf;
    char            fullName[1];
} CSFILE;

/* Space for 16 global environment variables, */
/* 32 bytes for name and 480 bytes for value. */
/* Only written by csoundSetGlobalEnv().      */

static char globalEnvVars[8192] = { (char) 0 };

#define globalEnvVarName(x)   ((char*) &(globalEnvVars[(int) (x) << 9]))
#define globalEnvVarValue(x)  ((char*) &(globalEnvVars[((int) (x) << 9) + 32]))

static inline envVarEntry_t **getEnvVarChain(CSOUND *csound, const char *name)
{
    unsigned char h;
    /* check for trivial cases */
    if (ENV_DB == NULL || name == NULL || name[0] == '\0')
      return NULL;
    /* calculate hash value */
    h = name_hash_2(csound, name);
    /* return with pointer from table */
    return &(((envVarEntry_t**) ENV_DB)[(int) h]);
}

static int is_valid_envvar_name(const char *name)
{
    char *s;

    if (name == NULL || name[0] == '\0')
      return 0;
    s = (char*) &(name[0]);
    if (!(isalpha(*s) || *s == '_'))
      return 0;
    while (*(++s) != '\0') {
      if (!(isalpha(*s) || isdigit(*s) || *s == '_'))
        return 0;
    }
    return 1;
}

/**
 * Get pointer to value of environment variable 'name'.
 * Return value is NULL if the variable is not set.
 */

PUBLIC const char *csoundGetEnv(CSOUND *csound, const char *name)
{
    envVarEntry_t **pp, *p;

    if (csound == NULL) {
      int i;
      if (name == NULL || name[0] == '\0')
        return (const char*) NULL;
      for (i = 0; i < 16; i++) {
        if (strcmp(globalEnvVarName(i), name) == 0)
          return (const char*) globalEnvVarValue(i);
      }
      return (const char*) getenv(name);
    }
    pp = getEnvVarChain(csound, name);
    if (pp == NULL)
      return (const char*) NULL;
    p = *pp;
    while (p != NULL && sCmp(p->name, name) != 0)
      p = p->nxt;
    if (p == NULL)
      return (const char*) NULL;
    return (const char*) p->value;
}

/**
 * Set the global value of environment variable 'name' to 'value',
 * or delete variable if 'value' is NULL.
 * It is not safe to call this function while any Csound instances
 * are active.
 * Returns zero on success.
 */

PUBLIC int csoundSetGlobalEnv(const char *name, const char *value)
{
    int   i;

    if (name == NULL || name[0] == '\0' || (int) strlen(name) >= 32)
      return -1;                        /* invalid name             */
    for (i = 0; i < 16; i++) {
      if ((value != NULL && globalEnvVarName(i)[0] == '\0') ||
          strcmp(name, globalEnvVarName(i)) == 0)
        break;
    }
    if (i >= 16)                        /* not found / no free slot */
      return -1;
    if (value == NULL) {
      globalEnvVarName(i)[0] = '\0';    /* delete existing variable */
      return 0;
    }
    if (strlen(value) >= 480)
      return -1;                        /* string value is too long */
    strcpy(globalEnvVarName(i), name);
    strcpy(globalEnvVarValue(i), value);
    return 0;
}

/**
 * Set environment variable 'name' to 'value'.
 * Returns CSOUND_SUCCESS on success, and CSOUND_ERROR or CSOUND_MEMORY
 * if the environment variable could not be set for some reason.
 */

int csoundSetEnv(CSOUND *csound, const char *name, const char *value)
{
    searchPathCacheEntry_t  *ep, *nxt;
    envVarEntry_t           **pp, *p;
    char                    *s1, *s2;

    /* check for valid parameters */
    if (csound == NULL || !is_valid_envvar_name(name))
      return CSOUND_ERROR;
    pp = getEnvVarChain(csound, name);
    if (pp == NULL)
      return CSOUND_ERROR;
    /* invalidate search path cache */
    ep = (searchPathCacheEntry_t*) csound->searchPathCache;
    while (ep != NULL) {
      nxt = ep->nxt;
      mfree(csound, ep);
      ep = nxt;
    }
    csound->searchPathCache = NULL;
    p = *pp;
    s1 = (char*) name;
    s2 = NULL;
    /* copy value */
    if (value != NULL) {
      s2 = (char*) mmalloc(csound, strlen(value) + 1);
      strcpy(s2, value);
    }
    /* is this variable already defined ? */
    while (p != NULL && sCmp(p->name, name) != 0)
      p = p->nxt;
    if (p != NULL) {
      /* yes, only need to replace value */
      if (p->value != NULL)
        mfree(csound, p->value);
      p->value = s2;
    }
    else {
      /* no, need to allocate new entry, and copy name too */
      p = (envVarEntry_t*) mmalloc(csound, sizeof(envVarEntry_t));
      s1 = (char*) mmalloc(csound, (size_t) strlen(name) + (size_t) 1);
      strcpy(s1, name);
      /* store pointers to name and value, and link into chain */
      p->nxt = *pp;
      p->name = s1;
      p->value = s2;
      *pp = p;
    }
    /* print debugging info if requested */
    if (csound->oparms->odebug) {
      csound->Message(csound, Str("Environment variable '%s' has been set to "),
                              name);
      if (value == NULL)
        csound->Message(csound, "NULL\n");
      else
        csound->Message(csound, "'%s'\n", s2);
    }
    /* report success */
    return CSOUND_SUCCESS;
}

/**
 * Append 'value' to environment variable 'name', using ';' as
 * separator character.
 * Returns CSOUND_SUCCESS on success, and CSOUND_ERROR or CSOUND_MEMORY
 * if the environment variable could not be set for some reason.
 */

int csoundAppendEnv(CSOUND *csound, const char *name, const char *value)
{
    const char  *oldval;
    char        *newval;
    int         retval;

    /* check for valid parameters */
    if (csound == NULL || !is_valid_envvar_name(name))
      return CSOUND_ERROR;
    /* get original value of variable */
    oldval = csoundGetEnv(csound, name);
    if (oldval == NULL)
      return csoundSetEnv(csound, name, value);
    if (value == NULL || value[0] == '\0')
      return CSOUND_SUCCESS;
    /* allocate new value (+ 2 bytes for ';' and null character) */
    newval = (char*) mmalloc(csound, (size_t) strlen(oldval)
                                     + (size_t) strlen(value) + (size_t) 2);
    /* append to old value */
    strcpy(newval, oldval);
    strcat(newval, ";");
    strcat(newval, value);
    /* set variable */
    retval = csoundSetEnv(csound, name, newval);
    mfree(csound, newval);
    /* return with error code */
    return retval;
}

/**
 * Prepend 'value' to environment variable 'name', using ';' as
 * separator character.
 * Returns CSOUND_SUCCESS on success, and CSOUND_ERROR or CSOUND_MEMORY
 * if the environment variable could not be set for some reason.
 */

int csoundPrependEnv(CSOUND *csound, const char *name, const char *value)
{
    const char  *oldval;
    char        *newval;
    int         retval;

    /* check for valid parameters */
    if (csound == NULL || !is_valid_envvar_name(name))
      return CSOUND_ERROR;
    /* get original value of variable */
    oldval = csoundGetEnv(csound, name);
    if (oldval == NULL)
      return csoundSetEnv(csound, name, value);
    if (value == NULL || value[0] == '\0')
      return CSOUND_SUCCESS;
    /* allocate new value (+ 2 bytes for ';' and null character) */
    newval = (char*) mmalloc(csound, (size_t) strlen(oldval)
                                     + (size_t) strlen(value) + (size_t) 2);
    /* prepend to old value */
    strcpy(newval, value);
    strcat(newval, ";");
    strcat(newval, oldval);
    /* set variable */
    retval = csoundSetEnv(csound, name, newval);
    mfree(csound, newval);
    /* return with error code */
    return retval;
}

/**
 * Initialise environment variable database, and copy system
 * environment variables.
 * Returns CSOUND_SUCCESS on success, and CSOUND_ERROR or
 * CSOUND_MEMORY in case of an error.
 */

int csoundInitEnv(CSOUND *csound)
{
    int i, retval;
    /* check if already initialised */
    if (ENV_DB != NULL)
      return CSOUND_SUCCESS;
    /* allocate table */
    ENV_DB = (void*) mmalloc(csound, sizeof(envVarEntry_t*) * (size_t) 256);
    for (i = 0; i < 256; i++)
      ((envVarEntry_t**) ENV_DB)[i] = (envVarEntry_t*) NULL;
    /* copy standard Csound environment variables */
    for (i = 0; envVar_list[i] != NULL; i++) {
      const char  *name = envVar_list[i];
      const char  *value = getenv(name);
      if (value != NULL) {
        retval = csoundSetEnv(csound, name, value);
        if (retval != CSOUND_SUCCESS)
          return retval;
      }
    }
    /* copy any global defaults set with csoundSetGlobalEnv() */
    for (i = 0; i < 16; i++) {
      if (globalEnvVarName(i)[0] != '\0') {
        retval = csoundSetEnv(csound, globalEnvVarName(i),
                                      globalEnvVarValue(i));
        if (retval != CSOUND_SUCCESS)
          return retval;
      }
    }
    /* done */
    return CSOUND_SUCCESS;
}

/**
 * Parse 's' as an assignment to environment variable, in the format
 * "NAME=VALUE" for replacing the previous value, or "NAME+=VALUE"
 * for appending.
 * Returns CSOUND_SUCCESS on success, and CSOUND_ERROR or
 * CSOUND_MEMORY in case of an error.
 */

int csoundParseEnv(CSOUND *csound, const char *s)
{
    char  *name, *value, msg[256];
    int   append_mode, retval;

    /* copy string constant */
    name = (char*) mmalloc(csound, (size_t) strlen(s) + (size_t) 1);
    strcpy(name, s);
    /* check assignment format */
    value = strchr(name, '=');
    append_mode = 0;
    if (value == NULL || value == name) {
      sprintf(msg, " *** invalid format for --env\n");
      retval = CSOUND_ERROR;
      goto err_return;
    }
    *(value++) = '\0';
    if (*(value - 2) == '+') {
      append_mode = 1;
      *(value - 2) = '\0';
    }
    if (!is_valid_envvar_name(name)) {
      sprintf(msg, " *** invalid environment variable name\n");
      retval = CSOUND_ERROR;
      goto err_return;
    }
    /* set variable */
    if (!append_mode)
      retval = csoundSetEnv(csound, name, value);
    else
      retval = csoundAppendEnv(csound, name, value);
    if (retval == CSOUND_MEMORY)
      sprintf(msg, " *** memory allocation failure\n");
    else
      sprintf(msg, " *** error setting environment variable\n");

 err_return:
    if (retval != CSOUND_SUCCESS)
      csound->Message(csound, Str(msg));
    if (name != NULL)
      mfree(csound, name);
    return retval;
}

char **csoundGetSearchPathFromEnv(CSOUND *csound, const char *envList)
{
    searchPathCacheEntry_t  *p;
    nameChain_t             *env_lst = NULL, *path_lst = NULL, *tmp, *prv, *nxt;
    char                    *s;
    int                     i, j, k, len, pathCnt = 0, totLen = 0;

    /* check if the specified environment variable list was already parsed */
    p = (searchPathCacheEntry_t*) csound->searchPathCache;
    while (p != NULL) {
      if (sCmp(p->name, envList) == 0)
        return (&(p->lst[0]));
      p = p->nxt;
    }
    /* not found, need to create new entry */
    len = (int) strlen(envList);
    /* split environment variable list to tokens */
    for (i = j = 0; i <= len; i++) {
      if (envList[i] == ';' || envList[i] == '\0') {
        if (i > j) {
          tmp = (nameChain_t*) mmalloc(csound, sizeof(nameChain_t) + (i - j));
          for (k = 0; j < i; j++, k++)
            tmp->s[k] = envList[j];
          tmp->s[k] = '\0';
          tmp->nxt = NULL;
          if (env_lst != NULL) {
            /* search for duplicate entry */
            prv = nxt = env_lst;
            do {
              if (sCmp(env_lst->s, tmp->s) == 0)
                break;
              prv = nxt;
            } while ((nxt = prv->nxt) != NULL);
            if (nxt == NULL)
              prv->nxt = tmp;
            else
              mfree(csound, tmp);       /* and remove if there is any */
          }
          else
            env_lst = tmp;
        }
        j = i + 1;
      }
    }
    /* expand environment variables to path list */
    while (env_lst != NULL) {
      nxt = env_lst->nxt;
      s = (char*) csoundGetEnv(csound, env_lst->s);
      mfree(csound, env_lst);
      env_lst = nxt;
      if (s != NULL && s[0] != '\0')
        len = (int) strlen(s);
      else
        len = -1;
      for (i = j = 0; i <= len; i++) {
        if (s[i] == ';' || s[i] == '\0') {
          if (i > j) {
            tmp = (nameChain_t*) mmalloc(csound, sizeof(nameChain_t)
                                                 + (i - j) + 1);
            /* copy with converting pathname delimiters */
            for (k = 0; j < i; j++, k++)
              tmp->s[k] = (s[j] == '/' || s[j] == '\\' ? DIRSEP : s[j]);
            while (tmp->s[--k] == DIRSEP);
            tmp->s[++k] = DIRSEP;
            tmp->s[++k] = '\0';
            tmp->nxt = path_lst;
            path_lst = tmp;
            /* search for duplicate entry */
            for (prv = tmp; (tmp = tmp->nxt) != NULL; prv = tmp)
              if (sCmp(path_lst->s, tmp->s) == 0)
                break;
            if (tmp != NULL) {
              /* and remove if there is any */
              prv->nxt = tmp->nxt;
              mfree(csound, tmp);
            }
            else {
              /* calculate storage requirement */
              pathCnt++; totLen += (k + 1);
            }
          }
          j = i + 1;
        }
      }
    }
    totLen += ((int) strlen(envList) + 1);
    /* create path cache entry */
    p = (searchPathCacheEntry_t*) mmalloc(csound, sizeof(searchPathCacheEntry_t)
                                                  + sizeof(char*) * pathCnt
                                                  + sizeof(char) * totLen);
    s = (char*) &(p->lst[pathCnt + 1]);
    p->name = s;
    strcpy(p->name, envList);
    s += ((int) strlen(envList) + 1);
    p->nxt = (searchPathCacheEntry_t*) csound->searchPathCache;
    if (csound->oparms->odebug)
      csound->DebugMsg(csound, Str("Creating search path cache for '%s':"),
                               p->name);
    for (i = 0; (i < pathCnt) && (path_lst != NULL); i++) {
      p->lst[i] = s;
      strcpy(s, path_lst->s);
      s += ((int) strlen(path_lst->s) + 1);
      nxt = path_lst->nxt;
      mfree(csound, path_lst);
      path_lst = nxt;
      if (csound->oparms->odebug)
        csound->DebugMsg(csound, "%5d: \"%s\"", (i + 1), p->lst[i]);
    }
    p->lst[i] = NULL;
    /* link into database */
    csound->searchPathCache = (void*) p;
    /* return with pathname list */
    return (&(p->lst[0]));
}

/** Check if file name is valid, and copy with converting pathname delimiters */
char *csoundConvertPathname(CSOUND *csound, const char *filename)
{
    char  *name;
    int   i = 0;

/* FIXMEs:  need to convert ':' from Mac pathnames (but be careful of not
   messing up Windows drive names!); need to be careful of
   relative paths containing "./", "../", or multiple colons "::"; need to
   collapse multiple slashes "//" or "\\\\" ??  */
    if (filename == NULL || filename[0] == '\0')
      return NULL;
    name = (char*) mmalloc(csound, (size_t) strlen(filename) + (size_t) 1);
    do {
      if (filename[i] != '/' && filename[i] != '\\')
        name[i] = filename[i];
      else
        name[i] = DIRSEP;
    } while (filename[i++] != '\0');
    if (name[i - 2] == DIRSEP
#ifdef WIN32
        || (isalpha(name[0]) && name[1] == ':' && name[2] == '\0')
#endif
        ) {
      mfree(csound, name);
      return NULL;
    }
    return name;
}

/**  Check if name is a full pathname for the platform we are running on. */
int csoundIsNameFullpath(const char *name)
{
#ifdef WIN32    
    if (isalpha(name[0]) && name[1] == ':') return 1;
#endif
#ifndef mac_classic
    if (name[0] == DIRSEP) /* ||
        (name[0] == '.' && (name[1] == DIRSEP ||
                            (name[1] == '.' && name[2] == DIRSEP)))) */
      return 1;
    return 0;
#else
    /* MacOS full paths contain DIRSEP but do not start with it */
    if (name[0] == DIRSEP || strchr(name, DIRSEP) == NULL)
      return 0;
    return 1;
#endif
}

/** Check if name is a relative pathname for this platform.  Bare 
 *  filenames with no path information are not counted.
 */
int csoundIsNameRelativePath(const char *name)
{
#ifndef mac_classic
    if (name[0] != DIRSEP && strchr(name, DIRSEP) != NULL)
      return 1;
    return 0;
#else
    /* MacOS relative paths begin with DIRSEP */
    if (name[0] == DIRSEP)
      return 1;
    return 0;
#endif
}

/** Check if name is a "leaf" (bare) filename for this platform. */
int csoundIsNameJustFilename(const char *name)
{
    if (strchr(name, DIRSEP) != NULL) return 0;
#ifdef WIN32
    if (name[2] == ':') return 0;
#endif
    return 1;
}

/** Properly concatenates the full or relative pathname in path1 with 
 *  the relative pathname or filename in path2 according to the rules
 *  for the platform we are running on.  path1 is assumed to be
 *  a directory whether it ends with DIRSEP or not.  Relative paths must
 *  conform to the conventions for the current platform (begin with ':'
 *  on MacOS 9 and not begin with DIRSEP on others).
 */
char* csoundConcatenatePaths(CSOUND* csound, const char *path1, const char *path2)
{
    char *result;
    const char *start2;
    char separator[2];
    int  len1 = strlen(path1);
    int  len2 = strlen(path2);
    
    /* cannot join two full pathnames -- so just return path2 ? */
    if (csoundIsNameFullpath(path2)) {
        result = (char*) mmalloc(csound, (size_t)len2+1);
        strcpy(result, path2);
        return result;
    }

    start2 = path2;
#ifndef mac_classic
    /* ignore "./" at the beginning */
    if (path2[0] == '.' && path2[1] == DIRSEP) start2 = path2 + 2;
#else
    /* ignore the first ':' of path2 if any are present */
    if (path2[0] == ':') start2 = path2 + 1;
#endif
    
    result = (char*) mmalloc(csound, (size_t)len1+(size_t)len2+2);
    strcpy(result, path1);
    /* check for final DIRSEP in path1 */
    if (path1[len1-1] != DIRSEP) {
        separator[0] = DIRSEP; separator[1] = '\0';
        strcat(result, separator);
    }
    strcat(result, start2);
    
    return result;
}

/** Converts a pathname to native format and returns just the part of
 *  the path that specifies the directory.  Does not return the final 
 *  DIRSEP.  Returns an empty string if no path components occur before
 *  the filename.  Returns NULL if unable to carry out the operation 
 *  for some reason.
 */
char *csoundSplitDirectoryFromPath(CSOUND* csound, const char * path)
{
    char *convPath;
    char *lastIndex;
    char *partialPath;
    int  len;

    if ((convPath = csoundConvertPathname(csound, path)) == NULL)
        return NULL;
    lastIndex = strrchr(convPath, DIRSEP);
    
    if (lastIndex == NULL) {  /* no DIRSEP before filename */
#ifdef WIN32  /* e.g. C:filename */
        if (isalpha(convPath[0]) && convPath[1] == ':') {
            partialPath = (char*) mmalloc(csound, (size_t) 3);
            partialPath[0] = convPath[0];
            partialPath[1] = convPath[1];
            partialPath[2] = '\0';
            mfree(csound, convPath);
            return partialPath;
        }
#endif
        partialPath = (char*) mmalloc(csound, (size_t) 1);
        partialPath[0] = '\0';
    }
    else {
        len = lastIndex - convPath;
        partialPath = (char*) mmalloc(csound, len);
        strncpy(partialPath, convPath, len);
        partialPath[len] = '\0';
   }
   mfree(csound, convPath);
   return partialPath;
}

/** Return just the final component of a full path */
char *csoundSplitFilenameFromPath(CSOUND* csound, const char * path)
{
    char *convPath;
    char *lastIndex;
    char *filename;
    int  len;

    if ((convPath = csoundConvertPathname(csound, path)) == NULL)
        return NULL;
    lastIndex = strrchr(convPath, DIRSEP);
    len = strlen(lastIndex);
    filename = (char*) mmalloc(csound, len+1);
    strcpy(filename, lastIndex+1);
    mfree(csound, convPath);
    return filename;
}

/* given a file name as string, return full path of directory of file;
 * Note: does not check if file exists
 */
char *csoundGetDirectoryForPath(CSOUND* csound, const char * path) {
    char *partialPath;
    char *retval;
    char *cwd;
    int  len;
    char *lastIndex = strrchr(path, DIRSEP);

    if (csoundIsNameFullpath(path))
    {
#ifndef mac_classic
        /* check if root directory */
        if (lastIndex == path) {
            partialPath = (char *)mcalloc(csound, 2);
            partialPath[0] = DIRSEP;
            partialPath[1] = '\0';

            return partialPath;
        }

#  ifdef WIN32
        /* check if root directory of Windows drive */
        if ((lastIndex - path) == 2 && path[1] == ':') {
            partialPath = (char *)mcalloc(csound, 4);
            partialPath[0] = path[0];
            partialPath[1] = path[1];
            partialPath[2] = path[2];
            partialPath[3] = '\0';

            return partialPath;
        }
#  endif
#endif  /* no special case needed on OS 9 for root directory */
        /* not the root directory or we are on OS 9 */
        len = (lastIndex - path);

        partialPath = (char *)mcalloc(csound, len + 1);
        strncpy(partialPath, path, len);

        return partialPath;
    }

    /* do we need to worry about ~/ on *nix systems ? */
    /* we have a relative path or just a filename */
    cwd = mmalloc(csound, 512);
    getcwd(cwd, 512);

    if(lastIndex == NULL) {
        return cwd;
    }

    len = (lastIndex - path);  /* could be 0 on OS 9 */

    partialPath = (char *)mcalloc(csound, len + 1);
    strncpy(partialPath, path, len);

    retval = csoundConcatenatePaths(csound, cwd, partialPath);

    mfree(csound, cwd);
    mfree(csound, partialPath);

    return retval;
}

static FILE *csoundFindFile_Std(CSOUND *csound, char **fullName,
                                const char *filename, const char *mode,
                                const char *envList)
{
    FILE  *f;
    char  *name, *name2, **searchPath;
    int   len;

    *fullName = NULL;
    if ((name = csoundConvertPathname(csound, filename)) == NULL)
      return (FILE*) NULL;
    if (mode[0] != 'w') {
      /* read: try the specified name first */
      f = fopen(name, mode);
      if (f != NULL) {
        *fullName = name;
        return f;
      }
      /* if full path, and not found: */
      if (csoundIsNameFullpath(name)) {
        mfree(csound, name);
        return (FILE*) NULL;
      }
    }
    else if (csoundIsNameFullpath(name)) {
      /* if write and full path: */
      f = fopen(name, mode);
      if (f != NULL)
        *fullName = name;
      else
        mfree(csound, name);
      return f;
    }
    /* search paths defined by environment variable list */
    if (envList != NULL && envList[0] != '\0' &&
        (searchPath = csoundGetSearchPathFromEnv((CSOUND*) csound, envList))
          != NULL) {
      len = (int) strlen(name) + 1;
      while (*searchPath != NULL) {
        name2 = csoundConcatenatePaths(csound, *searchPath, name);
        f = fopen(name2, mode);
        if (f != NULL) {
          mfree(csound, name);
          *fullName = name2;
          return f;
        }
        mfree(csound, name2);
        searchPath++;
      }
    }
    /* if write mode, try current directory last */
    if (mode[0] == 'w') {
      f = fopen(name, mode);
      if (f != NULL) {
        *fullName = name;
        return f;
      }
    }
    /* not found */
    mfree(csound, name);
    return (FILE*) NULL;
}

static int csoundFindFile_Fd(CSOUND *csound, char **fullName,
                             const char *filename, int write_mode,
                             const char *envList)
{
    char  *name, *name2, **searchPath;
    int   len, fd;

    *fullName = NULL;
    if ((name = csoundConvertPathname(csound, filename)) == NULL)
      return -1;
    if (!write_mode) {
      /* read: try the specified name first */
      fd = open(name, RD_OPTS);
      if (fd >= 0) {
        *fullName = name;
        return fd;
      }
      /* if full path, and not found: */
      if (csoundIsNameFullpath(name)) {
        mfree(csound, name);
        return -1;
      }
    }
    else if (csoundIsNameFullpath(name)) {
      /* if write and full path: */
      fd = open(name, WR_OPTS);
      if (fd >= 0)
        *fullName = name;
      else
        mfree(csound, name);
      return fd;
    }
    /* search paths defined by environment variable list */
    if (envList != NULL && envList[0] != '\0' &&
        (searchPath = csoundGetSearchPathFromEnv((CSOUND*) csound, envList))
          != NULL) {
      len = (int) strlen(name) + 1;
      while (*searchPath != NULL) {
        name2 = csoundConcatenatePaths(csound, *searchPath, name);
        if (!write_mode)
          fd = open(name2, RD_OPTS);
        else
          fd = open(name2, WR_OPTS);
        if (fd >= 0) {
          mfree(csound, name);
          *fullName = name2;
          return fd;
        }
        mfree(csound, name2);
        searchPath++;
      }
    }
    /* if write mode, try current directory last */
    if (write_mode) {
      fd = open(name, WR_OPTS);
      if (fd >= 0) {
        *fullName = name;
        return fd;
      }
    }
    /* not found */
    mfree(csound, name);
    return -1;
}

/**
 * Search for input file 'filename'.
 * If the file name specifies full path (it begins with '.', the pathname
 * delimiter character, or a drive letter and ':' on Windows), that exact
 * file name is tried without searching.
 * Otherwise, the file is searched relative to the current directory first,
 * and if it is still not found, a pathname list that is created the
 * following way is searched:
 *   1. if envList is NULL or empty, no directories are searched
 *   2. envList is parsed as a ';' separated list of environment variable
 *      names, and all environment variables are expanded and expected to
 *      contain a ';' separated list of directory names
 *   2. all directories in the resulting pathname list are searched, starting
 *      from the last and towards the first one, and the directory where the
 *      file is found first will be used
 * The function returns a pointer to the full name of the file if it is
 * found, and NULL if the file could not be found in any of the search paths,
 * or an error has occured. The caller is responsible for freeing the memory
 * pointed to by the return value, by calling mfree().
 */
char *csoundFindInputFile(CSOUND *csound,
                          const char *filename, const char *envList)
{
    char  *name_found;
    int   fd;

    if (csound == NULL)
      return NULL;
    fd = csoundFindFile_Fd(csound, &name_found, filename, 0, envList);
    if (fd >= 0)
      close(fd);
    return name_found;
}

/**
 * Search for a location to write file 'filename'.
 * If the file name specifies full path (it begins with '.', the pathname
 * delimiter character, or a drive letter and ':' on Windows), that exact
 * file name is tried without searching.
 * Otherwise, a pathname list that is created the following way is searched:
 *   1. if envList is NULL or empty, no directories are searched
 *   2. envList is parsed as a ';' separated list of environment variable
 *      names, and all environment variables are expanded and expected to
 *      contain a ';' separated list of directory names
 *   2. all directories in the resulting pathname list are searched, starting
 *      from the last and towards the first one, and the directory that is
 *      found first where the file can be written to will be used
 * Finally, if the file cannot be written to any of the directories in the
 * search paths, writing relative to the current directory is tried.
 * The function returns a pointer to the full name of the file if a location
 * suitable for writing the file is found, and NULL if the file cannot not be
 * written anywhere in the search paths, or an error has occured.
 * The caller is responsible for freeing the memory pointed to by the return
 * value, by calling mfree().
 */
char *csoundFindOutputFile(CSOUND *csound,
                           const char *filename, const char *envList)
{
    char  *name_found;
    int   fd;

    if (csound == NULL)
      return NULL;
    fd = csoundFindFile_Fd(csound, &name_found, filename, 1, envList);
    if (fd >= 0) {
      close(fd);
      remove(name_found);
    }
    return name_found;
}

/**
 * Open a file and return handle.
 *
 * CSOUND *csound:
 *   Csound instance pointer
 * void *fd:
 *   pointer a variable of type int, FILE*, or SNDFILE*, depending on 'type',
 *   for storing handle to be passed to file read/write functions
 * int type:
 *   file type, one of the following:
 *     CSFILE_FD_R:     read file using low level interface (open())
 *     CSFILE_FD_W:     write file using low level interface (open())
 *     CSFILE_STD:      use ANSI C interface (fopen())
 *     CSFILE_SND_R:    read sound file
 *     CSFILE_SND_W:    write sound file
 * const char *name:
 *   file name
 * void *param:
 *   parameters, depending on type:
 *     CSFILE_FD_R:     unused (should be NULL)
 *     CSFILE_FD_W:     unused (should be NULL)
 *     CSFILE_STD:      mode parameter (of type char*) to be passed to fopen()
 *     CSFILE_SND_R:    SF_INFO* parameter for sf_open(), with defaults for
 *                      raw file; the actual format paramaters of the opened
 *                      file will be stored in this structure
 *     CSFILE_SND_W:    SF_INFO* parameter for sf_open(), output file format
 * const char *env:
 *   list of environment variables for search path (see csoundFindInputFile()
 *   for details); if NULL, the specified name is used as it is, without any
 *   conversion or search.
 * return value:
 *   opaque handle to the opened file, for use with csoundGetFileName() or
 *   csoundFileClose(), or storing in FDCH.fd.
 *   On failure, NULL is returned.
 */

void *csoundFileOpen(CSOUND *csound, void *fd, int type,
                     const char *name, void *param, const char *env)
{
    CSFILE  *p = NULL;
    char    *fullName = NULL;
    FILE    *tmp_f = NULL;
    SF_INFO sfinfo;
    int     tmp_fd = -1, nbytes = (int) sizeof(CSFILE);

    /* check file type */
    if ((unsigned int) (type - 1) >= (unsigned int) CSFILE_SND_W) {
      csoundErrorMsg(csound, Str("internal error: csoundFileOpen(): "
                                 "invalid type: %d"), type);
      return NULL;
    }
    /* get full name and open file */
    if (env == NULL) {
      fullName = (char*) name;
      if (type == CSFILE_STD) {
        tmp_f = fopen(fullName, (char*) param);
        if (tmp_f == NULL)
          goto err_return;
      }
      else {
        if (type == CSFILE_SND_R || type == CSFILE_FD_R)
          tmp_fd = open(fullName, RD_OPTS);
        else
          tmp_fd = open(fullName, WR_OPTS);
        if (tmp_fd < 0)
          goto err_return;
      }
    }
    else {
      if (type == CSFILE_STD) {
        tmp_f = csoundFindFile_Std(csound, &fullName, name, (char*) param, env);
        if (tmp_f == NULL)
          goto err_return;
      }
      else {
        if (type == CSFILE_SND_R || type == CSFILE_FD_R)
          tmp_fd = csoundFindFile_Fd(csound, &fullName, name, 0, env);
        else
          tmp_fd = csoundFindFile_Fd(csound, &fullName, name, 1, env);
        if (tmp_fd < 0)
          goto err_return;
      }
    }
    nbytes += (int) strlen(fullName);
    /* allocate file structure */
    p = (CSFILE*) malloc((size_t) nbytes);
    if (p == NULL)
      goto err_return;
    p->nxt = (CSFILE*) csound->open_files;
    p->prv = (CSFILE*) NULL;
    p->type = type;
    p->fd = tmp_fd;
    p->f = tmp_f;
    p->sf = (SNDFILE*) NULL;
    strcpy(&(p->fullName[0]), fullName);
    if (env != NULL) {
      mfree(csound, fullName);
      env = NULL;
    }
    /* if sound file, re-open file descriptor with libsndfile */
    switch (type) {
      case CSFILE_STD:                          /* stdio */
        *((FILE**) fd) = tmp_f;
        break;
      case CSFILE_SND_R:                        /* sound file read */
        memset(&sfinfo, 0, sizeof(SF_INFO));
        p->sf = sf_open_fd(tmp_fd, SFM_READ, &sfinfo, 0);
        if (p->sf == (SNDFILE*) NULL) {
          int   extPos;
          /* open failed: */
          extPos = (nbytes - (int) sizeof(CSFILE)) - 4;
          /* check for .sd2 file first */
          if (extPos > 0 &&
              p->fullName[extPos] == (char) '.' &&
              (p->fullName[extPos + 1] | (char) 0x20) == (char) 's' &&
              (p->fullName[extPos + 2] | (char) 0x20) == (char) 'd' &&
              p->fullName[extPos + 3] == (char) '2') {
            memset(&sfinfo, 0, sizeof(SF_INFO));
            p->sf = sf_open(&(p->fullName[0]), SFM_READ, &sfinfo);
            if (p->sf != (SNDFILE*) NULL) {
              /* if successfully opened as .sd2, */
              /* the integer file descriptor is no longer needed */
              close(tmp_fd);
              p->fd = tmp_fd = -1;
              goto doneSFOpen;
            }
          }
          /* maybe raw file ? rewind and try again */
          if (lseek(tmp_fd, (off_t) 0, SEEK_SET) == (off_t) 0)
            p->sf = sf_open_fd(tmp_fd, SFM_READ, (SF_INFO*) param, 0);
          if (p->sf == (SNDFILE*) NULL)
            goto err_return;
        }
        else {
 doneSFOpen:
          memcpy((SF_INFO*) param, &sfinfo, sizeof(SF_INFO));
        }
        *((SNDFILE**) fd) = p->sf;
        break;
      case CSFILE_SND_W:                        /* sound file write */
        p->sf = sf_open_fd(tmp_fd, SFM_WRITE, (SF_INFO*) param, 0);
        if (p->sf == (SNDFILE*) NULL)
          goto err_return;
        sf_command(p->sf, SFC_SET_CLIPPING, NULL, SF_TRUE);
        *((SNDFILE**) fd) = p->sf;
        break;
      default:                                  /* low level I/O */
        *((int*) fd) = tmp_fd;
    }
    /* link into chain of open files */
    if (csound->open_files != NULL)
      ((CSFILE*) csound->open_files)->prv = p;
    csound->open_files = (void*) p;
    /* return with opaque file handle */
    return (void*) p;

 err_return:
    /* clean up on error */
    if (p != NULL)
      free(p);
    if (fullName != NULL && env != NULL)
      mfree(csound, fullName);
    if (tmp_fd >= 0)
      close(tmp_fd);
    else if (tmp_f != NULL)
      fclose(tmp_f);
    if (type > CSFILE_STD)
      *((SNDFILE**) fd) = (SNDFILE*) NULL;
    else if (type == CSFILE_STD)
      *((FILE**) fd) = (FILE*) NULL;
    else
      *((int*) fd) = -1;
    return NULL;
}

/**
 * Allocate a file handle for an existing file already opened with open(),
 * fopen(), or sf_open(), for later use with csoundFileClose() or
 * csoundGetFileName(), or storing in an FDCH structure.
 * Files registered this way (or opened with csoundFileOpen()) are also
 * automatically closed by csoundReset().
 * Parameters and return value are similar to csoundFileOpen(), except
 * fullName is the name that will be returned by a later call to
 * csoundGetFileName().
 */

void *csoundCreateFileHandle(CSOUND *csound,
                             void *fd, int type, const char *fullName)
{
    CSFILE  *p = NULL;
    int     nbytes = (int) sizeof(CSFILE);

    /* name should not be empty */
    if (fullName == NULL || fullName[0] == '\0')
      return NULL;
    nbytes += (int) strlen(fullName);
    /* allocate file structure */
    p = (CSFILE*) malloc((size_t) nbytes);
    if (p == NULL)
      return NULL;
    p->nxt = (CSFILE*) csound->open_files;
    p->prv = (CSFILE*) NULL;
    p->type = type;
    p->fd = -1;
    p->f = (FILE*) NULL;
    p->sf = (SNDFILE*) NULL;
    strcpy(&(p->fullName[0]), fullName);
    /* open file */
    switch (type) {
      case CSFILE_FD_R:
      case CSFILE_FD_W:
        p->fd = *((int*) fd);
        break;
      case CSFILE_STD:
        p->f = *((FILE**) fd);
        break;
      case CSFILE_SND_R:
      case CSFILE_SND_W:
        p->sf = *((SNDFILE**) fd);
        break;
      default:
        csoundErrorMsg(csound, Str("internal error: csoundCreateFileHandle(): "
                                   "invalid type: %d"), type);
        free(p);
        return NULL;
    }
    /* link into chain of open files */
    if (csound->open_files != NULL)
      ((CSFILE*) csound->open_files)->prv = p;
    csound->open_files = (void*) p;
    /* return with opaque file handle */
    return (void*) p;
}

/**
 * Get the full name of a file previously opened with csoundFileOpen().
 */

char *csoundGetFileName(void *fd)
{
    return &(((CSFILE*) fd)->fullName[0]);
}

/**
 * Close a file previously opened with csoundFileOpen().
 */

int csoundFileClose(CSOUND *csound, void *fd)
{
    CSFILE  *p = (CSFILE*) fd;
    int     retval = -1;

    /* close file */
    switch (p->type) {
      case CSFILE_FD_R:
      case CSFILE_FD_W:
        retval = close(p->fd);
        break;
      case CSFILE_STD:
        retval = fclose(p->f);
        break;
      case CSFILE_SND_R:
      case CSFILE_SND_W:
        retval = sf_close(p->sf);
        if (p->fd >= 0)
          retval |= close(p->fd);
        break;
    }
    /* unlink from chain of open files */
    if (p->prv == NULL)
      csound->open_files = (void*) p->nxt;
    else
      p->prv->nxt = p->nxt;
    if (p->nxt != NULL)
      p->nxt->prv = p->prv;
    /* free allocated memory */
    free(fd);
    /* return with error value */
    return retval;
}

/* Close all open files; called by csoundReset(). */

void close_all_files(CSOUND *csound)
{
    while (csound->open_files != NULL)
      csoundFileClose(csound, csound->open_files);
}

