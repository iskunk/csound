/*
    main.c:

    Copyright (C) 1991-2002 Barry Vercoe, John ffitch

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

#include "csoundCore.h"         /*                      MAIN.C          */
#include "soundio.h"
#include "csmodule.h"

extern  void    dieu(CSOUND *, char *, ...);
extern  int     argdecode(CSOUND *, int, char **);
extern  int     init_pvsys(CSOUND *);
extern  char    *get_sconame(CSOUND *);
extern  void    print_benchmark_info(CSOUND *, const char *);
extern  void    openMIDIout(CSOUND *);
extern  int     read_unified_file(CSOUND *, char **, char **);

extern  OENTRY  opcodlst_1[];

static void create_opcodlst(CSOUND *csound)
{
    OENTRY  *saved_opcodlst = csound->opcodlst;
    int     old_cnt = 0, err;

    if (saved_opcodlst != NULL) {
      csound->opcodlst = NULL;
      if (csound->oplstend != NULL)
        old_cnt = (int) ((OENTRY*) csound->oplstend - (OENTRY*) saved_opcodlst);
      csound->oplstend = NULL;
      memset(csound->opcode_list, 0, sizeof(int) * 256);
    }
    /* Basic Entry1 stuff */
    err = csoundAppendOpcodes(csound, &(opcodlst_1[0]), -1);
    /* Add opcodes registered by host application */
    if (old_cnt)
      err |= csoundAppendOpcodes(csound, saved_opcodlst, old_cnt);
    if (saved_opcodlst != NULL)
      free(saved_opcodlst);
    if (err)
      csoundDie(csound, Str("Error allocating opcode list"));
}

PUBLIC int csoundCompile(CSOUND *csound, int argc, char **argv)
{
    OPARMS  *O = csound->oparms;
    char    *s;
    char    *sortedscore = NULL;
    char    *xtractedscore = "score.xtr";
    char    *playscore = NULL;      /* unless we extract */
    FILE    *scorin = NULL, *scorout = NULL, *xfile = NULL;
    int     n;
    int     csdFound = 0;
    char    *fileDir;

    /* IV - Feb 05 2005: find out if csoundPreCompile() needs to be called */
    if (csound->engineState != CS_STATE_PRE) {
      if ((n = csoundPreCompile(csound)) != CSOUND_SUCCESS)
        return n;
    }

    if ((n = setjmp(csound->exitjmp)) != 0) {
      return ((n - CSOUND_EXITJMP_SUCCESS) | CSOUND_EXITJMP_SUCCESS);
    }

    init_pvsys(csound);
    /* utilities depend on this as well as orchs; may get changed by an orch */
    dbfs_init(csound, DFLT_DBFS);
    csound->csRtClock = (RTCLOCK*) csound->Calloc(csound, sizeof(RTCLOCK));
    csoundInitTimerStruct(csound->csRtClock);
    csound->engineState |= CS_STATE_COMP | CS_STATE_CLN;

#ifndef USE_DOUBLE
#ifdef BETA
    csound->Message(csound, Str("Csound version %s beta (float samples) %s\n"),
                            CS_PACKAGE_VERSION, __DATE__);
#else
    csound->Message(csound, Str("Csound version %s (float samples) %s\n"),
                            CS_PACKAGE_VERSION, __DATE__);
#endif
#else
#ifdef BETA
    csound->Message(csound, Str("Csound version %s beta (double samples) %s\n"),
                            CS_PACKAGE_VERSION, __DATE__);
#else
    csound->Message(csound, Str("Csound version %s (double samples) %s\n"),
                            CS_PACKAGE_VERSION, __DATE__);
#endif
#endif
    {
      char buffer[128];
      sf_command(NULL, SFC_GET_LIB_VERSION, buffer, 128);
      csound->Message(csound, "%s\n", buffer);
    }

    /* do not know file type yet */
    O->filetyp = -1;
    O->sfheader = 0;
    csound->peakchunks = 1;
    create_opcodlst(csound);

    if (--argc <= 0) {
      dieu(csound, Str("insufficient arguments"));
    }
    /* command line: allow orc/sco/csd name */
    csound->orcname_mode = 0;   /* 0: normal, 1: ignore, 2: fail */
    if (argdecode(csound, argc, argv) == 0)
      csound->LongJmp(csound, 1);
    /* do not allow orc/sco/csd name in .csoundrc */
    csound->orcname_mode = 2;
    {
      const char  *csrcname;
      const char  *home_dir;
      FILE        *csrc = NULL;
      void        *fd = NULL;
      /* IV - Feb 17 2005 */
      csrcname = csoundGetEnv(csound, "CSOUNDRC");
      if (csrcname != NULL && csrcname[0] != '\0') {
        fd = csound->FileOpen(csound, &csrc, CSFILE_STD, csrcname, "r", NULL);
        if (fd == NULL)
          csoundMessage(csound, Str("WARNING: cannot open csoundrc file %s\n"),
                                csrcname);
      }
      if (fd == NULL && ((home_dir = csoundGetEnv(csound, "HOME")) != NULL &&
                         home_dir[0] != '\0')) {
        /* 11 bytes for DIRSEP, ".csoundrc" (9 chars), and null character */
        s = (char*) mmalloc(csound, (size_t) strlen(home_dir) + (size_t) 11);
        sprintf(s, "%s%c%s", home_dir, DIRSEP, ".csoundrc");
        fd = csound->FileOpen(csound, &csrc, CSFILE_STD, s, "r", NULL);
        mfree(csound, s);
      }
      /* read global .csoundrc file (if exists) */
      if (fd != NULL) {
        readOptions(csound, csrc);
        csound->FileClose(csound, fd);
      }
      /* check for .csoundrc in current directory */
      fd = csound->FileOpen(csound, &csrc, CSFILE_STD, ".csoundrc", "r", NULL);
      if (fd != NULL) {
        readOptions(csound, csrc);
        csound->FileClose(csound, fd);
      }
    }
    /* check for CSD file */
    if (csound->orchname == NULL)
      dieu(csound, Str("no orchestra name"));
    else if (csound->scorename == NULL || csound->scorename[0] == (char) 0) {
      int   tmp = (int) strlen(csound->orchname) - 4;
      if (tmp >= 0 && csound->orchname[tmp] == '.' &&
          (csound->orchname[tmp + 1] | (char) 0x20) == 'c' &&
          (csound->orchname[tmp + 2] | (char) 0x20) == 's' &&
          (csound->orchname[tmp + 3] | (char) 0x20) == 'd') {
        /* FIXME: allow orc/sco/csd name in CSD file: does this work ? */
        csound->orcname_mode = 0;
        csound->Message(csound, "UnifiedCSD:  %s\n", csound->orchname);

        /* Add directory of CSD file to search paths before orchname gets
         * replaced with temp orch name */
        fileDir = csoundGetDirectoryForPath(csound, csound->orchname);
        csoundPrependEnv(csound, "SADIR", fileDir);
        csoundPrependEnv(csound, "SSDIR", fileDir);
        /*csoundPrependEnv(csound, "INCDIR", fileDir);*/
        csoundPrependEnv(csound, "MFDIR", fileDir);
        mfree(csound, fileDir);

        if (!read_unified_file(csound, &(csound->orchname),
                                       &(csound->scorename))) {
          csound->Die(csound, Str("Decode failed....stopping"));
        }

        csdFound = 1;
      }
    }
    /* IV - Feb 19 2005: run a second pass of argdecode so that */
    /* command line options override CSD options */
    /* this assumes that argdecode is safe to run multiple times */
    csound->orcname_mode = 1;           /* ignore orc/sco name */
    argdecode(csound, argc, argv);      /* should not fail this time */
    /* some error checking */
    if (csound->stdin_assign_flg &&
        (csound->stdin_assign_flg & (csound->stdin_assign_flg - 1)) != 0) {
      csound->Die(csound, Str("error: multiple uses of stdin"));
    }
    if (csound->stdout_assign_flg &&
        (csound->stdout_assign_flg & (csound->stdout_assign_flg - 1)) != 0) {
      csound->Die(csound, Str("error: multiple uses of stdout"));
    }
    /* done parsing csoundrc, CSD, and command line options */
    /* if sound file type is still not known, check SFOUTYP */
    if (O->filetyp <= 0) {
      const char  *envoutyp;
      envoutyp = csoundGetEnv(csound, "SFOUTYP");
      if (envoutyp != NULL && envoutyp[0] != '\0') {
        if (strcmp(envoutyp, "AIFF") == 0)
          O->filetyp = TYP_AIFF;
        else if (strcmp(envoutyp, "WAV") == 0 || strcmp(envoutyp, "WAVE") == 0)
          O->filetyp = TYP_WAV;
        else if (strcmp(envoutyp, "IRCAM") == 0)
          O->filetyp = TYP_IRCAM;
        else if (strcmp(envoutyp, "RAW") == 0)
          O->filetyp = TYP_RAW;
        else {
          dieu(csound, Str("%s not a recognised SFOUTYP env setting"),
                       envoutyp);
        }
      }
      else
#if !defined(__MACH__) && !defined(mac_classic)
        O->filetyp = TYP_WAV;   /* default to WAV if even SFOUTYP is unset */
#else
        O->filetyp = TYP_AIFF;  /* ... or AIFF on the Mac */
#endif
    }
    /* everything other than a raw sound file has a header */
    O->sfheader = (O->filetyp == TYP_RAW ? 0 : 1);
    if (O->Linein || O->Midiin || O->FMidiin)
      O->RTevents = 1;
    if (!O->sfheader)
      O->rewrt_hdr = 0;         /* cannot rewrite header of headerless file */
    if (O->sr_override || O->kr_override) {
      if (!O->sr_override || !O->kr_override)
        dieu(csound, Str("srate and krate overrides must occur jointly"));
    }
    if (!O->outformat)                      /* if no audioformat yet  */
      O->outformat = AE_SHORT;              /*  default to short_ints */
    O->sfsampsize = sfsampsize(FORMAT2SF(O->outformat));
    O->informat = O->outformat;             /* informat default */

    if (csound->scorename == NULL || strlen(csound->scorename) == 0) {
      /* No scorename yet */
      char *sconame = get_sconame(csound);
      FILE *scof;
      if (O->RTevents)
        csound->Message(csound, Str("realtime performance using dummy "
                                    "numeric scorefile\n"));
      csoundTmpFileName(csound, sconame, ".sco");   /* Generate score name */
      scof = fopen(sconame, "w");
      fprintf(scof, "f0 42000\n");
      fclose(scof);
      csound->scorename = sconame;
      add_tmpfile(csound, sconame);     /* IV - Feb 03 2005 */
    } else if(!csdFound){
        /* Add directory of SCO file to search paths*/
        fileDir = csoundGetDirectoryForPath(csound, csound->scorename);
        csoundPrependEnv(csound, "SADIR", fileDir);
        csoundPrependEnv(csound, "SSDIR", fileDir);
        /*csoundPrependEnv(csound, "INCDIR", fileDir);*/
        csoundPrependEnv(csound, "MFDIR", fileDir);
        mfree(csound, fileDir);
    }

    /* Add directory of ORC file to search paths*/
    if(!csdFound) {
        fileDir = csoundGetDirectoryForPath(csound, csound->orchname);
        csoundPrependEnv(csound, "SADIR", fileDir);
        csoundPrependEnv(csound, "SSDIR", fileDir);
        /*csoundPrependEnv(csound, "INCDIR", fileDir);*/
        csoundPrependEnv(csound, "MFDIR", fileDir);
        mfree(csound, fileDir);
    }


    csound->Message(csound, Str("orchname:  %s\n"), csound->orchname);
    if (csound->scorename != NULL)
      csound->Message(csound, Str("scorename: %s\n"), csound->scorename);
    if (csound->xfilename != NULL)
      csound->Message(csound, Str("xfilename: %s\n"), csound->xfilename);
    /* IV - Oct 31 2002: moved orchestra compilation here, so that named */
    /* instrument numbers are known at the score read/sort stage */
    csoundLoadExternals(csound);    /* load plugin opcodes */
    /* IV - Jan 31 2005: initialise external modules */
    if (csoundInitModules(csound) != 0)
      csound->LongJmp(csound, 1);
    otran(csound);                  /* read orcfile, setup desblks & spaces */
    /* IV - Jan 28 2005 */
    print_benchmark_info(csound, Str("end of orchestra compile"));
    if (!csoundYield(csound))
      return -1;
    /* IV - Oct 31 2002: now we can read and sort the score */
    if ((n = strlen(csound->scorename)) > 4 &&  /* if score ?.srt or ?.xtr */
        (!strcmp(csound->scorename + (n - 4), ".srt") ||
         !strcmp(csound->scorename + (n - 4), ".xtr"))) {
      csound->Message(csound, Str("using previous %s\n"), csound->scorename);
      playscore = sortedscore = csound->scorename;  /*   use that one */
    }
    else {
      if (csound->keep_tmp) {
        playscore = sortedscore = "score.srt";
      }
      else {
        playscore = sortedscore = csoundTmpFileName(csound, NULL, ".srt");
        add_tmpfile(csound, playscore);         /* IV - Feb 03 2005 */
      }
      if (!(scorin = fopen(csound->scorename, "rb")))   /* else sort it   */
        csoundDie(csound, Str("cannot open scorefile %s"), csound->scorename);
      if (!(scorout = fopen(sortedscore, "w")))
        csoundDie(csound, Str("cannot open %s for writing"), sortedscore);
      csound->Message(csound, Str("sorting score ...\n"));
      scsort(csound, scorin, scorout);
      fclose(scorin);
      fclose(scorout);
    }
    if (csound->xfilename != NULL) {            /* optionally extract */
      if (!strcmp(csound->scorename, "score.xtr"))
        csoundDie(csound, Str("cannot extract %s, name conflict"),
                          csound->scorename);
      if (!(xfile = fopen(csound->xfilename, "r")))
        csoundDie(csound, Str("cannot open extract file %s"),csound->xfilename);
      if (!(scorin = fopen(sortedscore, "r")))
        csoundDie(csound, Str("cannot reopen %s"), sortedscore);
      if (!(scorout = fopen(xtractedscore, "w")))
        csoundDie(csound, Str("cannot open %s for writing"), xtractedscore);
      csound->Message(csound, Str("  ... extracting ...\n"));
      scxtract(csound, scorin, scorout, xfile);
      fclose(scorin);
      fclose(scorout);
      fclose(xfile);
      playscore = xtractedscore;
    }
    csound->Message(csound, Str("\t... done\n"));
    /* copy sorted score name */
    O->playscore = (char*) mmalloc(csound, strlen(playscore) + 1);
    strcpy(O->playscore, playscore);
    /* IV - Jan 28 2005 */
    print_benchmark_info(csound, Str("end of score sort"));

    /* open MIDI output (moved here from argdecode) */
    if (O->Midioutname != NULL && O->Midioutname[0] == (char) '\0')
      O->Midioutname = NULL;
    if (O->FMidioutname != NULL && O->FMidioutname[0] == (char) '\0')
      O->FMidioutname = NULL;
    if (O->Midioutname != NULL || O->FMidioutname != NULL)
      openMIDIout(csound);

    return musmon(csound);
}

