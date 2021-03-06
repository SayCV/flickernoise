/*
 * Flickernoise
 * Copyright (C) 2010, 2011, 2012 Sebastien Bourdeauducq
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <bsp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <assert.h>
#include <mtklib.h>

#include "../util.h"
#include "../input.h"
#include "../config.h"
#include "cp.h"
#include "messagebox.h"
#include "filedialog.h"
#include "performance.h"
#include "../renderer/framedescriptor.h"

#include "midi.h"

static void strmidi(char *buf, int n)
{
	int octave, note;

	octave = (n / 12) - 2;
	note = n % 12;

	switch(note) {
		case 0:
			*(buf++) = 'C';
			break;
		case 1:
			*(buf++) = 'C';
			*(buf++) = '#';
			break;
		case 2:
			*(buf++) = 'D';
			break;
		case 3:
			*(buf++) = 'D';
			*(buf++) = '#';
			break;
		case 4:
			*(buf++) = 'E';
			break;
		case 5:
			*(buf++) = 'F';
			break;
		case 6:
			*(buf++) = 'F';
			*(buf++) = '#';
			break;
		case 7:
			*(buf++) = 'G';
			break;
		case 8:
			*(buf++) = 'G';
			*(buf++) = '#';
			break;
		case 9:
			*(buf++) = 'A';
			break;
		case 10:
			*(buf++) = 'A';
			*(buf++) = '#';
			break;
		case 11:
			*(buf++) = 'B';
			break;
	}
	sprintf(buf, "%d", octave);
}

static int midistr(const char *str)
{
	int octave, note;
	char *c;
	int r;
	
	switch(*str) {
		case 'c':
		case 'C':
			note = 0;
			break;
		case 'd':
		case 'D':
			note = 2;
			break;
		case 'e':
		case 'E':
			note = 4;
			break;
		case 'f':
		case 'F':
			note = 5;
			break;
		case 'g':
		case 'G':
			note = 7;
			break;
		case 'a':
		case 'A':
			note = 9;
			break;
		case 'b':
		case 'B':
			note = 11;
			break;
		default:
			return -1;
	}
	str++;

	octave = 0;
	if(*str == '#') {
		note++;
		if(note == 12) {
			octave++;
			note = 0;
		}
		str++;
	} else if(*str == 'b') {
		note--;
		if(note == -1) {
			octave--;
			note = 11;
		}
		str++;
	}

	octave += strtol(str, &c, 0);
	if(*c != 0) return -1;

	r = 12*(octave + 2) + note;
	if(r < 0) r = 0;
	if(r > 127) r = 127;

	return r;
}

static int appid;
static struct filedialog *browse_dlg;

static int w_open;

static char midi_bindings[128][384];

static void load_config(void)
{
	char config_key[8];
	int i;
	const char *val;

	mtk_cmdf(appid, "e_channel.set(-text \"%d\")",
	    config_read_int("midi_channel", 0)+1);
	for(i=0;i<128;i++) {
		sprintf(config_key, "midi_%02x", i);
		val = config_read_string(config_key);
		if(val != NULL)
			strcpy(midi_bindings[i], val);
		else
			midi_bindings[i][0] = 0;
	}
}

static void set_config(void)
{
	char config_key[16];
	int i;

	for(i=0;i<128;i++) {
		sprintf(config_key, "midi_%02x", i);
		if(midi_bindings[i][0] != 0)
			config_write_string(config_key, midi_bindings[i]);
		else
			config_delete(config_key);
	}
	config_write_int("midi_channel", mtk_req_i(appid, "e_channel.text")-1);
}

static void update_list(void)
{
	char str[32768];
	char note[16];
	char *p;
	int i;

	str[0] = 0;
	p = str;
	for(i=0;i<128;i++) {
		if(midi_bindings[i][0] != 0) {
			strmidi(note, i);
			p += sprintf(p, "%s: %s\n", note, midi_bindings[i]);
		}
	}
	/* remove last \n */
	if(p != str) {
		p--;
		*p = 0;
	}
	mtk_cmdf(appid, "lst_existing.set(-text \"%s\" -selection 0)", str);
}

static void selchange_callback(mtk_event *e, void *arg)
{
	int sel;
	int count;
	int i;
	char note[16];

	sel = mtk_req_i(appid, "lst_existing.selection");
	count = 0;
	for(i=0;i<128;i++) {
		if(midi_bindings[i][0] != 0) {
			if(count == sel) {
				count++;
				break;
			}
			count++;
		}
	}
	if(count != 0) {
		strmidi(note, i);
		mtk_cmdf(appid, "e_note.set(-text \"%s\")", note);
		mtk_cmdf(appid, "e_filename.set(-text \"%s\")", midi_bindings[i]);
	}
}

static void note_event(int chan, int code)
{
	char note[16];
	
	chan++;
	
	strmidi(note, code);
	if(chan == mtk_req_i(appid, "e_channel.text")) {
		mtk_cmdf(appid, "e_note.set(-text \"%s\")", note);
		mtk_cmdf(appid, "l_lastctl.set(-text \"\e%d = %s\")",
		    chan, note);
	} else {
		mtk_cmdf(appid, "l_lastctl.set(-text \"\e(%d = %s)\")",
		    chan, note);
	}
}

static void controller_event(int chan, int controller, int value)
{
	chan++;
	if(chan == mtk_req_i(appid, "e_channel.text")) {
		mtk_cmdf(appid, "l_lastctl.set(-text \"\e%d.%d = %d\")",
		    chan, controller, value);
	} else {
		mtk_cmdf(appid, "l_lastctl.set(-text \"\e(%d.%d = %d)\")",
		    chan, controller, value);
	}
}

static void midi_event(mtk_event *e, int count)
{
	int i, chan;

	for(i=0;i<count;i++) {
		chan = (e[i].press.code & 0x0f0000) >> 16;
		switch(e[i].type) {
			case EVENT_TYPE_MIDI_NOTEON:
				note_event(chan, e[i].press.code & 0x7f);
				break;
			case EVENT_TYPE_MIDI_CONTROLLER:
				controller_event(chan,
				    (e[i].press.code & 0x7f00) >> 8,
				    e[i].press.code & 0x7f);
				break;
			case EVENT_TYPE_MIDI_PITCH:
				/* EVENT_TYPE_MIDI_PITCH */
				// TODO
				break;
		}
	}
}

static void close_window(void)
{
	mtk_cmd(appid, "w.close()");
	input_delete_callback(midi_event);
	w_open = 0;
	close_filedialog(browse_dlg);
}

static void ok_callback(mtk_event *e, void *arg)
{
	set_config();
	cp_notify_changed();
	close_window();
}

static void cancel_callback(mtk_event *e, void *arg)
{
	close_window();
}

static void browse_ok_callback(void *arg)
{
	char filename[384];

	get_filedialog_selection(browse_dlg, filename, sizeof(filename));
	mtk_cmdf(appid, "e_filename.set(-text \"%s\")", filename);
}

static void browse_callback(mtk_event *e, void *arg)
{
	open_filedialog(browse_dlg);
}

static void clear_callback(mtk_event *e, void *arg)
{
	mtk_cmd(appid, "e_filename.set(-text \"\")");
}

static void addupdate_callback(mtk_event *e, void *arg)
{
	char note[8];
	int notecode;
	char filename[384];

	mtk_req(appid, note, sizeof(note), "e_note.text");
	mtk_req(appid, filename, sizeof(filename), "e_filename.text");
	notecode = midistr(note);
	if(notecode < 0) {
		messagebox("Error", "Invalid note");
		return;
	}
	strcpy(midi_bindings[notecode], filename);
	update_list();
	mtk_cmd(appid, "e_note.set(-text \"\")");
	mtk_cmd(appid, "e_filename.set(-text \"\")");
}

static int cmpstringp(const void *p1, const void *p2)
{
	return strcmp(*(char * const *)p1, *(char * const *)p2);
}

static void autobuild(int sn, char *folder)
{
	DIR *d;
	struct dirent *entry;
	struct stat s;
	char fullname[384];
	char *c;
	char *files[384];
	int n_files;
	int max_files = 128 - sn;
	int i;
	
	d = opendir(folder);
	if(!d) {
		messagebox("Auto build failed", "Unable to open directory");
		return;
	}
	n_files = 0;
	while((entry = readdir(d))) {
		if(entry->d_name[0] == '.') continue;
		snprintf(fullname, sizeof(fullname), "%s/%s", folder, entry->d_name);
		lstat(fullname, &s);
		if(!S_ISDIR(s.st_mode)) {
			c = strrchr(entry->d_name, '.');
			if((c != NULL) && (strcmp(c, ".fnp") == 0)) {
				if(n_files < 384) {
					files[n_files] = strdup(entry->d_name);
					n_files++;
				}
			}
		}
	}
	closedir(d);
	qsort(files, n_files, sizeof(char *), cmpstringp);
	
	for(i=0;i<n_files;i++) {
		if(i < max_files)
			sprintf(midi_bindings[i+sn], "%s/%s", folder, files[i]);
		free(files[i]);
	}
}

static void autobuild_callback(mtk_event *e, void *arg)
{
	char note[8];
	int notecode;
	char filename[384];
	int i;

	mtk_req(appid, note, sizeof(note), "e_note.text");
	mtk_req(appid, filename, sizeof(filename), "e_filename.text");
	if(note[0] == 0x00)
		strcpy(note, "C2");
	if(filename[0] == 0x00)
		strcpy(filename, SIMPLE_PATCHES_FOLDER);
	notecode = midistr(note);
	if(notecode < 0) {
		messagebox("Auto build failed", "Invalid starting note");
		return;
	}
	i = strlen(filename);
	if(filename[i-1] == '/')
		filename[i-1] = 0x00;
	autobuild(notecode, filename);
	update_list();
}

void init_midi(void)
{
	appid = mtk_init_app("MIDI");

	mtk_cmd_seq(appid,
		"g = new Grid()",
		 
		"g_parameters = new Grid()",
		"l_channel = new Label(-text \"Patch switch channel (1-16):\")",
		"e_channel = new Entry()",
		"g_parameters.place(l_channel, -column 1 -row 1)",
		"g_parameters.place(e_channel, -column 2 -row 1)",
		"g.place(g_parameters, -column 1 -row 0)",

		"g_existing0 = new Grid()",
		"l_existing = new Label(-text \"Existing bindings\" -font \"title\")",
		"s_existing1 = new Separator(-vertical no)",
		"s_existing2 = new Separator(-vertical no)",
		"g_existing0.place(s_existing1, -column 1 -row 1)",
		"g_existing0.place(l_existing, -column 2 -row 1)",
		"g_existing0.place(s_existing2, -column 3 -row 1)",
		"lst_existing = new List()",
		"lst_existingf = new Frame(-content lst_existing -scrollx no -scrolly yes)",

		"g_addedit0 = new Grid()",
		"l_addedit = new Label(-text \"Add/edit\" -font \"title\")",
		"s_addedit1 = new Separator(-vertical no)",
		"s_addedit2 = new Separator(-vertical no)",
		"g_addedit0.place(s_addedit1, -column 1 -row 1)",
		"g_addedit0.place(l_addedit, -column 2 -row 1)",
		"g_addedit0.place(s_addedit2, -column 3 -row 1)",
		
		"g_addedit1 = new Grid()",
		"l_note = new Label(-text \"Note:\")",
		"e_note = new Entry()",
		"g_addedit1.place(l_note, -column 1 -row 1)",
		"g_addedit1.place(e_note, -column 2 -row 1)",
		"l_filename = new Label(-text \"Filename:\")",
		"e_filename = new Entry()",
		"b_filename = new Button(-text \"Browse\")",
		"b_filenameclear = new Button(-text \"Clear\")",
		"g_addedit1.place(l_filename, -column 1 -row 2)",
		"g_addedit1.place(e_filename, -column 2 -row 2)",
		"g_addedit1.place(b_filename, -column 3 -row 2)",
		"g_addedit1.place(b_filenameclear, -column 4 -row 2)",
		"g_addedit1.columnconfig(2, -size 135)",
		"g_addedit1.columnconfig(3, -size 0)",
		"g_addedit1.columnconfig(4, -size 0)",
		"b_addupdate = new Button(-text \"Add/update\")",
		"b_autobuild = new Button(-text \"Auto build\")",
		
		"g.place(g_existing0, -column 1 -row 1)",
		"g.place(lst_existingf, -column 1 -row 2)",
		"g.rowconfig(2, -size 130)",
		"g.place(g_addedit0, -column 1 -row 3)",
		"g.place(g_addedit1, -column 1 -row 4)",
		"g.place(b_addupdate, -column 1 -row 5)",
		"g.place(b_autobuild, -column 1 -row 6)",
		
		"g_lastctl = new Grid()",
		"l_lastctltxt = new Label(-text \"Latest active controller:\")",
		"l_lastctl = new Label()",
		"g_lastctl.place(l_lastctltxt, -column 1 -row 1)",
		"g_lastctl.place(l_lastctl, -column 2 -row 1)",
		"g.place(g_lastctl, -column 1 -row 7)",
		
		"g_btn = new Grid()",
		"b_ok = new Button(-text \"OK\")",
		"b_cancel = new Button(-text \"Cancel\")",
		"g_btn.columnconfig(1, -size 250)",
		"g_btn.place(b_ok, -column 2 -row 1)",
		"g_btn.place(b_cancel, -column 3 -row 1)",

		"g.rowconfig(8, -size 10)",
		"g.place(g_btn, -column 1 -row 9)",

		"w = new Window(-content g -title \"MIDI settings\" -workx 10)",
		0);

	mtk_bind(appid, "lst_existing", "selchange", selchange_callback, NULL);
	mtk_bind(appid, "lst_existing", "selcommit", selchange_callback, NULL);
	mtk_bind(appid, "b_filename", "commit", browse_callback, NULL);
	mtk_bind(appid, "b_filenameclear", "commit", clear_callback, NULL);
	mtk_bind(appid, "b_addupdate", "commit", addupdate_callback, NULL);
	mtk_bind(appid, "b_autobuild", "commit", autobuild_callback, NULL);

	mtk_bind(appid, "b_ok", "commit", ok_callback, NULL);
	mtk_bind(appid, "b_cancel", "commit", cancel_callback, NULL);

	mtk_bind(appid, "w", "close", cancel_callback, NULL);

	browse_dlg = create_filedialog("Select MIDI patch", 0, "fnp", browse_ok_callback, NULL, NULL, NULL);
}

void open_midi_window(void)
{
	if(w_open) return;
	w_open = 1;
	load_config();
	update_list();
	mtk_cmd(appid, "w.open()");
	input_add_callback(midi_event);
}
