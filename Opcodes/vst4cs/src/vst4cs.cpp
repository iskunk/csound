//  vst4cs: VST HOST OPCODES FOR CSOUND
//
//  Uses code by Hermann Seib from his Vst Host program
//  and from the vst~ object by Thomas Grill,
//  which in turn borrows from the Psycle tracker.
//  VST is a trademark of Steinberg Media Technologies GmbH.
//  VST Plug-In Technology by Steinberg.
//
//  Copyright (C) 2004 Andres Cabrera, Michael Gogins
//
//  The vst4cs library is free software; you can redistribute it
//  and/or modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  The vst4cs library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with The vst4cs library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
//  02111-1307 USA

#ifdef _WIN32 
  #pragma warning(disable:4786) //gab
#endif

#include <cstdlib>
#include <cmath>
#include <vector>
#include <string>
#include "vst4cs.h"
#include "vsthost.h"
#include "fxbank.h"

#include "InOut/widglobals.h" //gab


extern "C" {
  //TODO: 29 replacements
//   std::vector<VSTPlugin *> vstPlugins;
  std::string version = "0.2beta";
#ifdef WIN32
  static void path_convert(char *in);
#endif

  static int vstinit(CSOUND *csound, void *data)
  {
      VSTINIT *p = (VSTINIT *) data;
      VSTPlugin *plugin = new VSTPlugin(csound);

      *p->iVSThandle = (MYFLT) ST(vstPlugins).size();
      ST(vstPlugins).push_back(plugin);
      if ((int) ST(vstPlugins).size() == 1) {
        plugin->Log("=============================================================\n");
        plugin->Log("vst4cs version %s by Andres Cabrera and Michael Gogins\n", version.c_str());
        plugin->Log("Using code from H. Seib's VstHost and T. Grill's vst~ object\n");
        plugin->Log("VST is a trademark of Steinberg Media Technologies GmbH\n");
        plugin->Log("VST Plug-In Technology by Steinberg\n");
        plugin->Log("=============================================================\n");
      }
      char    vstplugname[0x100];
      strcpy(vstplugname, (char *) p->iplugin);
#if WIN32
      path_convert(vstplugname);
#endif
      if (plugin->Instantiate(vstplugname)) {
        csound->InitError(csound, "vstinit: Error loading effect.");
        csound->LongJmp(csound, 1);
      }
      plugin->Init();
      if (*p->iverbose)
        plugin->Info();
      return OK;
  }

  static int vstinfo(CSOUND *csound, void *data)
  {
      VSTINFO *p = (VSTINFO *) data;
      VSTPlugin *plugin = ST(vstPlugins)[(size_t) *p->iVSThandle];

      plugin->Info();
      return OK;
  }

  static int vstaudio_init(CSOUND *csound, void *data)
  {
      VSTAUDIO *p = (VSTAUDIO *) data;

      p->opcodeInChannels = (size_t) (csound->GetInputArgCnt(data) - 1);
      if (p->opcodeInChannels > 32)
        csound->InitError(csound, "vstaudio: too many input args");
      VSTPlugin *plugin = ST(vstPlugins)[(size_t) *p->iVSThandle];
//       if (!plugin) {
//         csound->InitError(csound, "vstaudio_init: invalid plugin pointer!");
//         return NOTOK;
//       }

      plugin->Debug("vstaudio_init.\n");
      p->framesPerBlock = csound->ksmps;
      p->pluginInChannels  = (size_t) plugin->getNumInputs();
      p->pluginOutChannels = (size_t) plugin->getNumOutputs();
      p->opcodeOutChannels = (size_t) csound->GetOutputArgCnt(data);

      return OK;
  }

  static int vstaudio(CSOUND *csound, void *data)
  {
      VSTAUDIO *p = (VSTAUDIO *) data;
      size_t  i, j;
      VSTPlugin *plugin = ST(vstPlugins)[(size_t) *p->iVSThandle];
      // plugin->Debug("vstaudio: plugin %x.\n", plugin);
      if (!p->h.insdshead->nxtact) {
        for (j = 0; j < p->pluginInChannels && j < p->opcodeInChannels; j++)
          for (i = 0; i < p->framesPerBlock; i++)
            plugin->inputs_[j][i] =
              (float) (p->ains[j][i] * csound->dbfs_to_float);
        for ( ; j < p->pluginInChannels; j++)
          for (i = 0; i < p->framesPerBlock; i++)
            plugin->inputs_[j][i] = 0.0f;
        for (j = 0; j < p->pluginOutChannels; j++)
          for (i = 0; i < p->framesPerBlock; i++)
            plugin->outputs_[j][i] = 0.0f;
        plugin->process(&plugin->inputs.front(), &plugin->outputs.front(),
                        p->framesPerBlock);
        for (j = 0; j < p->pluginOutChannels && j < p->opcodeOutChannels; j++)
          for (i = 0; i < p->framesPerBlock; i++)
            p->aouts[j][i] = (MYFLT) plugin->outputs_[j][i] * csound->e0dbfs;
        for ( ; j < p->opcodeOutChannels; j++)
          for (i = 0; i < p->framesPerBlock; i++)
            p->aouts[j][i] = FL(0.0);
      }
      else {
        for (j = 0; j < p->opcodeInChannels && j < p->opcodeOutChannels; j++)
          for (i = 0; i < p->framesPerBlock; i++)
            p->aouts[j][i] = p->ains[j][i];
        for ( ; j < p->opcodeOutChannels; j++)
          for (i = 0; i < p->framesPerBlock; i++)
            p->aouts[j][i] = FL(0.0);
      }
      return OK;
  }

  static int vstaudiog(CSOUND *csound, void *data)
  {
      VSTAUDIO *p = (VSTAUDIO *) data;
      size_t  i, j;
      VSTPlugin *plugin = ST(vstPlugins)[(size_t) *p->iVSThandle];
      // plugin->Debug("vstaudio: plugin %x.\n", plugin);
      for (j = 0; j < p->pluginInChannels && j < p->opcodeInChannels; j++)
        for (i = 0; i < p->framesPerBlock; i++)
          plugin->inputs_[j][i] =
            (float) (p->ains[j][i] * csound->dbfs_to_float);
      for ( ; j < p->pluginInChannels; j++)
        for (i = 0; i < p->framesPerBlock; i++)
          plugin->inputs_[j][i] = 0.0f;
      for (j = 0; j < p->pluginOutChannels; j++)
        for (i = 0; i < p->framesPerBlock; i++)
          plugin->outputs_[j][i] = 0.0f;
      plugin->process(&plugin->inputs.front(), &plugin->outputs.front(),
                      p->framesPerBlock);
      for (j = 0; j < p->pluginOutChannels && j < p->opcodeOutChannels; j++)
        for (i = 0; i < p->framesPerBlock; i++)
          p->aouts[j][i] = (MYFLT) plugin->outputs_[j][i] * csound->e0dbfs;
      for ( ; j < p->opcodeOutChannels; j++)
        for (i = 0; i < p->framesPerBlock; i++)
          p->aouts[j][i] = FL(0.0);

      return OK;
  }

  static int vstnote_init(CSOUND *csound, void *data)
  {
      VSTNOTE *p = (VSTNOTE *) data;

      p->vstHandle = (size_t) *p->iVSThandle;
      VSTPlugin *vstPlugin = ST(vstPlugins)[p->vstHandle];

      int     rounded = (int) ((double) *p->knote + 0.5);
      int     cents = (int) (((double) *p->knote - (double) rounded) * 100.0
                             + 100.5) - 100;
      int     veloc = (int) *p->kveloc;
      double  deltaTime =
        ((double) p->h.insdshead->p2 + csound->timeOffs) - csound->curTime;
      int     deltaFrames =
        (int) (deltaTime * (double) vstPlugin->framesPerSecond
               + (deltaTime < 0.0 ? -0.5 : 0.5));
      long    framesRemaining =
        (long) ((double) *p->kdur * (double) vstPlugin->framesPerSecond + 0.5)
        + (long) deltaFrames;

      p->chn = (int) *p->kchan & 15;
      p->note = (rounded >= 0 ? (rounded < 128 ? rounded : 127) : 0);
      veloc = (veloc >= 0 ? (veloc < 128 ? veloc : 127) : 0);
      if (deltaFrames < 0)
        deltaFrames = 0;
      else if (deltaFrames >= (int) vstPlugin->framesPerBlock)
      deltaFrames = (int) vstPlugin->framesPerBlock - 1;
      if (framesRemaining < 0L)
        framesRemaining = 0L;
      p->framesRemaining = (size_t) framesRemaining;

      vstPlugin->Debug("vstnote_init\n");
      vstPlugin->AddMIDI(144 | p->chn | (p->note << 8) | (veloc << 16),
                       deltaFrames, cents);

      return OK;
  }

  static int vstnote(CSOUND *csound, void *data)
  {
      VSTNOTE *p = (VSTNOTE *) data;

      if (p->framesRemaining >= (size_t) csound->ksmps) {
        p->framesRemaining -= (size_t) csound->ksmps;
        return OK;
      }

      VSTPlugin *plugin = ST(vstPlugins)[p->vstHandle];
      plugin->Debug("vstnote.\n");
      plugin->AddMIDI(144 | p->chn | (p->note << 8), (int) p->framesRemaining, 0);
      p->framesRemaining = ~((size_t) 0);
      return OK;
  }

  static int vstmidiout_init(CSOUND *csound, void *data)
  {
      VSTMIDIOUT *p = (VSTMIDIOUT *) data;
      VSTPlugin *plugin;

      p->vstHandle = (size_t) *p->iVSThandle;
      plugin = ST(vstPlugins)[p->vstHandle];
      plugin->Debug("vstmidiout_init.\n");
      p->prvMidiData = 0;

      return OK;
  }

  static int vstmidiout(CSOUND *csound, void *data)
  {
      VSTMIDIOUT *p = (VSTMIDIOUT *) data;
      VSTPlugin *plugin;
      int     st, ch, d1, d2, midiData;

      st = (int) *(p->kstatus);
      if (st < 128 || st >= 240) {
        p->prvMidiData = 0;
        return OK;
      }
      ch = (int) *(p->kchan) & 15;
      if ((st & 15) > ch)
        ch = st & 15;
      st &= 240;
      d1 = (int) *(p->kdata1);
      if (st != 192 && st != 208) {
        d2 = (int) *(p->kdata2);
        if (st == 224) {
          d2 += ((d1 >> 7) & 127);
          d1 &= 127;
        }
        d2 = (d2 >= 0 ? (d2 < 128 ? d2 : 127) : 0);
      }
      else
        d2 = 0;
      d1 = (d1 >= 0 ? (d1 < 128 ? d1 : 127) : 0);
      midiData = st | ch | (d1 << 8) | (d2 << 16);

      if (midiData == p->prvMidiData)
        return OK;

      p->prvMidiData = midiData;
      plugin = ST(vstPlugins)[p->vstHandle];
      plugin->Debug("vstmidiout. kstatus = %i kdata1 = %i kdata2 = %i--- mididata = %i\n",
                    (int) *(p->kstatus),
                    (int) *(p->kdata1),
                    (int) *(p->kdata2),
                    midiData);
      plugin->AddMIDI(midiData, 0, 0);

      return OK;
  }

  static int vstparamget_init(CSOUND *csound, void *data)
  {
      return OK;
  }

  static int vstparamget(CSOUND *csound, void *data)
  {
      VSTPARAMGET *p = (VSTPARAMGET *) data;
      VSTPlugin *plugin = ST(vstPlugins)[(size_t) *p->iVSThandle];

      plugin->Debug("vstparamset(%d).\n", int(*p->kparam));
      *p->kvalue = plugin->GetParamValue(int(*p->kparam));

      if (*(p->kvalue) == FL(-1.0))
        plugin->Log("Invalid parameter number %d.\n", int(*p->kparam));
      return OK;
  }

  static int vstparamset_init(CSOUND *csound, void *data)
  {
      VSTPARAMSET *p = (VSTPARAMSET *) data;
      VSTPlugin *plugin = ST(vstPlugins)[(size_t) *p->iVSThandle];

      plugin->Debug("vstparamset_init.\n");
      p->oldkparam = 0;
      p->oldkvalue = 0;
      return OK;
  }

  static int vstparamset(CSOUND *csound, void *data)
  {
      VSTPARAMSET *p = (VSTPARAMSET *) data;
      VSTPlugin *plugin = ST(vstPlugins)[(size_t) *p->iVSThandle];

      if (*p->kparam == p->oldkparam && *p->kvalue == p->oldkvalue)
        return OK;
      p->oldkparam = *p->kparam;
      p->oldkvalue = *p->kvalue;
      plugin->Debug("vstsend(%d, %f).\n", int(*p->kparam), *p->kvalue);
      plugin->SetParameter(int(*p->kparam), float(*p->kvalue));

      return OK;
  }
  static int vstbankload(CSOUND *csound, void *data)
  {
      VSTBANKLOAD *p = (VSTBANKLOAD *) data;
      VSTPlugin *plugin = ST(vstPlugins)[(size_t) *p->iVSThandle];
      void    *dummyPointer = 0;

      CFxBank fxBank((char *) p->ibank);          /* load the bank    */

      plugin->Dispatch(effBeginLoadBank,
                       0, 0, (VstPatchChunkInfo *) fxBank.GetChunk(), 0);
      if (plugin->Dispatch(effBeginLoadBank,
                           0, 0, (VstPatchChunkInfo *) fxBank.GetChunk(), 0)) {
        csound->InitError(csound, "Error: BeginLoadBank.");
        return NOTOK;
      }
      // csound->Message(csound, "EffBeginLoadBank\n");
      if (fxBank.IsLoaded()) {
        if (plugin->aeffect->uniqueID != fxBank.GetFxID()) {
          csound->InitError(csound, "Loaded bank ID doesn't match plug-in ID.");
          return NOTOK;
        }
        if (fxBank.IsChunk()) {
          if (!(plugin->aeffect->flags & effFlagsProgramChunks)) {
            csound->InitError(csound, "Loaded bank contains a chunk format "
                              "that the effect cannot handle.");
            return NOTOK;
          }
          plugin->Dispatch(effSetChunk, 0, fxBank.GetChunkSize(),
                           fxBank.GetChunk(), 0); // isPreset = 0
          plugin->Log("Chunks loaded OK.\n");
        }
        else {
          //     int     cProg = plugin->EffGetProgram();
          int     cProg = plugin->Dispatch(effGetProgram, 0, 0, dummyPointer, 0);
          //     csound->Message(csound, "Current Program= %i\n", cProg);
          int     i, j;
          int     nParms = fxBank.GetNumParams();

          //     csound->Message(csound, "nParms= %i\n", nParms);
          for (i = 0; i < fxBank.GetNumPrograms(); i++) {
            plugin->Dispatch(effSetProgram, 0, i, dummyPointer, 0);
            plugin->Dispatch(effSetProgramName, 0, 0, fxBank.GetProgramName(i),
                             0);
            for (j = 0; j < nParms; j++)
              plugin->SetParameter(j, fxBank.GetProgParm(i, j));
          }
          //     pEffect->EffSetProgram(cProg);
          plugin->Dispatch(effSetProgram, 0, cProg, dummyPointer, 0);
          //     csound->Message(csound, "Programs OK\n");
        }
        //   pEffect->SetChunkFile(dlg.GetPathName());
        //   ShowDetails();
        //   OnSetProgram();
      }
      else {
        csound->InitError(csound, "Problem loading bank.");
        return NOTOK;           /* check if error loading */
      }
      plugin->Log("Bank loaded OK.\n");
      // if (fxBank.SetChunk()) {
      // }
      return OK;
  }

  static int vstprogset(CSOUND *csound, void *data)
  {
      // The changes here are part of an attempt to map 0 to 1 and others
      VSTPROGSET *p = (VSTPROGSET *) data;
      int program = (int)*p->iprogram;
      VSTPlugin *plugin = ST(vstPlugins)[(size_t) *p->iVSThandle];
      if (program>16 || program<=0) {
        csound->Message(csound, "VSTprogset: Program %d treated as 1\n", program);
        program = 1;
      }
      plugin->SetCurrentProgram(program);
      return OK;
  }

  static int vstedit_init(CSOUND *csound, void *data)
  {
      VSTEDIT *p = (VSTEDIT *) data;
      VSTPlugin *plugin = ST(vstPlugins)[(size_t) *p->iVSThandle];

        plugin->OpenEditor();
  	plugin->targetFLpanel = ST(fl_windows).size()-1; //gab
	ST(VSTplugEditors).push_back(plugin); //gab
    return OK;
  }

  static int vstSetTempo(CSOUND *csound, void *data)
  {
          VSTTEMPO *p = (VSTTEMPO *)data;
          VSTPlugin *plugin = ST(vstPlugins)[(size_t) *p->iVSThandle];
          plugin->vstTimeInfo.tempo = *p->tempo;
          return OK;
  }

  static int vstout_on_dur_set(CSOUND *csound, VSTOUT_ON_DUR *p) //gab
  {
      int temp;
      //if (MIDIoutDONE==0) openMIDIout();
      if (p->h.insdshead->xtratim < 1)      /* if not initialised by another opcode */
                  p->h.insdshead->xtratim = 1;

      p->chn = (temp = abs((int) *p->ichn-1)) < 16 ? temp : 15;
      p->num = (temp = abs((int) *p->inum)) < 128 ? temp : 127; 
      p->vel = (temp = abs((int) *p->ivel)) < 128 ? temp : 127;

      //note_on(p->chn,p->num, p->vel);
          VSTPlugin *plugin = ST(vstPlugins)[(size_t) *p->iVSThandle];
      plugin->AddMIDI( 0x90 + ((int) *p->ichn&0x0F), (int) *p->inum, (int) *p->ivel);
      p->istart_time = (MYFLT) csound->kcounter * csound->onedkr;
      p->fl_expired = false;
      p->fl_extra_dur = false;
          return OK;
  }


  static int vstout_on_dur(CSOUND *csound, VSTOUT_ON_DUR *p) //gab
  {
      if (!(p->fl_expired)) {
        MYFLT actual_dur = (MYFLT) csound->kcounter * csound->onedkr - p->istart_time;
        MYFLT dur = *p->idur;
        if (dur < actual_dur) {	
                VSTPlugin *plugin = ST(vstPlugins)[(size_t) *p->iVSThandle];
                p->fl_expired = true;
                //note_off(p->chn, p->num, p->vel);
                plugin->AddMIDI( 0x80 + ( p->chn&0x0F), p->num, p->vel);
        }
        else if (p->h.insdshead->relesing) {
                VSTPlugin *plugin = ST(vstPlugins)[(size_t) *p->iVSThandle];
                p->fl_expired = true;
                //note_off(p->chn, p->num, p->vel);
                plugin->AddMIDI( 0x80 + ( p->chn&0x0F), p->num, p->vel);
        }
      }
      return OK;
  }


  int vstbanksave(CSOUND *csound, void *data)
  {
          VSTBANKLOAD *p = (VSTBANKLOAD *)data;
          VSTPlugin *plugin = ST(vstPlugins)[(size_t) *p->iVSThandle];
          char bankname[512]; //gab
  //	if (*p->ibank == SSTRCOD) {
                  strcpy(bankname, (char *) p->ibank);          /*   use that         */
  //	}

  //------------
          //CSmpEffect *pEffect = (CSmpEffect *)GetApp()->vstHost.GetAt(nEffect);
          if (!plugin)
                  return NOTOK;

          CFxBank b;

          if (plugin->aeffect->flags & effFlagsProgramChunks)
          {
                  void * pChunk;
                  int lSize = plugin->EffGetChunk(&pChunk);
                  if (lSize)
                          b.SetSize(lSize);
                  if (b.IsLoaded())
                          b.SetChunk(pChunk);
          }
          else
          {
                  b.SetSize(plugin->aeffect->numPrograms,
              plugin->aeffect->numParams);
                  if (b.IsLoaded())
                  {
                          int i, j;
                          int cProg = plugin->EffGetProgram();
                          int nParms = b.GetNumParams();
                          for (i = 0; i < b.GetNumPrograms(); i++)
                          {
                                  plugin->EffSetProgram(i);
                                  char szName[128];
                                  plugin->EffGetProgramName(szName);
                                  b.SetProgramName(i, szName);
                                  for (j = 0; j < nParms; j++)
                                          b.SetProgParm(i, j, plugin->aeffect->getParameter(plugin->aeffect,j));
                          }
                          plugin->EffSetProgram(cProg);
                  }
          }
          if (!b.IsLoaded())
          {
                  plugin->Log("Error: Memory Allocation Error.\n");
                  return NOTOK;
          }
          b.SetFxID(plugin->aeffect->uniqueID);
          b.SetFxVersion(plugin->aeffect->version);
          if (b.SaveBank(bankname))
          {
                  plugin->Log("%s Bank saved OK.\n",bankname);
          //pEffect->SetChunkFile(bankname);
          //	ShowDetails();
  //		OnSetProgram();
          }
          else {
                  plugin->Log("Error: Error saving file\n");
                  return NOTOK;
          }
          return OK;
  }


#ifdef WIN32
  static void path_convert(char *in)
  {
      for (int i = 0; in[i] != '\0'; i++) {
        if (in[i] == '/')
          in[i] = '\\';
      }
  }
#endif




  static OENTRY localops[] = {
      { "vstinit",    sizeof(VSTINIT), 1, "i", "So", &vstinit, 0, 0 },
      { "vstinfo",    sizeof(VSTINFO), 1, "", "i", &vstinfo, 0, 0 },
      { "vstaudio",   sizeof(VSTAUDIO), 5, "mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm", "iy", &vstaudio_init, 0, &vstaudio },
      { "vstaudiog",  sizeof(VSTAUDIO), 5, "mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm", "iy", &vstaudio_init, 0, &vstaudiog },
      { "vstnote",    sizeof(VSTNOTE), 3, "", "iiiii", &vstnote_init, &vstnote, 0 },
      { "vstmidiout", sizeof(VSTMIDIOUT), 3, "", "ikkkk", &vstmidiout_init, &vstmidiout, 0 },
      { "vstparamget",sizeof(VSTPARAMGET), 3, "k", "ik", &vstparamget_init, &vstparamget, 0 },
      { "vstparamset",sizeof(VSTPARAMSET), 3, "", "ikk", &vstparamset_init, &vstparamset, 0 },
      { "vstbankload",sizeof(VSTBANKLOAD), 1, "", "iS", &vstbankload, 0, 0 },
      { "vstprogset", sizeof(VSTPROGSET), 1, "", "ii", &vstprogset, 0, 0 },
      { "vstedit",    sizeof(VSTEDIT), 1, "", "i", &vstedit_init, 0, 0 },
      { "vsttempo",    sizeof(VSTTEMPO), 2,"" ,"ki",  0,  (SUBR)vstSetTempo,         0/*,          &vstedit_deinit*/},
      { "vstnoteondur",sizeof(VSTOUT_ON_DUR),3,"" ,  "iiiii",  (SUBR)vstout_on_dur_set, (SUBR)vstout_on_dur,     0 },
      { "vstbanksave", sizeof(VSTBANKLOAD), 1, "" ,  "iS",    vstbanksave,      0,           0/*, 0        */},
      { NULL, 0, 0, NULL, NULL, (SUBR) NULL, (SUBR) NULL, (SUBR) NULL }
  };

  PUBLIC int csoundModuleCreate(CSOUND *csound)
  {
      return 0;
  }

  PUBLIC int csoundModuleInit(CSOUND *csound)
  {
      OENTRY  *ep = (OENTRY *) &(localops[0]);
      int     err = 0;
      while (ep->opname != NULL) {
        err |= csound->AppendOpcode(csound,
                        ep->opname, ep->dsblksiz, ep->thread,
                        ep->outypes, ep->intypes,
                        (int (*)(CSOUND *, void *)) ep->iopadr,
                        (int (*)(CSOUND *, void *)) ep->kopadr,
                        (int (*)(CSOUND *, void *)) ep->aopadr);
        ep++;
      }
      return err;
  }

  PUBLIC int csoundModuleDestroy(CSOUND *csound)
  {
      size_t  i, n = 0;
//       TODO: This causes segfault if no vst plugins were allocated
      if (csound->widgetGlobals) {
        for (i = 0; i < ST(vstPlugins).size(); i++) {
          if (ST(vstPlugins)[i] != (VSTPlugin *) 0) {
            n = i + 1;
//             vstPlugins is not global so this should not longer be needed:
            if (ST(vstPlugins)[i]->csound == csound) {
              delete ST(vstPlugins)[i];
              ST(vstPlugins)[i] = (VSTPlugin *) 0;
            }
          }
        }
        if (n < i)
          ST(vstPlugins).resize(n);
      }
      ST(vstPlugins).clear();
      return 0;
  }

}       // extern "C"
