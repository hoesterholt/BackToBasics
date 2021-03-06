#include <playlist/playlist_player.h>
#include <audio/audio.h>
#include <playlist/playlist.h>
#include <elementals.h>
#include <i18n/i18n.h>
#include <pthread.h>

/****************************************************************************************
 * playlist player state fifo.
 ****************************************************************************************/

static playlist_player_cmd_t* copycmd(playlist_player_cmd_t* cmd)
{
  playlist_player_cmd_t* ncmd=(playlist_player_cmd_t*) mc_malloc(sizeof(playlist_player_cmd_t));
  ncmd->cmd = cmd->cmd;
  ncmd->data = cmd->data;
  return ncmd;
}

static void destroycmd(playlist_player_cmd_t* cmd)
{
  mc_free(cmd);
}

IMPLEMENT_FIFO(playlist_player_command_fifo, playlist_player_cmd_t, copycmd, destroycmd);

static void post_command(playlist_player_t* plp, playlist_player_cmd_enum cmd, void* data)
{
  playlist_player_cmd_t c = { cmd, data };
  log_debug3("posting %d, %p\n", cmd, data);
  playlist_player_command_fifo_enqueue(plp->player_control, &c);
}

/****************************************************************************************
 * Implementation of playlist player.
 ****************************************************************************************/

void* playlist_player_thread(void*);
 
playlist_player_t* playlist_player_new(void) 
{
  playlist_player_t* plp = (playlist_player_t*) mc_malloc(sizeof(playlist_player_t));
  plp->worker = media_new();
  plp->current_track = -1;
  plp->current_position_in_ms = 0;
  plp->track_position_in_ms = -1;
  int i;
  for(i = 0; i < PLAYLIST_PLAYER_MAX_PRESETS; ++i) {
    plp->preset_positions_in_ms[i] = -1;
  }
  plp->playlist_hash = -1;
  plp->player_control = playlist_player_command_fifo_new();
  plp->playlist = playlist_new(NULL,_("Untitled"));
  plp->mutex=(pthread_mutex_t*) mc_malloc(sizeof(pthread_mutex_t));
  plp->player_state = PLAYLIST_PLAYER_STOPPED;
  plp->repeat = PLP_NO_REPEAT;
  plp->volume_percentage = 100.0;
  pthread_mutex_init(plp->mutex,NULL);
  int thread_id = pthread_create(&plp->playlist_player_thread, NULL, playlist_player_thread, plp);
  return plp;
}

void playlist_player_destroy(playlist_player_t* plp) 
{
  post_command(plp, PLP_CMD_DESTROY, NULL);
  pthread_join(plp->playlist_player_thread, NULL);
  log_debug("joined");
  
  media_destroy(plp->worker);
  playlist_destroy(plp->playlist);
  playlist_player_command_fifo_destroy(plp->player_control);
  pthread_mutex_destroy(plp->mutex);
  mc_free(plp->mutex);
  mc_free(plp);
}

long long playlist_player_get_hash(playlist_player_t* plp)
{
  return plp->playlist_hash;
}

void playlist_player_set_playlist(playlist_player_t* plp, playlist_t* pl)
{
  plp->playlist_hash = playlist_tracks_hash(pl);
  plp->current_track = -1;
  post_command(plp, PLP_CMD_SET_PLAYLIST, (void*) pl);
}


void playlist_player_play(playlist_player_t* plp)
{
  post_command(plp, PLP_CMD_PLAY, NULL);
}

void playlist_player_set_preset(playlist_player_t* plp, int preset_number)
{
  log_debug3("set preset %d to position = %ld ms", preset_number, plp->track_position_in_ms);
  track_t* t = playlist_player_get_track(plp);
  long ms = plp->track_position_in_ms;
  plp->preset_positions_in_ms[preset_number] = ms; 
  track_set_preset(t, preset_number, ms);
}

void playlist_player_clear_preset(playlist_player_t* plp, int preset_number)
{
  log_debug2("clear preset %d", preset_number);
  track_t* t = playlist_player_get_track(plp);
  plp->preset_positions_in_ms[preset_number] = -1;
  track_set_preset(t, preset_number, -1);
}

void playlist_player_seek_preset(playlist_player_t* plp, int preset_number)
{
  if (plp->preset_positions_in_ms[preset_number] >= 0) {
    log_debug3("seeking to preset %d (%ld ms)", preset_number, plp->preset_positions_in_ms[preset_number]);
    playlist_player_seek(plp, plp->preset_positions_in_ms[preset_number]);
  }
}

el_bool playlist_player_preset_set(playlist_player_t* player, int preset_number) 
{
  return player->preset_positions_in_ms[preset_number] >= 0;
}

void playlist_player_pause(playlist_player_t* plp)
{
  post_command(plp, PLP_CMD_PAUSE, NULL);
}

void playlist_player_set_track(playlist_player_t* plp, int track)
{
  int* tr = (int*) mc_malloc(sizeof(int));
  *tr = track;
  post_command(plp, PLP_CMD_SET_TRACK, (void*) tr);
}

void playlist_player_next(playlist_player_t* plp)
{
  post_command(plp, PLP_CMD_NEXT, NULL);
}

void playlist_player_previous(playlist_player_t* plp)
{
  post_command(plp, PLP_CMD_PREVIOUS, NULL);
}

void playlist_player_seek(playlist_player_t* plp, long position_in_ms)
{
  log_debug2("Seeking to position %ld", position_in_ms);
  long* pos = (long*) mc_malloc(sizeof(long));
  *pos = position_in_ms;
  post_command(plp, PLP_CMD_SEEK, (void*) pos); 
}

void playlist_player_again(playlist_player_t* plp) 
{
  post_command(plp, PLP_CMD_RESTART_TRACK, NULL);
}


double playlist_player_get_volume(playlist_player_t* plp)
{
  return plp->volume_percentage;
}

void playlist_player_set_volume(playlist_player_t* plp, double percentage)
{
  plp->volume_percentage = percentage;
  post_command(plp, PLP_CMD_SET_VOLUME, &plp->volume_percentage);
}


void playlist_player_set_repeat(playlist_player_t* plp, playlist_player_repeat_t r)
{
  playlist_player_cmd_enum cmd = PLP_CMD_NO_REPEAT;
  if (r == PLP_TRACK_REPEAT) { cmd = PLP_CMD_TRACK_REPEAT; }
  else if (r == PLP_LIST_REPEAT) { cmd = PLP_CMD_LIST_REPEAT; }
  post_command(plp, cmd, NULL);
}

playlist_player_repeat_t playlist_player_get_repeat(playlist_player_t* plp)
{
  playlist_player_repeat_t rep;
  pthread_mutex_lock(plp->mutex);
  rep = plp->repeat;
  pthread_mutex_unlock(plp->mutex);
  return rep;
}

int playlist_player_get_track_index(playlist_player_t* plp) 
{
  int result;
  pthread_mutex_lock(plp->mutex);
  result = plp->current_track;
  pthread_mutex_unlock(plp->mutex);
  return result;
}

track_t* playlist_player_get_track(playlist_player_t* plp)
{
  int i = playlist_player_get_track_index(plp);
  if (i >= 0) {
    return playlist_get(plp->playlist, i);
  } else {
    return NULL;
  }
}

long playlist_player_get_current_position_in_ms(playlist_player_t* plp)
{
  long result;
  pthread_mutex_lock(plp->mutex);
  result = plp->current_position_in_ms;
  pthread_mutex_unlock(plp->mutex);
  return result;
}

long playlist_player_get_track_position_in_ms(playlist_player_t* plp)
{
  long result;
  pthread_mutex_lock(plp->mutex);
  result = plp->track_position_in_ms;
  pthread_mutex_unlock(plp->mutex);
  return result;
}


el_bool playlist_player_get_track_changed(playlist_player_t* plp)
{
  el_bool result;
  pthread_mutex_lock(plp->mutex);
  result = plp->track_changed;
  plp->track_changed = el_false;
  pthread_mutex_unlock(plp->mutex);
  return result;
}

playlist_t* playlist_player_get_playlist(playlist_player_t *plp) {
  return plp->playlist;
}

el_bool playlist_player_is_playing(playlist_player_t* plp) {
  return plp->player_state == PLAYLIST_PLAYER_PLAYING;
}

el_bool playlist_player_is_paused(playlist_player_t* plp) {
  return plp->player_state == PLAYLIST_PLAYER_PAUSED;
}

el_bool playlist_player_does_nothing(playlist_player_t* plp) {
  return plp->player_state == PLAYLIST_PLAYER_STOPPED;
}

/****************************************************************************************
 * playlist player thread
 ****************************************************************************************/
 
void set_presets_from_track(playlist_player_t* plp, track_t* t) 
{
  int i;
  log_debug("Setting presets from track");
  for(i = 0; i < PLAYLIST_PLAYER_MAX_PRESETS; ++i) {
    long ms = track_get_preset(t, i);
    plp->preset_positions_in_ms[i] = ms;
    log_debug3("preset %d = %ld", i, ms);
  }
}

void load_and_play(playlist_player_t* plp, track_t* t)
{
  log_debug2("track = %p", t);
  log_debug2("loadandplay: %s", track_get_id(t));
  pthread_mutex_lock(plp->mutex);
  plp->player_state = PLAYLIST_PLAYER_PLAYING;
  pthread_mutex_unlock(plp->mutex);
  plp->current_position_in_ms = 0;
  plp->track_position_in_ms = 0;
  if (track_get_is_file(t)) {
    media_load_file(plp->worker, track_get_file(t));
    if (track_get_begin_offset_in_ms(t) >= 0) {
      log_debug("seeking");
      media_seek(plp->worker, track_get_begin_offset_in_ms(t));
      if (track_get_end_offset_in_ms(t) >= 0) {
        media_guard(plp->worker, track_get_end_offset_in_ms(t));
      }
    }
    media_set_volume(plp->worker, plp->volume_percentage);
    media_play(plp->worker);
  } else {
    media_load_url(plp->worker, track_get_url(t));
    media_set_volume(plp->worker, plp->volume_percentage);
    media_play(plp->worker);
  }
  log_debug("loadandplay");
  set_presets_from_track(plp, t);
}

void seek_and_guard(playlist_player_t* plp, track_t* t)
{
  if (track_get_begin_offset_in_ms(t) >= 0) {
    media_seek(plp->worker, track_get_begin_offset_in_ms(t));
    if (track_get_end_offset_in_ms(t) >= 0) {
      media_guard(plp->worker, track_get_end_offset_in_ms(t));
    } 
    set_presets_from_track(plp, t);  
  }
}

void guard(playlist_player_t* plp, track_t* t)
{
  if (track_get_end_offset_in_ms(t) >= 0) {
    media_guard(plp->worker, track_get_end_offset_in_ms(t));
    set_presets_from_track(plp, t);
  }
}

void proces_next(playlist_player_t* plp, el_bool force)
{
  pthread_mutex_lock(plp->mutex);
  int next_index = plp->current_track + 1;
  if (next_index >= playlist_count(plp->playlist)) {
    plp->player_state = PLAYLIST_PLAYER_STOPPED;
    plp->current_track = 0;
    media_pause(plp->worker);
    pthread_mutex_unlock(plp->mutex);
  } else {
    // check if tracks are of same file and end_offset = next.begin_offset
    track_t* t_current = playlist_get(plp->playlist, plp->current_track);
    track_t* t_next = playlist_get(plp->playlist, next_index);
    log_debug3("current: %p, next %p", t_current, t_next);
    plp->current_track = next_index;
    plp->track_changed = el_true;
    pthread_mutex_unlock(plp->mutex);
    const char* c_fu = (track_get_is_file(t_current)) ? track_get_file(t_current) 
                                                      : track_get_url(t_current);
    const char* n_fu = (track_get_is_file(t_next)) ? track_get_file(t_next)
                                                   : track_get_url(t_next);
                                          
    if (strcmp(c_fu, n_fu) == 0) {
      log_debug3("%d - next %d", track_get_end_offset_in_ms(t_current), track_get_begin_offset_in_ms(t_next));
      if (force) {
        seek_and_guard(plp, t_next);
      } else if ((track_get_begin_offset_in_ms(t_next) - track_get_end_offset_in_ms(t_current)) <= 1) {
        // do nothing, tracks follow each other 
        // only notify that the track has changed.
        // and guard the end.
        log_debug("Guarding next position");
        guard(plp, t_next);
        //seek_and_guard(plp, t_next);
      } else {
        // seek in the current file and set a new guard
        log_debug("Seeking to next");
        seek_and_guard(plp, t_next);
      }
    } else {
      // load an other
      log_debug("Loading a new track");
      load_and_play(plp, t_next);
    }
  }
}


void proces_media_event(playlist_player_t* plp, audio_event_t* aevent) {
  
  audio_state_t state = aevent->state;
  long pos_in_ms = aevent->position_in_ms;

  pthread_mutex_lock(plp->mutex);
  plp->current_position_in_ms = pos_in_ms;
  if (plp->current_track < playlist_count(plp->playlist) &&
          plp->current_track >= 0) {
    track_t* trk = playlist_get(plp->playlist, plp->current_track);
    long begin_offset = track_get_begin_offset_in_ms(trk);
    if (begin_offset >= 0) {
      plp->track_position_in_ms = pos_in_ms - begin_offset; 
    } else {
      plp->track_position_in_ms = pos_in_ms;
    }
  } else {
    plp->track_position_in_ms = pos_in_ms;
  }
  
  switch (state) {
    case AUDIO_GUARD_REACHED:
    case AUDIO_EOS: {
      log_debug2("repeat = %d", plp->repeat);
      if (plp->repeat == PLP_TRACK_REPEAT) {
        track_t* trk = playlist_get(plp->playlist, plp->current_track);
        pthread_mutex_unlock(plp->mutex);
        load_and_play(plp, trk);
      } else if (plp->repeat == PLP_LIST_REPEAT) {
        if (playlist_count(plp->playlist) > 0) {
          if (plp->current_track == (playlist_count(plp->playlist) - 1)) {
            track_t* trk0 = playlist_get(plp->playlist, 0);
            plp->current_track = 0;
            plp->current_position_in_ms = 0;
            pthread_mutex_unlock(plp->mutex);
            load_and_play(plp, trk0);
          } else {
            pthread_mutex_unlock(plp->mutex);
            proces_next(plp, el_false);
          }
        } else {
          pthread_mutex_unlock(plp->mutex);
        }
      } else {
        pthread_mutex_unlock(plp->mutex);
        proces_next(plp, el_false);
      }
    }
    break;
    default:
      pthread_mutex_unlock(plp->mutex);
    break;
  }
}

 
void* playlist_player_thread(void* _plp)
{
  playlist_player_t* plp = (playlist_player_t*) _plp;
  
  playlist_player_cmd_t *event;
  playlist_player_cmd_enum cmd;
  void* data;

  el_bool destroy = el_false;
  while (!destroy) {
    
    if (plp->player_state != PLAYLIST_PLAYER_PLAYING) { 
      event = playlist_player_command_fifo_dequeue(plp->player_control);
      cmd = event->cmd;
      data = event->data;
      destroycmd(event);
    } else {
      if (playlist_player_command_fifo_peek(plp->player_control)!=NULL) {
        event = playlist_player_command_fifo_dequeue(plp->player_control);
        cmd = event->cmd;
        data = event->data;
        destroycmd(event);
      } else {
        cmd = PLP_CMD_NONE;
        data = NULL;
      }
    }
    
    if (cmd != PLP_CMD_NONE) log_debug3("Command = %d, %p\n", cmd, data);
    
    switch (cmd) {
      case PLP_CMD_DESTROY: {
        destroy = el_true;
      }
      break;
      case PLP_CMD_PLAY: {
        pthread_mutex_lock(plp->mutex);
        if (plp->player_state != PLAYLIST_PLAYER_PLAYING) {
          if (plp->player_state == PLAYLIST_PLAYER_PAUSED) {
            plp->player_state = PLAYLIST_PLAYER_PLAYING;
            pthread_mutex_unlock(plp->mutex);
            media_play(plp->worker);
          } else {
            if (plp->current_track < 0) { plp->current_track = 0; }
            if (plp->current_track < playlist_count(plp->playlist)) {
              track_t* t = playlist_get(plp->playlist, plp->current_track);
              pthread_mutex_unlock(plp->mutex);
              load_and_play(plp, t);
            } else {
              pthread_mutex_unlock(plp->mutex);
            }
          }
        } else {
          pthread_mutex_unlock(plp->mutex);
        }
      }
      break;
      case PLP_CMD_PAUSE: {
        pthread_mutex_lock(plp->mutex);
        if (plp->player_state == PLAYLIST_PLAYER_PLAYING) {
          plp->player_state = PLAYLIST_PLAYER_PAUSED;
          pthread_mutex_unlock(plp->mutex);
          media_pause(plp->worker);
        } else {
          pthread_mutex_unlock(plp->mutex);
        }
      }
      break;
      case PLP_CMD_SET_VOLUME: {
        double volume_percentage = *((double*) data);
        media_set_volume(plp->worker, volume_percentage);
      }
      break;
      case PLP_CMD_SEEK: {
        long seek = *((long*) data);
        pthread_mutex_lock(plp->mutex);
        int nr = plp->current_track;
        pthread_mutex_unlock(plp->mutex);
        track_t* t = playlist_get(plp->playlist, nr);
        long begin_offset = track_get_begin_offset_in_ms(t);
        long end_offset = track_get_end_offset_in_ms(t);
        long len = track_get_length_in_ms(t);
        if (begin_offset >= 0) {
          seek = begin_offset + seek;
          if (end_offset >= 0) {
            if (seek >= end_offset) { seek = end_offset; }
          } else {
            if (seek >= len) { seek = len; }
          }
        } 
        log_debug2("seeking to %ld\n",seek);
        mc_free(data);
        media_seek(plp->worker, seek);
      }
      break;
      case PLP_CMD_NEXT: {
        pthread_mutex_lock(plp->mutex);
        if (plp->player_state == PLAYLIST_PLAYER_PLAYING ||
              plp->player_state == PLAYLIST_PLAYER_PAUSED) {
          pthread_mutex_unlock(plp->mutex);
          proces_next(plp, el_true);
        } else {
          pthread_mutex_unlock(plp->mutex);
        }
      }
      break;
      case PLP_CMD_PREVIOUS: {
        pthread_mutex_lock(plp->mutex);
        if (plp->player_state == PLAYLIST_PLAYER_PLAYING ||
              plp->player_state == PLAYLIST_PLAYER_PAUSED) {
          if (plp->current_track == 0) {
            pthread_mutex_unlock(plp->mutex);
          } else {
            int prev_index = plp->current_track - 1;
            track_t* t_current = playlist_get(plp->playlist, plp->current_track);
            track_t* t_prev = playlist_get(plp->playlist, prev_index);
            plp->current_track = prev_index;
            plp->track_changed = el_true;
            pthread_mutex_unlock(plp->mutex);
            const char* c_fu = (track_get_is_file(t_current)) ? track_get_file(t_current) 
                                                              : track_get_url(t_current);
            const char* p_fu = (track_get_is_file(t_prev)) ? track_get_file(t_prev)
                                                           : track_get_url(t_prev);
           if (strcmp(c_fu, p_fu) == 0) {
             seek_and_guard(plp, t_prev);
           } else {
             load_and_play(plp, t_prev);
           }
          }
        } else {
          pthread_mutex_unlock(plp->mutex);
        }
      }
      break;
      case PLP_CMD_RESTART_TRACK: {
        pthread_mutex_lock(plp->mutex);
        if (plp->player_state == PLAYLIST_PLAYER_PLAYING ||
              plp->player_state == PLAYLIST_PLAYER_PAUSED) {
          track_t* t_current = playlist_get(plp->playlist, plp->current_track);
          pthread_mutex_unlock(plp->mutex);
          seek_and_guard(plp, t_current);
        } else {
          pthread_mutex_unlock(plp->mutex);
        }
      }
      break;
      case PLP_CMD_SET_TRACK: {
        log_debug("settrack");
        pthread_mutex_lock(plp->mutex);
        log_debug("settrack");
        if (plp->player_state == PLAYLIST_PLAYER_PLAYING ||
              plp->player_state == PLAYLIST_PLAYER_PAUSED) {
          int nr = *((int*) data);
          mc_free(data);
          track_t* t_current = NULL;
          if (nr < playlist_count(plp->playlist) && nr >=0) {
            plp->current_track = nr;
            t_current = playlist_get(plp->playlist, plp->current_track);
          }
          pthread_mutex_unlock(plp->mutex);
          printf("loading track %d\n", nr);
          if (t_current) load_and_play(plp, t_current);
        } else {
          int nr = *((int*) data);
          mc_free(data);
          if (nr < playlist_count(plp->playlist) && nr >=0) {
            plp->current_track = nr;
          }
          pthread_mutex_unlock(plp->mutex);
          post_command(plp, PLP_CMD_PLAY, NULL);
        }
      }
      break;
      case PLP_CMD_NO_REPEAT: {
        pthread_mutex_lock(plp->mutex);
        plp->repeat = PLP_NO_REPEAT;
        pthread_mutex_unlock(plp->mutex);
      }
      break;
      case PLP_CMD_TRACK_REPEAT: {
        pthread_mutex_lock(plp->mutex);
        plp->repeat = PLP_TRACK_REPEAT;
        pthread_mutex_unlock(plp->mutex);
      }
      break;
      case PLP_CMD_LIST_REPEAT: {
        pthread_mutex_lock(plp->mutex);
        plp->repeat = PLP_LIST_REPEAT;
        pthread_mutex_unlock(plp->mutex);
      }
      break;
      case PLP_CMD_SET_PLAYLIST: {
        pthread_mutex_lock(plp->mutex);
        plp->player_state = PLAYLIST_PLAYER_STOPPED;
        plp->current_track = -1;
        media_pause(plp->worker);
        playlist_destroy(plp->playlist);
        plp->playlist = (playlist_t*) data;
        pthread_mutex_unlock(plp->mutex);
      }
      break;
      default: 
      break;    
    }
    
    // Now check the worker state
    {
      audio_event_t *event = media_get_event_when_available(plp->worker, 200);
      if (event != NULL) {
        do {
          proces_media_event(plp, event);
          audio_event_destroy(event);
          if (media_peek_event(plp->worker)) {
            event = media_get_event(plp->worker);
          } else {
            event = NULL;
          }
        } while (event != NULL);
      }
    }
    
    
  }
  
  // destroy received.
  log_debug("destroy ");
  
  media_pause(plp->worker);

  return NULL;  
}


