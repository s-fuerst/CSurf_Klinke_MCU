#ifndef _CSURF_H_
#define _CSURF_H_

#include "reaper_plugin.h"
#include "db2val.h"
#include "wdlstring.h"
#include <stdio.h>
#include "resource.h"

extern REAPER_PLUGIN_HINSTANCE g_hInst; // used for dialogs
extern HWND g_hwnd;
/* 
** Calls back to REAPER (all validated on load)
*/
extern double (*DB2SLIDER)(double x);
extern double (*SLIDER2DB)(double y);
extern int (*GetNumMIDIInputs)(); 
extern int (*GetNumMIDIOutputs)();
extern int (*GetNumAudioInputs)(); 
extern int (*GetNumAudioOutputs)();
extern midi_Input *(*CreateMIDIInput)(int dev);
extern midi_Output *(*CreateMIDIOutput)(int dev, bool streamMode, int *msoffset100); 
extern bool (*GetMIDIOutputName)(int dev, char *nameout, int nameoutlen);
extern bool (*GetMIDIInputName)(int dev, char *nameout, int nameoutlen);


extern int (*CSurf_TrackToID)(MediaTrack *track, bool mcpView);
extern MediaTrack *(*CSurf_TrackFromID)(int idx, bool mcpView);
extern int (*CSurf_NumTracks)(bool mcpView);

// these will be called from app when something changes
extern void (*CSurf_SetTrackListChange)();
extern void (*CSurf_SetSurfaceVolume)(MediaTrack *trackid, double volume, IReaperControlSurface *ignoresurf);
extern void (*CSurf_SetSurfacePan)(MediaTrack *trackid, double pan, IReaperControlSurface *ignoresurf);
extern void (*CSurf_SetSurfaceMute)(MediaTrack *trackid, bool mute, IReaperControlSurface *ignoresurf);
extern void (*CSurf_SetSurfaceSelected)(MediaTrack *trackid, bool selected, IReaperControlSurface *ignoresurf);
extern void (*CSurf_SetSurfaceSolo)(MediaTrack *trackid, bool solo, IReaperControlSurface *ignoresurf);
extern void (*CSurf_SetSurfaceRecArm)(MediaTrack *trackid, bool recarm, IReaperControlSurface *ignoresurf);
extern bool (*CSurf_GetTouchState)(MediaTrack *trackid, int isPan);
extern void (*CSurf_SetAutoMode)(int mode, IReaperControlSurface *ignoresurf);

extern void (*CSurf_SetPlayState)(bool play, bool pause, bool rec, IReaperControlSurface *ignoresurf);
extern void (*CSurf_SetRepeatState)(bool rep, IReaperControlSurface *ignoresurf);

// these are called by our surfaces, and actually update the project
extern double (*CSurf_OnSendVolumeChange)(MediaTrack* trackid, int send_index, double volume, bool relative);
extern double (*CSurf_OnVolumeChange)(MediaTrack *trackid, double volume, bool relative);
extern double (*CSurf_OnPanChange)(MediaTrack *trackid, double pan, bool relative);
extern bool (*CSurf_OnMuteChange)(MediaTrack *trackid, int mute);
extern bool (*CSurf_OnSelectedChange)(MediaTrack *trackid, int selected);
extern bool (*CSurf_OnSoloChange)(MediaTrack *trackid, int solo);
extern bool (*CSurf_OnFXChange)(MediaTrack *trackid, int en);
extern bool (*CSurf_OnRecArmChange)(MediaTrack *trackid, int recarm);
extern void (*CSurf_OnPlay)();
extern void (*CSurf_OnStop)();
extern void (*CSurf_OnFwd)(int seekplay);
extern void (*CSurf_OnRew)(int seekplay);
extern void (*CSurf_OnRecord)();
extern void (*CSurf_GoStart)();
extern void (*CSurf_GoEnd)();
extern void (*CSurf_OnArrow)(int whichdir, bool wantzoom);
extern void (*CSurf_OnTrackSelection)(MediaTrack *trackid);
extern void (*CSurf_ResetAllCachedVolPanStates)();
extern void (*CSurf_ScrubAmt)(double amt);

extern void (*kbd_OnMidiEvent)(MIDI_event_t *evt, int dev_index);
extern const char *(*GetTrackInfo)(INT_PTR track, int *flags); 
extern int (*GetMasterMuteSoloFlags)();
extern void (*TrackList_UpdateAllExternalSurfaces)();
extern void (*MoveEditCursor)(double adjamt, bool dosel);
extern void (*adjustZoom)(double amt, int forceset, bool doupd, int centermode); // 0,true,-1 are defaults
extern double (*GetHZoomLevel)(); // returns pixels/second
extern void (*ClearAllRecArmed)();
extern void (*SetTrackAutomationMode)(MediaTrack *tr, int mode);
extern int (*GetTrackAutomationMode)(MediaTrack *tr);
extern void (*SoloAllTracks)(int solo); // solo=2 for SIP
extern void (*MuteAllTracks)(bool mute);
extern void (*BypassFxAllTracks)(int bypass); // -1 = bypass all if not all bypassed, otherwise unbypass all
extern void (*SetTrackSelected)(MediaTrack *tr, bool sel);
extern int (*GetPlayState)();
extern double (*GetPlayPosition)();
extern double (*GetCursorPosition)();
extern void (*format_timestr_pos)(double tpos, char *buf, int buflen, int modeoverride); // modeoverride=-1 for proj
extern void (*UpdateTimeline)(void);

extern int (*GetSetRepeat)(int val);



extern void (*SetAutomationMode)(int mode, bool onlySel);
extern void (*Main_UpdateLoopInfo)(int ignoremask);

extern double (*TimeMap_timeToBeats)(double tpos, int *measures, int *cml, double *fullbeats, int *cdenom);
extern double (*TimeMap2_timeToBeats)(ReaProject*__proj, double tpos, int *measures, int *cml, double *fullbeats, int *cdenom);

extern double (*Track_GetPeakInfo)(MediaTrack *tr, int chidx);
extern bool (*GetTrackUIVolPan)(MediaTrack *tr, double *vol, double *pan);
extern void (*mkvolpanstr)(char *str, double vol, double pan);
extern void (*mkvolstr)(char *str, double vol);
extern void (*mkpanstr)(char *str, double pan);

extern int (*TrackFX_GetCount)(MediaTrack *tr);
extern int (*TrackFX_GetNumParams)(MediaTrack *tr, int fx);
extern double (*TrackFX_GetParam)(MediaTrack *tr, int fx, int param, double *minval, double *maxval);
extern bool (*TrackFX_SetParam)(MediaTrack *tr, int fx, int param, double val);
extern bool (*TrackFX_GetParamName)(MediaTrack *tr, int fx, int param, char *buf, int buflen);
extern bool (*TrackFX_FormatParamValue)(MediaTrack *tr, int fx, int param, double val, char *buf, int buflen);
extern bool (*TrackFX_GetFXName)(MediaTrack *tr, int fx, char *buf, int buflen);
extern GUID *(*GetTrackGUID)(MediaTrack *tr);

extern int *g_config_csurf_rate,*g_config_zoommode;
extern int __g_projectconfig_timemode2, __g_projectconfig_timemode;
extern int __g_projectconfig_timeoffs;
extern int __g_projectconfig_measoffs;

// needed for additional MCU support (Klinke)
extern bool (*SetTrackSendUIVol)(MediaTrack* track, int send_idx, double vol, int isend);
extern void *(*GetSetTrackSendInfo)(MediaTrack *tr, int category, int sendidx, const char *parmname, void *setNewValue);
extern void *(*GetSetMediaTrackInfo)(MediaTrack *tr, const char *parmname, void *setNewValue);
extern int (*EnumProjectMarkers)(int idx, bool *isrgn, double *pos, double *rgnend, 
                          char **name, int *markrgnindexnumber);
extern void (*GetSet_LoopTimeRange)(bool isSet, bool isLoop, double *start, double *end, bool allowautoseek);
extern void (*SetEditCurPos)(double time, bool moveview, bool seekplay);
extern void (*Undo_OnStateChangeEx)(const char *descchange, int whichStates, int trackparm); // trackparm=-1 by default, or if updating one fx chain, you can specify track index
extern void (*Undo_BeginBlock)(); // call to start a new "block"
extern void (*Undo_EndBlock)(const char *descchange, int extraflags); // call to end the block, with extra flags if any, and a description
extern double (*TimeMap_timeToQN)(double time);
extern double (*TimeMap_QNToTime)(double qn);
extern void (*GetProjectTimeSignature)(double *bpm, double *bpi);
extern void (*TrackList_AdjustWindows)(bool isMajor);
extern char* (*GetSetObjectState)(void* obj, char* str); 
extern void (*FreeHeapPtr)(void* ptr);
extern int (*projectconfig_var_getoffs)(const char *name, int *szout);
extern void * (*projectconfig_var_addr)(ReaProject *proj, int idx);
extern int (*GetNumTracks)();
extern void (*guidToString)(GUID* g, char* dest);
extern void (*stringToGuid)(const char* str, GUID* g);


// returns index of effect visible in chain, or -1 for chain hidden, or -2 for chain visible but no effect selected
extern int (*TrackFX_GetChainVisible)(MediaTrack* tr);
// returns HWND of floating window for effect index, if any
extern HWND (*TrackFX_GetFloatingWindow)(MediaTrack* tr, int  index);
// showflag=0 for hidechain, =1 for show chain(index valid), =2 for hide floating window(index valid), =3 for show floating window (index valid)
extern void (*TrackFX_Show)(MediaTrack* tr, int index, int showFlag);

extern GUID* (*TrackFX_GetFXGUID)(MediaTrack* tr, int fx);

extern MediaTrack* (*GetMasterTrack)(ReaProject* proj);

extern int (*AddProjectMarker)(ReaProject* proj, bool isrgn, double pos, double rgnend, const char* name, int wantidx);
extern bool (*DeleteProjectMarker)(void* proj, int markrgnindexnumber, bool isrgn);

#define NUM_MAX_CHANNELS 32
/*
** REAPER command message defines
*/

#define ID_TOGGLE_MASTER_MUTE              14
#define IDC_REPEAT                       1068
#define ID_FILE_SAVEAS                  40022
#define ID_FILE_NEWPROJECT              40023
#define ID_FILE_OPENPROJECT             40025
#define ID_FILE_SAVEPROJECT             40026
#define IDC_EDIT_UNDO                   40029
#define IDC_EDIT_REDO                   40030
#define ID_GOTO_PROJECT_END             40043
#define ID_RECORD_MODE_TIME             40076
#define ID_TOGGLE_SHOW_MIXER            40078
#define ID_TOGGLE_TRACK_ZOOM_MAX_HEIGHT 40113
#define ID_MARKER_PREV                  40172
#define ID_MARKER_NEXT                  40173
#define ID_INSERT_MARKERRGN             40174
#define ID_INSERT_MARKER                40157
#define ID_GOTO_MARKER1                 40161
#define ID_LOOP_SETSTART                40222
#define ID_LOOP_SETEND                  40223
#define ID_RECORD_MODE_NORMAL           40252
#define ID_RECORD_MODE_ITEM             40253
#define ID_VIEW_FX_CHAIN                40291
#define ID_ZOOM_OUT_PROJECT             40295
#define ID_METRONOME                    40364
#define ID_TOGGLE_SHOW_MASTER_RIGHT     40389
#define ID_TOGGLE_SHOW_SEND             40557
#define ID_SET_MARKER1                  40657
#define ID_STOP_AND_SAVE_MEDIA          40667
#define ID_STOP_AND_DELETE_MEDIA        40668
#define ID_TOUCH_SELECTED_TRACK         40914
#define ID_TOGGLE_MASTER_TRACK_TCP      41050

// Reaper track automation modes
enum AutoMode {
  AUTO_MODE_TRIM,
  AUTO_MODE_READ,
  AUTO_MODE_TOUCH,
  AUTO_MODE_WRITE,
  AUTO_MODE_LATCH,
};
extern midi_Output *CreateThreadedMIDIOutput(midi_Output *output); // returns null on null

#endif
