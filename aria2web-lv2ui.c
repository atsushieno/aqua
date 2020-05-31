#include <spawn.h>
#include <vector>
#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/extensions/ui/ui.h>
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>
#define HTTPSERVER_IMPL
#define ARIA2WEB_IMPL
#include "lv2_external_ui.h"
#include "aria2web.h"

extern "C" {

// rewrite this part to whichever port your plugin is listening.
#ifndef ARIA2WEB_LV2_CONTROL_PORT
#define ARIA2WEB_LV2_CONTROL_PORT 0 //  0 is sfizz control-in port.
#endif

typedef struct aria2weblv2ui_tag {
#if IN_PROCESS_WEBVIEW
	aria2web* a2w;
#else
	pthread_t ui_launcher_thread;
#endif
	const char *plugin_uri;
	const char *bundle_path;
	LV2UI_Write_Function write_function;
	LV2UI_Controller controller;
	const LV2_Feature *const *features;
	int urid_atom, urid_frame_time, urid_midi_event, urid_atom_event_transfer;
	LV2_External_UI_Widget extui;

	char atom_buffer[50];
} aria2weblv2ui;

char* fill_atom_message_base(aria2weblv2ui* a, LV2_Atom_Sequence* seq)
{
	auto events = (LV2_Atom_Event*) ((char*) seq + sizeof (LV2_Atom_Sequence));
	seq->body.unit = a->urid_frame_time;
	auto ev = &events[0];
	auto msg = ((char*) ev + sizeof (LV2_Atom_Event));
	ev->time.frames = 0;
	ev->body.type = a->urid_midi_event;
	ev->body.size = 3;
	seq->atom.size = sizeof(LV2_Atom_Event) + 3;
	seq->atom.type = a->urid_atom_event_transfer;
	return msg;
}

// Unlike the name implies, it is actually not that general, but more specific to sfizz...
// See https://github.com/sfztools/sfizz/blob/a1fdfa2e/lv2/sfizz.c#L491 for MIDI message processing details.
void a2wlv2_cc_callback(void* context, int cc, int value)
{
	auto a = (aria2weblv2ui*) context;

	auto seq = (LV2_Atom_Sequence*) a->atom_buffer;
	char* msg = fill_atom_message_base(a, seq);
	msg[0] = 0xB0; // channel does not matter on sfizz
	msg[1] = (char) cc;
	msg[2] = (char) value;
	
	a->write_function(a->controller, ARIA2WEB_LV2_CONTROL_PORT, seq->atom.size + sizeof(LV2_Atom), a->urid_atom_event_transfer, a->atom_buffer);
}

// Unlike the name implies, it is actually not that general, but more specific to sfizz...
// See https://github.com/sfztools/sfizz/blob/a1fdfa2e/lv2/sfizz.c#L491 for MIDI message processing details.
void a2wlv2_note_callback(void* context, int key, int velocity)
{
	auto a = (aria2weblv2ui*) context;
	
	auto seq = (LV2_Atom_Sequence*) a->atom_buffer;
	char* msg = fill_atom_message_base(a, seq);
	msg[0] = velocity == 0 ? 0x80 : 0x90; // channel does not matter on sfizz
	msg[1] = (char) key;
	msg[2] = (char) velocity;
	
	a->write_function(a->controller, ARIA2WEB_LV2_CONTROL_PORT, seq->atom.size + sizeof(LV2_Atom), a->urid_atom_event_transfer, a->atom_buffer);
}

void extui_show_callback(LV2_External_UI_Widget* widget) {
	// FIXME: implement
}
void extui_hide_callback(LV2_External_UI_Widget* widget) {
	// FIXME: implement
}
void extui_run_callback(LV2_External_UI_Widget* widget) {
	// nothing to do here...
}

void* runloop_aria2web_host(void* context) {
	auto aui = (aria2weblv2ui*) context;

	std::string cmd = std::string{} + aui->bundle_path + "/aria2web-host";
	char *const args[3] = {cmd.c_str(), "--plugin", nullptr};
	char input[1024];

	int outPipes[2];
	int errPipes[2];
	posix_spawn_file_actions_t action;

	assert(!pipe(outPipes) && !pipe(errPipes));

	posix_spawn_file_actions_init(&action);
	posix_spawn_file_actions_addclose(&action, outPipes[0]);
	posix_spawn_file_actions_addclose(&action, errPipes[0]);
	posix_spawn_file_actions_adddup2(&action, outPipes[1], 1);
	posix_spawn_file_actions_adddup2(&action, errPipes[1], 2);

	posix_spawn_file_actions_addclose(&action, outPipes[1]);
	posix_spawn_file_actions_addclose(&action, errPipes[1]);

	pid_t pid;
	int spawn_ret = posix_spawnp(&pid, cmd.c_str(), &action, nullptr, (char* const*) &args, nullptr);
	if (pid == 0) {
		int x = POSIX_SPAWN_SETPGROUP;
		printf("Failed to launch %s: error code %d\n", cmd.c_str(), spawn_ret);
		return nullptr;
	}

	while(1) {
		int index = 0;
		input[1023] = '\0';
		while(1) {
			int size = read(outPipes[0], input + index, 1024 - index);
			if (index + size >= 1023)
				break;
			index += size;
			if (input[index] == '\n') {
				input[index + 1] = '\0';
				break;
			}
		}
		int note, cc, value;
		switch (input[0]) {
		case 'N':
			sscanf(input + 1, "#%x,#%x", &note, &value);
			a2wlv2_note_callback(aui, note, value);
			break;
		case 'C':
			sscanf(input, "#%x,#%x", &cc, &value);
			a2wlv2_cc_callback(aui, cc, value);
			break;
		case'Q':
			// terminate
			return nullptr;
		default:
			printf("Unrecognized command sent by aria2web-host: %s\n", input);
		}
	}

	return nullptr;
}

LV2UI_Handle aria2web_lv2ui_instantiate(
	const LV2UI_Descriptor *descriptor,
	const char *plugin_uri,
	const char *bundle_path,
	LV2UI_Write_Function write_function,
	LV2UI_Controller controller,
	LV2UI_Widget *widget,
	const LV2_Feature *const *features)
{
	auto ret = (aria2weblv2ui*) calloc(sizeof(aria2weblv2ui), 1);
	ret->plugin_uri = plugin_uri;
	// FIXME: do not alter bundle path. Adjust it in aria2web.h. (it does not even free memory now)
	ret->bundle_path = strdup((std::string{bundle_path} + "/resources").c_str());
	ret->write_function = write_function;
	ret->controller = controller;
	ret->features = features;

	auto extui = &ret->extui;
	extui->show = extui_show_callback;
	extui->hide = extui_hide_callback;
	extui->run = extui_run_callback;

	for (int i = 0; features[i]; i++) {
		auto f = features[i];
		if (strcmp(f->URI, LV2_URID__map) == 0) {
			auto urid = (LV2_URID_Map*) f->data;
			ret->urid_atom = urid->map(urid->handle, LV2_ATOM_URI);
			ret->urid_frame_time = urid->map(urid->handle, LV2_ATOM__frameTime);
			ret->urid_midi_event = urid->map(urid->handle, LV2_MIDI__MidiEvent);
			ret->urid_atom_event_transfer = urid->map(urid->handle, LV2_ATOM__eventTransfer);
			break;
		}
	}

#if IN_PROCESS_WEBVIEW
	ret->a2w = aria2web_create(bundle_path);
	aria2web_set_control_change_callback(ret->a2w, a2wlv2_cc_callback, ret);
	aria2web_set_note_callback(ret->a2w, a2wlv2_note_callback, ret);
	ret->a2w->web_local_file_path = ret->bundle_path;

	aria2web_start(ret->a2w, nullptr);
#else
	pthread_t thread;
	pthread_create(&thread, nullptr, runloop_aria2web_host, ret);
	pthread_setname_np(thread, "aria2web_lv2ui_host_launcher");
	ret->ui_launcher_thread = thread;
#endif

	*widget = &ret->extui;
	return ret;
}

void aria2web_lv2ui_cleanup(LV2UI_Handle ui)
{
#if IN_PROCESS_WEBVIEW
	auto a2wlv2 = (aria2weblv2ui*) ui;
	aria2web_stop(a2wlv2->a2w);
	free(a2wlv2);
#else
	assert(0); // implement!
#endif
}

void aria2web_lv2ui_port_event(LV2UI_Handle ui, uint32_t port_index, uint32_t buffer_size, uint32_t format, const void *buffer)
{
	if (port_index == ARIA2WEB_LV2_CONTROL_PORT) {
		// FIXME: reflect the value changes back to UI.
		printf("port event received. buffer size: %d bytes\n", buffer_size);
	}
}


LV2UI_Descriptor uidesc;

LV2_SYMBOL_EXPORT const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index)
{
	uidesc.URI = "https://github.com/atsushieno/aria2web#ui";
	uidesc.instantiate = aria2web_lv2ui_instantiate;
	uidesc.cleanup = aria2web_lv2ui_cleanup;
	uidesc.port_event = aria2web_lv2ui_port_event;
	uidesc.extension_data = nullptr;

	return &uidesc;
}

} // extern "C"
